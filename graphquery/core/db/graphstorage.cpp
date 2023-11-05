#include "graphstorage.h"
#include "system.h"

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
graphquery::database::storage::CGraphStorage::Initialise(std::string file_path) noexcept
{
    if(this->m_existing_db_loaded)
    {
        UnInitialise();
    }

    this->m_graph_loader->Load();
    this->m_graph_memory->Load();
}

void
graphquery::database::storage::CGraphStorage::UnInitialise() noexcept
{
    this->m_graph_loader.reset();
    this->m_graph_memory.reset();
}