/************************************************************
* \author Ryan Skelton
* \date 18/09/2023
* \file memory_model.h
* \brief Header of the Graph data model API for shared libraries
*        to include to define an appropiate implementation.
*        Such as the Labelled directed graph, will be instantiated
*        and linked at run-time to read a graph of this type.
*
*        (under development)
************************************************************/

#pragma once

#include <string_view>
#include <unistd.h>
#include <memory>

namespace graphquery::database::storage
{
    class IMemoryModel
    {
    public:
        IMemoryModel() = default;
        virtual ~IMemoryModel() {};

        virtual void
        init(const std::string_view graph) final
        {
            if(access(graph.cbegin(), F_OK) == -1)
                create_graph(graph);
            else load_graph(graph);
        }

        virtual void load_graph(std::string_view graph) noexcept = 0;
        virtual void create_graph(std::string_view graph) noexcept = 0;
        virtual void close() noexcept = 0;

        friend class CDBStorage;
    };
}

extern "C"
{
    void create_memory_model(std::unique_ptr<graphquery::database::storage::IMemoryModel> & memory_model);
}
