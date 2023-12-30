#include "transaction.h"

#include <sys/mman.h>
#include <fcntl.h>

graphquery::database::storage::CTransaction::
CTransaction(const std::filesystem::path & local_path, CMemoryModelLPG * lpg): m_lpg(lpg), m_transaction_file(O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED)
{
    m_transaction_file.set_path(local_path);
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
    m_header_block.eof_addr      = sizeof(SHeaderBlock);
}

void
graphquery::database::storage::CTransaction::store_transaction_header()
{
    m_transaction_file.seek(TRANSACTION_HEADER_START_ADDR);
    m_transaction_file.write(&m_header_block, sizeof(SHeaderBlock), 1);
}

void
graphquery::database::storage::CTransaction::read_transaction_header()
{
    m_transaction_file.seek(TRANSACTION_HEADER_START_ADDR);
    m_transaction_file.read(&m_header_block, sizeof(SHeaderBlock), 1);
}

void
graphquery::database::storage::CTransaction::commit_vertex(std::string_view label, const std::vector<std::pair<std::string, std::string>> & props) noexcept
{
    m_transaction_file.seek(m_header_block.eof_addr);

    auto type = ETransactionType::vertex;
    m_transaction_file.write(&type, sizeof(type), 1);

    char lbl[LPG_LABEL_LENGTH] = {};
    strncpy(lbl, label.data(), LPG_LABEL_LENGTH);
    m_transaction_file.write(&lbl, sizeof(char), LPG_LABEL_LENGTH);

    const auto prop_c = static_cast<int64_t>(props.size());
    m_transaction_file.write(&prop_c, sizeof(int64_t), 1);

    for (const auto & [key, value] : props)
    {
        m_transaction_file.write(&key, sizeof(char), LPG_PROPERTY_KEY_LENGTH);
        m_transaction_file.write(&value, sizeof(char), LPG_PROPERTY_VALUE_LENGTH);
    }

    m_header_block.eof_addr = m_transaction_file.get_seek_offset();
    store_transaction_header();
}

void
graphquery::database::storage::CTransaction::commit_edge(int64_t src, int64_t dst, std::string_view label, const std::vector<std::pair<std::string, std::string>> & props) noexcept
{
    m_transaction_file.seek(m_header_block.eof_addr);

    auto type = ETransactionType::edge;
    m_transaction_file.write(&type, sizeof(type), 1);

    m_transaction_file.write(&src, sizeof(int64_t), 1);
    m_transaction_file.write(&dst, sizeof(int64_t), 1);

    char lbl[LPG_LABEL_LENGTH] = {};
    strncpy(lbl, label.data(), LPG_LABEL_LENGTH);
    m_transaction_file.write(&lbl, sizeof(char), LPG_LABEL_LENGTH);

    const auto prop_c = static_cast<int64_t>(props.size());
    m_transaction_file.write(&prop_c, sizeof(int64_t), 1);

    for (const auto & [key, value] : props)
    {
        m_transaction_file.write(&key, sizeof(char), LPG_PROPERTY_KEY_LENGTH);
        m_transaction_file.write(&value, sizeof(char), LPG_PROPERTY_VALUE_LENGTH);
    }

    m_header_block.eof_addr = m_transaction_file.get_seek_offset();
    store_transaction_header();
}

void
graphquery::database::storage::CTransaction::handle_transactions() noexcept
{
    m_transaction_file.seek(sizeof(SHeaderBlock));
    for (int i = 0; i < m_header_block.transaction_c; i++)
    {
        ETransactionType type;
        m_transaction_file.read(&type, sizeof(uint8_t), 1);

        if (type == ETransactionType::vertex)
        {
            SVertexTransaction transc = {};
            m_transaction_file.read(&transc, sizeof(SVertexTransaction), 1);

            std::vector<std::pair<std::string, std::string>> props = {};
            props.resize(transc.property_c);

            std::pair<std::string, std::string> prop = {};
            for(int j = 0; j < transc.property_c; j++)
            {
                m_transaction_file.read(&prop.first, sizeof(char), LPG_PROPERTY_KEY_LENGTH);
                m_transaction_file.read(&prop.second, sizeof(char), LPG_PROPERTY_VALUE_LENGTH);
                props.emplace_back(std::move(prop));
            }
            m_lpg->add_vertex_entry(transc.label, props);
        }
        else if (type == ETransactionType::edge)
        {
            SEdgeTransaction transc = {};
            m_transaction_file.read(&transc, sizeof(SEdgeTransaction), 1);

            std::vector<std::pair<std::string, std::string>> props = {};
            props.resize(transc.property_c);

            std::pair<std::string, std::string> prop = {};
            for(int j = 0; j < transc.property_c; j++)
            {
                m_transaction_file.read(&prop.first, sizeof(char), LPG_PROPERTY_KEY_LENGTH);
                m_transaction_file.read(&prop.second, sizeof(char), LPG_PROPERTY_VALUE_LENGTH);
                props.emplace_back(std::move(prop));
            }
            m_lpg->add_edge_entry(transc.src, transc.dst, transc.label, props);
        }
    }
    this->reset();
}
