#include "transaction.h"

#include <sys/mman.h>
#include <fcntl.h>

graphquery::database::storage::CTransaction::
CTransaction(const std::filesystem::path & local_path): m_path(local_path), m_transaction_file(O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED)
{
    m_transaction_file.set_path(m_path);
    m_transactions = std::vector<STransaction>();
}

void
graphquery::database::storage::CTransaction::init() noexcept
{
    if (!CDiskDriver::check_if_file_exists((m_path / TRANSACTION_FILE_NAME).string()))
        set_up();
    else
        load();

    m_transaction_file.open(TRANSACTION_FILE_NAME);
}

void
graphquery::database::storage::CTransaction::set_up()
{
    CDiskDriver::create_file(m_path, TRANSACTION_FILE_NAME, TRANSACTION_FILE_SIZE);
    m_transaction_file.open(TRANSACTION_FILE_NAME);

    define_transaction_header();
}

void
graphquery::database::storage::CTransaction::load()
{
    m_transaction_file.open(TRANSACTION_FILE_NAME);

    read_transaction_header();
    read_transactions();
}

void
graphquery::database::storage::CTransaction::define_transaction_header()
{
    m_header_block                  = {};
    m_header_block.transaction_size = sizeof(STransaction);
    m_header_block.transaction_c    = 0;
    m_header_block.action_size      = TRANSACTION_ACTION_SIZE;
    m_header_block.log_size         = TRANSACTION_LOG_SIZE;
}

void
graphquery::database::storage::CTransaction::store_transaction_header()
{
}

void
graphquery::database::storage::CTransaction::read_transaction_header()
{
}

void
graphquery::database::storage::CTransaction::read_transactions()
{
}
