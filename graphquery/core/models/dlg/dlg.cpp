#include "dlg.h"

graphquery::database::storage::CGraphModelPropertyLabel::CGraphModelPropertyLabel()
{

}

graphquery::database::storage::CGraphModelPropertyLabel::~CGraphModelPropertyLabel()
{
}

void
graphquery::database::storage::CGraphModelPropertyLabel::CreateGraph(std::string_view graph) noexcept
{
}

void
graphquery::database::storage::CGraphModelPropertyLabel::LoadGraph(std::string_view graph) noexcept
{

}

void
graphquery::database::storage::CGraphModelPropertyLabel::Close() noexcept
{

}

extern "C"
{
    LIB_EXPORT void CreateGraphModel(std::unique_ptr<graphquery::database::storage::IGraphModel> & model)
    {
        model = std::make_unique<graphquery::database::storage::CGraphModelPropertyLabel>();
    }
}
