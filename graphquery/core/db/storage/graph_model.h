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

#include "config.h"
#include "memory_model.h"

#include <cstdint>
#include <vector>

namespace graphquery::database::storage
{
    class ILPGModel : public IMemoryModel
    {
      public:
        ILPGModel()          = default;
        virtual ~ILPGModel() = default;

        struct SLabel
        {
            char label_s[LPG_LABEL_LENGTH] = {};
            int64_t label_id               = {};
            int64_t item_c                 = {};
        };

        struct SProperty
        {
            char key[LPG_PROPERTY_KEY_LENGTH]     = {};
            char value[LPG_PROPERTY_VALUE_LENGTH] = {};
            SProperty * prev_version              = nullptr;
        };

        struct SEdge
        {
            int64_t id         = {};
            int64_t dst        = {};
            int64_t label_id   = {};
            int64_t property_c = {};
        };

        struct SVertex
        {
            int64_t id           = {};
            int64_t label_id     = {};
            int64_t neighbour_c  = {};
            int64_t edge_label_c = {};
            int64_t property_c   = {};
        };

        virtual void delete_vertex(int64_t vertex_id)                                                                                = 0;
        virtual void delete_edge(int64_t src, int64_t dst)                                                                           = 0;
        virtual void update_edge(int64_t edge_id, const std::initializer_list<std::pair<std::string, std::string>> & prop)                               = 0;
        virtual void update_vertex(int64_t vertex_id, const std::initializer_list<std::pair<std::string, std::string>> & prop)                           = 0;
        virtual void add_vertex(std::string_view label, const std::initializer_list<std::pair<std::string, std::string>> & prop)                         = 0;
        virtual void add_edge(int64_t src, int64_t dst, std::string_view label, const std::initializer_list<std::pair<std::string, std::string>> & prop) = 0;

        virtual int64_t get_num_edges() const                                = 0;
        virtual int64_t get_num_vertices() const                             = 0;
        virtual SEdge get_edge(int64_t edge_id)                              = 0;
        virtual SVertex get_vertex(int64_t vertex_id)                        = 0;
        virtual std::vector<SVertex> get_vertices_by_label(int64_t label_id) = 0;
        virtual std::vector<SVertex> get_edges_by_label(int64_t label_id)    = 0;
    };
} // namespace graphquery::database::storage

extern "C"
{
    void create_graph_model(std::shared_ptr<graphquery::database::storage::ILPGModel> & graph_model);
}
