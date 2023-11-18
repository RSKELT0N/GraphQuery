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
<<<<<<< HEAD
graphquery::database::storage::CGraphModelPropertyLabel::CreateGraph(std::string_view graph) noexcept
=======
graphquery::database::storage::CGraphModelPropertyLabel::Close() noexcept
>>>>>>> 4158259 (Add graph table.)
{

}

<<<<<<< HEAD
void
graphquery::database::storage::CGraphModelPropertyLabel::LoadGraph(std::string_view graph) noexcept
{

}

=======
extern "C"
{
    LIB_EXPORT void CreateGraphModel(std::unique_ptr<graphquery::database::storage::IGraphModel> & model)
    {
        model = std::make_unique<graphquery::database::storage::CGraphModelPropertyLabel>();
    }
}


>>>>>>> 4158259 (Add graph table.)
