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

#include <cstdint>

namespace graphquery::database::storage
{
    class CLPGModel
    {
    public:
        CLPGModel() = default;
        virtual ~CLPGModel() = default;

        struct SProperty
        {
            int64_t id;
            char key[20] = {};
            char value[20] = {};
            SProperty* prev_version = nullptr;
        } __attribute__((packed));

        struct SEdge
        {
            int64_t id;
            int64_t property_id;
        } __attribute__((packed));

        struct SNode
        {
            int64_t id;
            int64_t property_id;
            std::vector<SEdge> neighbours;
        } __attribute__((packed));

    protected:
        virtual void add_node(const SProperty & properties) = 0;
        virtual void add_edge(int64_t src, int64_t dst, const SProperty & properties) = 0;
        virtual void delete_node(int64_t node_id) = 0;
        virtual void delete_edge(int64_t src, int64_t dst) = 0;
        virtual void update_node(int64_t node_id, const SProperty & properties) = 0;
        virtual void update_edge(int64_t edge_id, const SProperty & properties) = 0;

        virtual SNode get_node(int64_t node_id) = 0;
        virtual SNode get_edge(int64_t edge_id) = 0;
        virtual std::vector<SNode> get_nodes_by_label(int64_t label_id) = 0;
        virtual std::vector<SNode> get_edges_by_label(int64_t label_id) = 0;
    };
}
