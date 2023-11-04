#pragma once

#include "../../../graphstorage.h"

namespace graphquery::database::storage
{
    class CLoaderDirectedLabelGraph final : public IGraphDiskStorage
    {
    public:
        CLoaderDirectedLabelGraph();
        ~CLoaderDirectedLabelGraph() override;

        void Load() noexcept override;

    private:
        void CheckIfMainFileExists() noexcept;
    };
}
