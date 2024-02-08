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
#include <functional>
#include <vector>
#include <optional>
#include <unordered_set>

namespace graphquery::database::storage
{
    class ILPGModel : public IMemoryModel
    {
      public:
        template<typename T>
        using LabelGroup = std::vector<T>;

        ILPGModel()           = default;
        ~ILPGModel() override = default;

        struct SNodeID
        {
            uint64_t id       = {};
            std::string label = {};

            SNodeID() = default;
            SNodeID(const uint64_t _id, const std::string_view _label): id(_id), label(_label) {}
        };

        struct SEdge_t
        {
            uint64_t src           = {};
            uint64_t dst           = {};
            uint32_t property_id   = {};
            uint16_t edge_label_id = {};
            uint16_t dst_label_id  = {};
            uint16_t property_c    = {};

            SEdge_t() = default;
            SEdge_t(const SEdge_t & cpy)
            {
                this->src           = cpy.src;
                this->dst           = cpy.dst;
                this->property_id   = cpy.property_id;
                this->edge_label_id = cpy.edge_label_id;
                this->dst_label_id  = cpy.dst_label_id;
                this->property_c    = cpy.property_c;
            }

            SEdge_t & operator=(const SEdge_t & cpy)
            {
                this->src           = cpy.src;
                this->dst           = cpy.dst;
                this->property_id   = cpy.property_id;
                this->edge_label_id = cpy.edge_label_id;
                this->dst_label_id  = cpy.dst_label_id;
                this->property_c    = cpy.property_c;
                return *this;
            }
        };

        struct SVertex_t
        {
            uint64_t id           = {};
            uint32_t neighbour_c  = {};
            uint32_t property_id  = {};
            uint16_t property_c   = {};
            uint16_t label_id     = {};
            uint16_t edge_label_c = {};

            SVertex_t() = default;
            SVertex_t(const SVertex_t & cpy)
            {
                this->id           = cpy.id;
                this->neighbour_c  = cpy.neighbour_c;
                this->property_id  = cpy.property_id;
                this->property_c   = cpy.property_c;
                this->label_id     = cpy.label_id;
                this->edge_label_c = cpy.edge_label_c;
            }

            SVertex_t & operator=(const SVertex_t & cpy)
            {
                this->id           = cpy.id;
                this->neighbour_c  = cpy.neighbour_c;
                this->property_id  = cpy.property_id;
                this->property_c   = cpy.property_c;
                this->label_id     = cpy.label_id;
                this->edge_label_c = cpy.edge_label_c;
                return *this;
            }
        };

        struct SProperty_t
        {
            char key[CFG_LPG_PROPERTY_KEY_LENGTH]     = {0};
            char value[CFG_LPG_PROPERTY_VALUE_LENGTH] = {0};

            SProperty_t() = default;

            SProperty_t(const std::string_view k, const std::string_view v)
            {
                strncpy(key, k.data(), CFG_LPG_PROPERTY_KEY_LENGTH);
                strncpy(value, v.data(), CFG_LPG_PROPERTY_VALUE_LENGTH);
            }
        };

        [[nodiscard]] virtual uint64_t get_num_edges()                                                        = 0;
        [[nodiscard]] virtual uint64_t get_num_vertices()                                                     = 0;
        virtual std::vector<SEdge_t> get_edges_by_label(std::string_view label_id)                            = 0;
        virtual std::vector<SProperty_t> get_properties_by_property_id(uint32_t id)                           = 0;
        virtual std::vector<SVertex_t> get_vertices_by_label(std::string_view label_id)                       = 0;
        virtual std::vector<SVertex_t> get_vertices(std::function<bool(const SVertex_t &)>)                   = 0;
        virtual std::optional<SVertex_t> get_vertex(const SNodeID & vertex_id)                                = 0;
        virtual std::vector<SProperty_t> get_properties_by_vertex(const SNodeID & id)                         = 0;
        virtual std::unordered_map<std::string, std::string> get_properties_by_property_id_map(uint32_t id)   = 0;
        virtual std::unordered_map<std::string, std::string> get_properties_by_vertex_map(const SNodeID & id) = 0;

        virtual std::vector<SEdge_t> get_edges(const SNodeID & src, const SNodeID & dst)                                                                                    = 0;
        virtual std::vector<SEdge_t> get_edges(std::function<bool(const SEdge_t &)>)                                                                                        = 0;
        virtual std::vector<SEdge_t> get_edges(uint64_t src, uint16_t src_label_id, std::function<bool(const SEdge_t &)>)                                                   = 0;
        virtual std::unordered_set<uint64_t> get_edge_dst_vertices(const SNodeID & src, std::function<bool(const SEdge_t &)>)                                               = 0;
        virtual std::optional<SEdge_t> get_edge(const SNodeID & src, const SNodeID & dst, std::string_view edge_label)                                                      = 0;
        virtual std::optional<SEdge_t> get_edge(const SNodeID & src, std::string_view edge_label, std::string_view vertex_label)                                            = 0;
        virtual std::vector<SEdge_t> get_edges(const SNodeID & src, std::string_view edge_label, std::string_view vertex_label)                                             = 0;
        virtual std::vector<SEdge_t> get_edges(std::string_view vertex_label, std::function<bool(const SEdge_t &)>)                                                         = 0;
        virtual std::vector<SEdge_t> get_edges(std::string_view vertex_label, std::string_view edge_label, std::function<bool(const SEdge_t &)>)                            = 0;
        virtual std::unordered_set<uint64_t> get_edge_dst_vertices(const SNodeID & src, std::string_view edge_label, std::string_view vertex_label)                         = 0;
        virtual std::vector<SEdge_t> get_recursive_edges(const SNodeID & src, std::initializer_list<std::pair<std::string_view, std::string_view>> edge_vertex_label_pairs) = 0;

        virtual void rm_vertex(const SNodeID & vertex_id)                                                                                                                               = 0;
        virtual void rm_edge(const SNodeID & src, const SNodeID & dst)                                                                                                                  = 0;
        virtual void rm_edge(const SNodeID & src, const SNodeID & dst, std::string_view edge_label)                                                                                     = 0;
        virtual void add_vertex(const SNodeID & id, const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop)                                                  = 0;
        virtual void add_vertex(std::string_view label, const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop)                                              = 0;
        virtual void add_edge(const SNodeID & src, const SNodeID & dst, std::string_view edge_label, const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop) = 0;
    };
} // namespace graphquery::database::storage

extern "C"
{
    void create_graph_model(graphquery::database::storage::ILPGModel ** graph_model);
}
