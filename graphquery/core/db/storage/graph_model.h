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
            char label_s[LPG_LABEL_LENGTH];
            int64_t label_id = {};
            int64_t item_c   = {};
        } __attribute__((packed));

        struct SProperty
        {
            char key[20]             = {};
            char value[20]           = {};
            SProperty * prev_version = nullptr;
        } __attribute__((packed));

        struct SEdge
        {
            int64_t id       = {};
            int64_t label_id = {};
            int64_t property_c = {};
        } __attribute__((packed));

        struct SVertex
        {
            int64_t id          = {};
            int64_t label_id    = {};
            int64_t neighbour_c = {};
            int64_t edge_label_c     = {};
            int64_t property_c = {};
        };

        virtual void add_vertex(std::string_view label, const std::pair<std::string, std::string> & prop...) = 0;
        virtual void add_edge(int64_t src, int64_t dst, const std::pair<std::string, std::string> & prop...) = 0;
        virtual void delete_vertex(int64_t vertex_id)                                                        = 0;
        virtual void delete_edge(int64_t src, int64_t dst)                                                   = 0;
        virtual void update_vertex(int64_t vertex_id, const std::pair<std::string, std::string> & prop...)   = 0;
        virtual void update_edge(int64_t edge_id, const std::pair<std::string, std::string> & prop...)       = 0;

        virtual int64_t get_num_edges()                                      = 0;
        virtual int64_t get_num_vertices()                                   = 0;
        virtual SVertex get_vertex(int64_t vertex_id)                        = 0;
        virtual SEdge get_edge(int64_t edge_id)                              = 0;
        virtual std::vector<SVertex> get_vertices_by_label(int64_t label_id) = 0;
        virtual std::vector<SVertex> get_edges_by_label(int64_t label_id)    = 0;
    };
} // namespace graphquery::database::storage

extern "C"
{
    void create_graph_model(std::shared_ptr<graphquery::database::storage::ILPGModel> & graph_model);
}
