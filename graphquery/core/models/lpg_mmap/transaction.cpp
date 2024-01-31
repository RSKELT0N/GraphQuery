#include "transaction.h"

#include <sys/mman.h>
#include <fcntl.h>

graphquery::database::storage::CTransaction::CTransaction(const std::filesystem::path & local_path, CMemoryModelMMAPLPG * lpg):
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
graphquery::database::storage::CTransaction::reset() noexcept
{
    m_transaction_file.resize(INITIAL_TRANSACTION_FILE_SIZE);
    define_transaction_header();
    (void) m_transaction_file.sync();
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
graphquery::database::storage::CTransaction::set_up()
{
    CDiskDriver::create_file(m_transaction_file.get_path(), TRANSACTION_FILE_NAME, INITIAL_TRANSACTION_FILE_SIZE);
    m_transaction_file.open(TRANSACTION_FILE_NAME);
    define_transaction_header();
}

void
graphquery::database::storage::CTransaction::load()
{
    m_transaction_file.open(TRANSACTION_FILE_NAME);
}

void
graphquery::database::storage::CTransaction::define_transaction_header()
{
    auto header_ptr                     = read_transaction_header();
    header_ptr->transaction_c           = 0;
    header_ptr->transactions_start_addr = TRANSACTIONS_START_ADDR;
    header_ptr->eof_addr                = TRANSACTIONS_START_ADDR;
}

template<typename T>
graphquery::database::storage::SRef_t<T>
graphquery::database::storage::CTransaction::read_transaction(const uint64_t seek)
{
    return m_transaction_file.ref<T>(seek);
}

graphquery::database::storage::SRef_t<graphquery::database::storage::CTransaction::SHeaderBlock>
graphquery::database::storage::CTransaction::read_transaction_header()
{
    return m_transaction_file.ref<SHeaderBlock>(TRANSACTION_HEADER_START_ADDR);
}

void
graphquery::database::storage::CTransaction::commit_rm_vertex(const uint64_t id) noexcept
{
    const auto commit_addr = read_transaction_header()->eof_addr;
    read_transaction_header()->eof_addr += sizeof(SVertexTransaction);
    read_transaction_header()->transaction_c++;

    auto transaction_ptr = read_transaction<SVertexTransaction>(commit_addr);

    transaction_ptr->type               = ETransactionType::vertex;
    transaction_ptr->commit.optional_id = id;
    transaction_ptr->commit.remove      = 1;
    transaction_ptr->commit.property_c  = 0;
}

void
graphquery::database::storage::CTransaction::commit_rm_edge(const uint64_t src, const uint64_t dst, const std::string_view label) noexcept
{
    const auto commit_addr = read_transaction_header()->eof_addr;
    read_transaction_header()->eof_addr += sizeof(SEdgeTransaction);
    read_transaction_header()->transaction_c++;

    auto transaction_ptr = read_transaction<SEdgeTransaction>(commit_addr);

    transaction_ptr->type              = ETransactionType::edge;
    transaction_ptr->commit.src        = src;
    transaction_ptr->commit.dst        = dst;
    transaction_ptr->commit.remove     = 1;
    transaction_ptr->commit.property_c = 0;
    strncpy(&transaction_ptr->commit.label[0], label.data(), CFG_LPG_LABEL_LENGTH);
}

void
graphquery::database::storage::CTransaction::commit_vertex(const std::string_view label,
                                                           const std::vector<CMemoryModelMMAPLPG::SProperty_t> & props,
                                                           const uint64_t optional_id) noexcept
{
    const auto commit_addr = read_transaction_header()->eof_addr;
    read_transaction_header()->eof_addr += sizeof(SVertexTransaction) + (props.size() * sizeof(ILPGModel::SProperty_t));
    read_transaction_header()->transaction_c++;

    SRef_t<SVertexTransaction> transaction_ptr = read_transaction<SVertexTransaction>(commit_addr);

    transaction_ptr->type               = ETransactionType::vertex;
    transaction_ptr->commit.optional_id = optional_id;
    transaction_ptr->commit.remove      = 0;
    transaction_ptr->commit.property_c  = props.size();
    strncpy(&transaction_ptr->commit.label[0], label.data(), CFG_LPG_LABEL_LENGTH);

    transaction_ptr.~SRef_t<SVertexTransaction>(); //~ Remove reference to transaction file.

    uint64_t property_addr = commit_addr + sizeof(SVertexTransaction);
    for (const auto & [key, value] : props)
    {
        auto prop = m_transaction_file.ref<ILPGModel::SProperty_t>(property_addr);
        strncpy(&prop->key[0], key, CFG_LPG_PROPERTY_KEY_LENGTH);
        strncpy(&prop->value[0], value, CFG_LPG_PROPERTY_VALUE_LENGTH);
        property_addr += sizeof(ILPGModel::SProperty_t);
    }
}

void
graphquery::database::storage::CTransaction::commit_edge(const uint64_t src,
                                                         const uint64_t dst,
                                                         const std::string_view label,
                                                         const std::vector<CMemoryModelMMAPLPG::SProperty_t> & props) noexcept
{
    const auto commit_addr = read_transaction_header()->eof_addr;
    read_transaction_header()->eof_addr += sizeof(SEdgeTransaction) + (props.size() * sizeof(ILPGModel::SProperty_t));
    read_transaction_header()->transaction_c++;

    SRef_t<SEdgeTransaction> transaction_ptr = read_transaction<SEdgeTransaction>(commit_addr);

    transaction_ptr->type              = ETransactionType::edge;
    transaction_ptr->commit.src        = src;
    transaction_ptr->commit.dst        = dst;
    transaction_ptr->commit.remove     = 0;
    transaction_ptr->commit.property_c = props.size();
    strncpy(&transaction_ptr->commit.label[0], label.data(), CFG_LPG_LABEL_LENGTH);

    transaction_ptr.~SRef_t<SEdgeTransaction>(); //~ Remove reference to transaction file.

    uint64_t property_addr = commit_addr + sizeof(SVertexTransaction);
    for (const auto & [key, value] : props)
    {
        auto prop = m_transaction_file.ref<ILPGModel::SProperty_t>(property_addr);
        strncpy(&prop->key[0], key, CFG_LPG_PROPERTY_KEY_LENGTH);
        strncpy(&prop->value[0], value, CFG_LPG_PROPERTY_VALUE_LENGTH);
        property_addr += sizeof(ILPGModel::SProperty_t);
    }
}

void
graphquery::database::storage::CTransaction::handle_transactions() noexcept
{
    m_transaction_file.seek(TRANSACTIONS_START_ADDR);
    const uint64_t transaction_c = read_transaction_header()->transaction_c;

    SRef_t<ETransactionType> type;
    SRef_t<SVertexTransaction> v_transc = {};
    SRef_t<SEdgeTransaction> e_transc   = {};

    std::vector<CMemoryModelMMAPLPG::SProperty_t> props = {};

    for (uint64_t i = 0; i < transaction_c; i++)
    {
        type = m_transaction_file.ref_update<ETransactionType>();
        switch (*type)
        {
        case ETransactionType::vertex:
        {
            v_transc = m_transaction_file.ref_update<SVertexTransaction>();

            if (v_transc->commit.property_c > 0)
            {
                props.resize(v_transc->commit.property_c);
                m_transaction_file.read(&props[0], CFG_LPG_PROPERTY_KEY_LENGTH + CFG_LPG_PROPERTY_VALUE_LENGTH, v_transc->commit.property_c);
            }
            process_vertex_transaction(v_transc, props);
            break;
        }
        case ETransactionType::edge:
        {
            e_transc = m_transaction_file.ref_update<SEdgeTransaction>();

            if (e_transc->commit.property_c > 0)
            {
                props.resize(e_transc->commit.property_c);
                m_transaction_file.read(&props[0], CFG_LPG_PROPERTY_KEY_LENGTH + CFG_LPG_PROPERTY_VALUE_LENGTH, e_transc->commit.property_c);
            }

            process_edge_transaction(e_transc, props);
            break;
        }
        }
        props.clear();
        v_transc.~SRef_t();
        e_transc.~SRef_t();
    }
}

void
graphquery::database::storage::CTransaction::process_vertex_transaction(SRef_t<SVertexTransaction> & transaction,
                                                                        const std::vector<CMemoryModelMMAPLPG::SProperty_t> & props) const noexcept
{
    if (transaction->commit.remove == 0)
        if (transaction->commit.optional_id != ULONG_LONG_MAX)
            (void) m_lpg->add_vertex_entry(transaction->commit.optional_id, transaction->commit.label, props);
        else
            (void) m_lpg->add_vertex_entry(transaction->commit.label, props);
    else
        (void) m_lpg->rm_vertex_entry(transaction->commit.optional_id);
}

void
graphquery::database::storage::CTransaction::process_edge_transaction(SRef_t<SEdgeTransaction> & transaction,
                                                                      const std::vector<CMemoryModelMMAPLPG::SProperty_t> & props) const noexcept
{
    if (transaction->commit.remove == 0)
        (void) m_lpg->add_edge_entry(transaction->commit.src, transaction->commit.dst, transaction->commit.label, props);
    else
        (void) m_lpg->rm_edge_entry(transaction->commit.src, transaction->commit.dst);
}
