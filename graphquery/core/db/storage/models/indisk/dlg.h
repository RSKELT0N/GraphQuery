#pragma once

#include "db/storage/loader.h"

namespace graphquery::database::storage
{
    class CDirectedLabelGraph final : public ILoader
    {
        CDirectedLabelGraph();
        ~CDirectedLabelGraph();
    };
}
