#include "transaction.h"

#include <sys/mman.h>
#include <fcntl.h>

graphquery::database::storage::CTransaction::CTransaction(const std::filesystem::path & local_path, CMemoryModelVectorLPG * lpg):
    m_lpg(lpg), m_transaction_file(O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED)
{
    m_transaction_file.set_path(local_path);
}

void
graphquery::database::storage::CTransaction::close() noexcept
{
    m_transaction_file.close();
}

void
graphquery::database::storage::CTransaction::init() noexcept
{
    if (!CDiskDriver::check_if_file_exists((m_transaction_file.get_path() / TRANSACTION_FILE_NAME).string()))
        set_up();
    else
        load();
}

void
graphquery::database::storage::CTransaction::reset() noexcept
{
    m_header_block.transaction_c = 0;
    m_header_block.eof_addr      = sizeof(SHeaderBlock);
    m_transaction_file.resize(TRANSACTION_FILE_SIZE);
    (void) m_transaction_file.sync();
    store_transaction_header();
}

void
graphquery::database::storage::CTransaction::set_up()
{
    CDiskDriver::create_file(m_transaction_file.get_path(), TRANSACTION_FILE_NAME, TRANSACTION_FILE_SIZE);
    m_transaction_file.open(TRANSACTION_FILE_NAME);

    define_transaction_header();
}

void
graphquery::database::storage::CTransaction::load()
{
    m_transaction_file.open(TRANSACTION_FILE_NAME);

    read_transaction_header();
}

void
graphquery::database::storage::CTransaction::define_transaction_header()
{
    m_header_block               = {};
    m_header_block.transaction_c = 0;
    m_header_block.eof_addr      = TRANSACTIONS_START_ADDR;
}

void
graphquery::database::storage::CTransaction::read_transaction_header()
{
    m_transaction_file.seek(TRANSACTION_HEADER_START_ADDR);
    m_transaction_file.read(&m_header_block, sizeof(SHeaderBlock), 1);
}

void
graphquery::database::storage::CTransaction::commit_rm_vertex(const uint64_t id) noexcept
{
    m_transaction_file.seek(m_header_block.eof_addr);

    const SVertexTransaction to_write(id, 1);
    m_transaction_file.write(&to_write, sizeof(SVertexTransaction), 1);

    m_header_block.eof_addr = m_transaction_file.get_seek_offset();
    m_header_block.transaction_c++;
    store_transaction_header();
}

void
graphquery::database::storage::CTransaction::commit_rm_edge(const uint64_t src, const uint64_t dst, const std::string_view label) noexcept
{
    m_transaction_file.seek(m_header_block.eof_addr);

    const SEdgeTransaction to_write(src, dst, 1, label);
    m_transaction_file.write(&to_write, sizeof(SEdgeTransaction), 1);

    m_header_block.eof_addr = m_transaction_file.get_seek_offset();
    m_header_block.transaction_c++;
    store_transaction_header();
}

void
graphquery::database::storage::CTransaction::commit_vertex(const std::string_view label,
                                                           const std::vector<CMemoryModelVectorLPG::SProperty_t> & props,
                                                           const uint64_t optional_id) noexcept
{
    m_transaction_file.seek(m_header_block.eof_addr);

    const SVertexTransaction to_write(optional_id, 0, label, props.size());
    m_transaction_file.write(&to_write, sizeof(SVertexTransaction), 1);

    for (const auto & [key, value] : props)
    {
        m_transaction_file.write(&key[0], sizeof(char), CFG_LPG_PROPERTY_KEY_LENGTH);
        m_transaction_file.write(&value[0], sizeof(char), CFG_LPG_PROPERTY_VALUE_LENGTH);
    }

    m_header_block.eof_addr = m_transaction_file.get_seek_offset();
    m_header_block.transaction_c++;
    store_transaction_header();
}

void
graphquery::database::storage::CTransaction::commit_edge(const uint64_t src,
                                                         const uint64_t dst,
                                                         const std::string_view label,
                                                         const std::vector<CMemoryModelVectorLPG::SProperty_t> & props) noexcept
{
    m_transaction_file.seek(m_header_block.eof_addr);

    const SEdgeTransaction to_write(src, dst, 0, label, props.size());
    m_transaction_file.write(&to_write, sizeof(SEdgeTransaction), 1);

    for (const auto & [key, value] : props)
    {
        m_transaction_file.write(&key, sizeof(char), CFG_LPG_PROPERTY_KEY_LENGTH);
        m_transaction_file.write(&value, sizeof(char), CFG_LPG_PROPERTY_VALUE_LENGTH);
    }

    m_header_block.eof_addr = m_transaction_file.get_seek_offset();
    m_header_block.transaction_c++;
    store_transaction_header();
}

void
graphquery::database::storage::CTransaction::handle_transactions() noexcept
{
    m_transaction_file.seek(TRANSACTIONS_START_ADDR);
    auto v_transc = SVertexTransaction();
    auto e_transc = SEdgeTransaction();

    std::vector<CMemoryModelVectorLPG::SProperty_t> props = {};

    for (uint32_t i = 0; i < m_header_block.transaction_c; i++)
    {
        ETransactionType type;
        m_transaction_file.read(&type, sizeof(uint8_t), 1, false);

        switch (type)
        {
        case ETransactionType::vertex:
        {
            m_transaction_file.read(&v_transc, sizeof(SVertexTransaction), 1);

            if (v_transc.property_c > 0)
            {
                props.resize(v_transc.property_c);
                m_transaction_file.read(&props[0], CFG_LPG_PROPERTY_KEY_LENGTH + CFG_LPG_PROPERTY_VALUE_LENGTH, v_transc.property_c);
            }
            process_vertex_transaction(v_transc, props);
            break;
        }
        case ETransactionType::edge:
        {
            m_transaction_file.read(&e_transc, sizeof(SEdgeTransaction), 1);

            if (e_transc.property_c > 0)
            {
                props.resize(e_transc.property_c);
                m_transaction_file.read(&props[0], CFG_LPG_PROPERTY_KEY_LENGTH + CFG_LPG_PROPERTY_VALUE_LENGTH, e_transc.property_c);
            }

            process_edge_transaction(e_transc, props);
            break;
        }
        }
        v_transc = SVertexTransaction();
        e_transc = SEdgeTransaction();
        props.clear();
    }
}

void
graphquery::database::storage::CTransaction::process_vertex_transaction(const SVertexTransaction & transaction,
                                                                        const std::vector<CMemoryModelVectorLPG::SProperty_t> & props) const noexcept
{
    if (transaction.remove == 0)
        if (transaction.optional_id != ULONG_LONG_MAX)
            (void) m_lpg->add_vertex_entry(transaction.optional_id, transaction.label, props);
        else
            (void) m_lpg->add_vertex_entry(transaction.label, props);
    else
        (void) m_lpg->rm_vertex_entry(transaction.optional_id);
}

void
graphquery::database::storage::CTransaction::process_edge_transaction(const SEdgeTransaction & transaction,
                                                                      const std::vector<CMemoryModelVectorLPG::SProperty_t> & props) const noexcept
{
    if (transaction.remove == 0)
        (void) m_lpg->add_edge_entry(transaction.src, transaction.dst, transaction.label, props);
    else
        (void) m_lpg->rm_edge_entry(transaction.src, transaction.dst);
}
