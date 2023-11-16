#pragma once

#include "db/storage/graphmodel.h"

namespace graphquery::database::storage
{
    class CGraphModelPropertyLabel final : public IGraphModel
    {
    public:
        CGraphModelPropertyLabel();
        ~CGraphModelPropertyLabel() override;

        void Load() noexcept override;

    private:
        void CheckIfMainFileExists() noexcept;
    };
}
