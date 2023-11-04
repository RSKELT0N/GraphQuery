#include "graphstorage.h"

graphquery::database::storage::CGraphStorage::~CGraphStorage() = default;

graphquery::database::storage::CGraphStorage::CGraphStorage() : m_existing_database_loaded(false)
{
}

void
graphquery::database::storage::CGraphStorage::Initialise(std::string file_path) noexcept
{
    if(this->m_existing_database_loaded)
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