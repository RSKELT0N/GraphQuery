#pragma once

#include "db/storage/dbstorage.h"

namespace graphquery::database::storage
{
    class CLoaderPropertyLabelGraph final : public IGraphDiskStorage
    {
    public:
        CLoaderPropertyLabelGraph();
        ~CLoaderPropertyLabelGraph() override;

        void Load() noexcept override;

    private:
        void CheckIfMainFileExists() noexcept;
    };
}
