#include "lpg.h"

graphquery::database::storage::CMemoryModelLPG::CMemoryModelLPG() : m_graph_head(O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED) {}

graphquery::database::storage::CMemoryModelLPG::~CMemoryModelLPG()
{
}

void
graphquery::database::storage::CMemoryModelLPG::create_graph(std::string_view graph) noexcept
{
}


void
graphquery::database::storage::CMemoryModelLPG::load_graph(std::string_view graph) noexcept
{

}

void
graphquery::database::storage::CMemoryModelLPG::close() noexcept
{

}

extern "C"
{
    LIB_EXPORT void create_graph_model(std::unique_ptr<graphquery::database::storage::IMemoryModel> & model)
    {
        model = std::make_unique<graphquery::database::storage::CMemoryModelLPG>();
    }
}
