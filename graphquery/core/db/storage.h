/************************************************************
* \author Ryan Skelton
* \date 18/09/2023
* \file storage.h
* \brief Storage header file providing an API class to access
*        graph underlying on-disk and in-memory datamodels.
************************************************************/

#pragma once

#include "./storage/loader.h"
#include "./storage/memory.h"

#include <memory>

namespace graphquery::database::storage
{
    class CStorage final
    {
    public:
        CStorage();
        ~CStorage();

    private:
//        std::unique_ptr<IMemory> m_graph_memory;
//        std::unique_ptr<ILoader> m_graph_loader;
    };
}
