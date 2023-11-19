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
#include <sys/stat.h>

namespace graphquery::database::storage
{
    class IGraphModel
    {
    public:
        IGraphModel() = default;
        virtual ~IGraphModel() {};

        virtual void
        Init(const std::string_view graph) final
        {
            if(access(graph.cbegin(), F_OK) == -1)
                CreateGraph(graph);
            else LoadGraph(graph);
        }

        virtual void LoadGraph(std::string_view graph) noexcept = 0;
        virtual void CreateGraph(std::string_view graph) noexcept = 0;
        virtual void Close() noexcept = 0;

        friend class CDBStorage;
    };
}

extern "C"
{
    void CreateGraphModel(std::unique_ptr<graphquery::database::storage::IGraphModel> & graph_model);
}
