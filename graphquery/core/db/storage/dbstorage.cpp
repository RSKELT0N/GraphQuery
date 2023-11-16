#include "dbstorage.h"

#include "db/system.h"

#include <cassert>
#include <sys/fcntl.h>
#include <sys/mman.h>

graphquery::database::storage::CDBStorage::~CDBStorage()
{
    if(m_existing_db_loaded)
        Close();
}


void
graphquery::database::storage::CDBStorage::Close() noexcept
{
    m_existing_db_loaded = false;
    m_db_superblock = {};
    m_db_graph_table.clear();
    m_db_disk.Close();
    _log_system->Info("Database file has been closed and memory has been flushed");
}

bool
graphquery::database::storage::CDBStorage::IsExistingDBLoaded() const noexcept
{
    return this->m_existing_db_loaded;
}

void
graphquery::database::storage::CDBStorage::Init(std::string_view file_path)
{
    if(m_existing_db_loaded)
        Close();

    if(!m_db_disk.CheckIfFileExists(file_path))
        SetUp(file_path);
    else Load(file_path);
}

void
graphquery::database::storage::CDBStorage::SetUp(std::string_view file_path)
{
    _log_system->Info(fmt::format("Initialising new database file: {}", file_path));
    m_db_disk.Create(file_path, MASTER_DB_FILE_SIZE);
    m_db_disk.Open(file_path, O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED);

    DefineDBSuperblock();
    DefineDBGraphTable();
    StoreDBSuperblock();
    StoreDBGraphTable();

    m_existing_db_loaded = true;
}

void
graphquery::database::storage::CDBStorage::DefineDBSuperblock() noexcept
{
    m_db_superblock = {};
    SDBInfo_t metadata = {};

    metadata.graph_entry_size = sizeof(SGraph_Entry_t);
    metadata.graph_table_size = sizeof(SGraph_Entry_t) * GRAPH_ENTRIES_AMT;
    metadata.graph_table_start_addr = DB_SUPERBLOCK_START_ADDR + sizeof(SDB_Superblock_t);

    m_db_superblock.version = 1;
    m_db_superblock.db_info = metadata;
    m_db_superblock.magic_check_sum = 0xF13D00;
    m_db_superblock.timestamp = std::time(nullptr);
}

void
graphquery::database::storage::CDBStorage::StoreDBSuperblock() noexcept
{
    m_db_disk.Seek(0);
    m_db_disk.Write(static_cast<void *>(&this->m_db_superblock), sizeof(SGraph_Entry_t), 1);
}

void
graphquery::database::storage::CDBStorage::DefineDBGraphTable() noexcept
{
    m_db_graph_table.clear();
}

void
graphquery::database::storage::CDBStorage::StoreDBGraphTable() noexcept
{
    assert(m_db_disk.CheckIfInitialised());

    const auto graph_entry_amt = m_db_superblock.db_info.graph_table_size / m_db_superblock.db_info.graph_entry_size;

    m_db_disk.Seek(static_cast<int64_t>(m_db_superblock.db_info.graph_table_start_addr));
    m_db_disk.Write(static_cast<void *>(&m_db_graph_table), m_db_superblock.db_info.graph_entry_size, graph_entry_amt);
}
void
graphquery::database::storage::CDBStorage::Load(std::string_view file_path)
{
    m_db_disk.Open(file_path, O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED);
    LoadDBSuperblock();
    LoadDBGraphTable();
    _log_system->Info(fmt::format("Database file ({}) has been loaded into memory", file_path));

    m_existing_db_loaded = true;
}

void
graphquery::database::storage::CDBStorage::LoadDBSuperblock() noexcept
{
    assert(m_db_disk.CheckIfInitialised() == true);

    m_db_disk.Seek(DB_SUPERBLOCK_START_ADDR);
    m_db_disk.Read(static_cast<void *>(&m_db_superblock), sizeof(SDB_Superblock_t), 1);
}

void
graphquery::database::storage::CDBStorage::LoadDBGraphTable() noexcept
{
    assert(m_db_disk.CheckIfInitialised() == true);

    const auto graph_entry_amt = m_db_superblock.db_info.graph_table_size / m_db_superblock.db_info.graph_entry_size;

    m_db_disk.Seek(static_cast<int64_t>(m_db_superblock.db_info.graph_table_start_addr));
    m_db_disk.Read(static_cast<void *>(&m_db_graph_table), m_db_superblock.db_info.graph_entry_size, graph_entry_amt);

}

const std::string graphquery::database::storage::CDBStorage::GetDBInfo() const noexcept
{
    assert(m_existing_db_loaded);

    return fmt::format("Version: {}\nDate Created: {}\nCheckSum: {}\nGraphs: {}\n",
                       m_db_superblock.version,
                       m_db_superblock.timestamp,
                       m_db_superblock.magic_check_sum,
                       m_db_superblock.db_info.graph_table_size / m_db_superblock.db_info.graph_entry_size);
}
