#pragma once

#include "db/storage/memory.h"

namespace graphquery::database::storage
{
    class CCSR final : public IMemory
    {
        CCSR();
        ~CCSR();
    };
}