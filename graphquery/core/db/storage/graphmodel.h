/************************************************************
* \author Ryan Skelton
* \date 18/09/2023
* \file graphmodel.h
* \brief Header of the Graph data model API for shared libraries
*        to include to define an appropiate implementation.
*        Such as the Labelled directed graph, will be instantiated
*        and linked at run-time to read a graph of this type.
************************************************************/

#pragma once

#include <string_view>
#include <unistd.h>

namespace graphquery::database::storage
{
    class IGraphModel
    {
    public:
        IGraphModel() = default;
        virtual ~IGraphModel() {};

        virtual void
        init(const std::string_view graph) final
        {
            if(access(graph.cbegin(), F_OK) == -1)
                create_graph(graph);
            else load_graph(graph);
        }

        virtual void load_graph(std::string_view graph) noexcept = 0;
        virtual void create_graph(std::string_view graph) noexcept = 0;
        virtual void Close() noexcept = 0;

        friend class CDBStorage;
    };
}

extern "C"
{
    void create_graph_model(std::unique_ptr<graphquery::database::storage::IGraphModel> & graph_model);
}
