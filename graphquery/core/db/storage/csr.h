#pragma once

#include "graphstorage.h"

namespace graphquery::database::storage
{
    class CMemoryCSR final : public IGraphMemory
    {
    public:
        CMemoryCSR();
        ~CMemoryCSR() override = default;

        void Load() noexcept override;
    };
}