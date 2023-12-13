/************************************************************
* \author Ryan Skelton
* \date 18/09/2023
* \file graph_model.h
* \brief Interface describing the properties of the labelled
*        property graph for the model of the database. This
*        will be implemented by a memory model to support
*        its functionality and representation in memory.
*
*        (under development)
************************************************************/

#pragma once

namespace graphquery::database::storage
{
    class CLPGModel
    {
    public:
        CLPGModel() = default;
        virtual ~CLPGModel() = default;

        class CProperty
        {

        };

        void add_node(const CProperty & properties);
        void add_edge(int src, int dst, const CProperty & properties);
    };
}
