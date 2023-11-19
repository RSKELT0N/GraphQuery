#pragma once

#if defined(_WIN32) || defined(_WIN64)
#define LIB_EXPORT __declspec(dllexport)
#else
#define LIB_EXPORT
#endif

#include "db/storage/graphmodel.h"

namespace graphquery::database::storage
{
    class CGraphModelPropertyLabel : public IGraphModel
    {
    public:
        CGraphModelPropertyLabel();
        ~CGraphModelPropertyLabel() override;

        void LoadGraph(std::string_view graph) noexcept override;
        void CreateGraph(std::string_view graph) noexcept override;
        void Close() noexcept override;

    private:
        void CheckIfMainFileExists() noexcept;
    };
}
