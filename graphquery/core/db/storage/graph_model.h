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
#include <optional>

namespace graphquery::database::storage
{
    class ILPGModel : public IMemoryModel
    {
      public:
        ILPGModel()          = default;
        ~ILPGModel() override = default;

        struct SLabel
        {
            char label_s[CFG_LPG_LABEL_LENGTH] = {};
            uint64_t item_c                    = {};
            uint16_t label_id                  = {};
        };

        struct SProperty
        {
            char key[CFG_LPG_PROPERTY_KEY_LENGTH]     = {};
            char value[CFG_LPG_PROPERTY_VALUE_LENGTH] = {};

            SProperty() = default;

            SProperty(const std::string_view k, const std::string_view v)
            {
                strncpy(key, k.data(), CFG_LPG_PROPERTY_KEY_LENGTH);
                strncpy(value, v.data(), CFG_LPG_PROPERTY_VALUE_LENGTH);
            }
        };

        struct SEdge
        {
            uint64_t dst        = {};
            uint16_t label_id   = {};
        };

        struct SVertex
        {
            uint64_t id           = {};
            uint32_t neighbour_c  = {};
            uint16_t label_id     = {};
            uint16_t edge_label_c = {};
        };

        [[nodiscard]] virtual uint64_t get_num_edges() const                                = 0;
        [[nodiscard]] virtual uint64_t get_num_vertices() const                             = 0;
        virtual std::vector<SEdge> get_edge(uint64_t src, uint64_t dst)                     = 0;
        virtual std::optional<SEdge> get_edge(uint64_t src, uint64_t dst, std::string_view) = 0;
        virtual std::optional<SVertex> get_vertex(uint64_t vertex_id)                       = 0;
        virtual std::vector<SVertex> get_vertices_by_label(std::string_view label_id)       = 0;
        virtual std::vector<SVertex> get_edges_by_label(std::string_view label_id)          = 0;

        virtual void rm_vertex(uint64_t vertex_id)                                                                                                         = 0;
        virtual void rm_edge(uint64_t src, uint64_t dst)                                                                                                   = 0;
        virtual void rm_edge(uint64_t src, uint64_t dst, std::string_view)                                                                                 = 0;
        virtual void update_edge(uint64_t edge_id, const std::initializer_list<std::pair<std::string, std::string>> & prop)                                = 0;
        virtual void update_vertex(uint64_t vertex_id, const std::initializer_list<std::pair<std::string, std::string>> & prop)                            = 0;
        virtual void add_vertex(uint64_t id, std::string_view label, const std::initializer_list<std::pair<std::string, std::string>> & prop)              = 0;
        virtual void add_vertex(std::string_view label, const std::initializer_list<std::pair<std::string, std::string>> & prop)                           = 0;
        virtual void add_edge(uint64_t src, uint64_t dst, std::string_view label, const std::initializer_list<std::pair<std::string, std::string>> & prop) = 0;
    };
} // namespace graphquery::database::storage

extern "C"
{
    void create_graph_model(std::shared_ptr<graphquery::database::storage::ILPGModel> & graph_model);
}
