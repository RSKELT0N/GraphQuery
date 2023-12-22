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
#include <utility>
#include <vector>

namespace graphquery::database::storage
{
    class CLPGModel
    {
      public:
        CLPGModel()          = default;
        virtual ~CLPGModel() = default;

        struct SProperty
        {
            SProperty * prev_version = nullptr;
            int16_t property_c       = {};
            std::vector<std::pair<std::string, std::string>> properties;
        };

        struct SEdge
        {
            int64_t id;
            int64_t property_id;
        } __attribute__((packed));

        struct SVertex
        {
            int64_t id;
            int16_t label_id;
            std::vector<SEdge> neighbours;
        };

        virtual void add_vertex(std::string_view label, SProperty & properties) = 0;
        virtual void add_edge(int64_t src, int64_t dst, SProperty & properties) = 0;
        virtual void delete_vertex(int64_t vertex_id)                           = 0;
        virtual void delete_edge(int64_t src, int64_t dst)                      = 0;
        virtual void update_vertex(int64_t vertex_id, SProperty & properties)   = 0;
        virtual void update_edge(int64_t edge_id, SProperty & properties)       = 0;

        virtual int64_t get_num_edges()                        = 0;
        virtual int64_t get_num_vertices()                        = 0;
        virtual SVertex get_vertex(int64_t vertex_id)                        = 0;
        virtual SVertex get_edge(int64_t edge_id)                            = 0;
        virtual std::vector<SVertex> get_vertices_by_label(int64_t label_id) = 0;
        virtual std::vector<SVertex> get_edges_by_label(int64_t label_id)    = 0;
    };
} // namespace graphquery::database::storage
