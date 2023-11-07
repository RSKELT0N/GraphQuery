#include "graphstorage.h"
#include "db/system.h"

#include <filesystem>

graphquery::database::storage::CGraphStorage::~CGraphStorage() = default;

graphquery::database::storage::CGraphStorage::CGraphStorage() : m_existing_db_loaded(false)
{
}

bool
graphquery::database::storage::CGraphStorage::IsExistingDBLoaded() const noexcept
{
    return this->m_existing_db_loaded;
}

void
graphquery::database::storage::CGraphStorage::Init(std::string_view file_path)
{
    _log_system->Info(fmt::format("Initialising new database file: {}", file_path));
    this->m_disk.Create(file_path, GRAPH_MASTER_FILE_SIZE);
    this->m_disk.Open(file_path, O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED);
}