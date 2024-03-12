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
        explicit ILPGModel(const std::shared_ptr<graphquery::logger::CLogSystem> & log_system): m_log_system(log_system) {}

        ~ILPGModel() override = default;

        typedef int64_t SNodeID;

        struct SLabel
        {
            char label[CFG_LPG_LABEL_LENGTH] = {""};
        } __attribute__((packed));

        /****************************************************************
         * \struct SLabel_t
         * \brief Structure of a label type within the graph
         *
         * \param label_s char[]    - str of label name
         * \param item_c uint32_t   - count of the generic items under this label type
         * \param label_id uint16_t - unique identifier for relative label usage
         ***************************************************************/
        struct SLabel_t
        {
            char label_s[CFG_LPG_LABEL_LENGTH] = {};
            uint32_t item_c                    = {};
            uint16_t label_id                  = {};
        };

        struct SEdge_t
        {
            SNodeID src          = {};
            SNodeID dst          = {};
            uint32_t property_id = {};
            uint16_t property_c  = {};

            SEdge_t() = default;

            SEdge_t(const SEdge_t & cpy)
            {
                this->src         = cpy.src;
                this->dst         = cpy.dst;
                this->property_id = cpy.property_id;
                this->property_c  = cpy.property_c;
            }

            SEdge_t & operator=(const SEdge_t & cpy)
            {
                this->src         = cpy.src;
                this->dst         = cpy.dst;
                this->property_id = cpy.property_id;
                this->property_c  = cpy.property_c;
                return *this;
            }
        };

        struct SVertex_t
        {
            SNodeID id            = {};
            uint32_t outdegree    = {};
            uint32_t indegree     = {};
            uint32_t property_id  = {};
            uint32_t label_id     = {};
            uint16_t property_c   = {};
            uint16_t edge_label_c = {};

            SVertex_t() = default;

            SVertex_t(const SVertex_t & cpy)
            {
                this->id           = cpy.id;
                this->outdegree    = cpy.outdegree;
                this->indegree     = cpy.indegree;
                this->property_id  = cpy.property_id;
                this->property_c   = cpy.property_c;
                this->label_id     = cpy.label_id;
                this->edge_label_c = cpy.edge_label_c;
            }

            SVertex_t & operator=(const SVertex_t & cpy)
            {
                this->id           = cpy.id;
                this->outdegree    = cpy.outdegree;
                this->indegree     = cpy.indegree;
                this->property_id  = cpy.property_id;
                this->property_c   = cpy.property_c;
                this->label_id     = cpy.label_id;
                this->edge_label_c = cpy.edge_label_c;
                return *this;
            }
        };

        struct SProperty_t
        {
            char key[CFG_LPG_PROPERTY_KEY_LENGTH]     = {""};
            char value[CFG_LPG_PROPERTY_VALUE_LENGTH] = {""};

            SProperty_t() = default;

            SProperty_t(const std::string_view & k, const std::string_view & v)
            {
                strncpy(key, k.data(), CFG_LPG_PROPERTY_KEY_LENGTH - 1);
                strncpy(value, v.data(), CFG_LPG_PROPERTY_VALUE_LENGTH - 1);
            }
        };

        [[nodiscard]] virtual int64_t get_num_edges()                                   = 0;
        [[nodiscard]] virtual int64_t get_num_vertices()                                = 0;
        [[nodiscard]] virtual uint16_t get_num_vertex_labels()                          = 0;
        [[nodiscard]] virtual uint16_t get_num_edge_labels()                            = 0;
        [[nodiscard]] virtual double get_avg_outdegree()                                = 0;
        virtual std::optional<SVertex_t> get_vertex(SNodeID vertex_id)                  = 0;
        virtual std::vector<SEdge_t> get_edges_by_label(std::string_view label_id)      = 0;
        virtual std::vector<SVertex_t> get_vertices_by_label(std::string_view label_id) = 0;

        virtual std::vector<SProperty_t> get_properties_by_id(int64_t id)                                   = 0;
        virtual std::vector<SProperty_t> get_properties_by_property_id(uint32_t id)                         = 0;
        virtual std::vector<SProperty_t> get_properties_by_vertex(SNodeID id)                               = 0;
        virtual std::unordered_map<std::string, std::string> get_properties_by_id_map(int64_t id)           = 0;
        virtual std::unordered_map<std::string, std::string> get_properties_by_property_id_map(uint32_t id) = 0;
        virtual std::unordered_map<std::string, std::string> get_properties_by_vertex_map(SNodeID id)       = 0;

        virtual std::vector<SEdge_t> get_edges(SNodeID src, SNodeID dst)                                                                                       = 0;
        virtual std::vector<SEdge_t> get_edges(SNodeID src, std::string_view edge_label, std::string_view vertex_label)                                        = 0;
        virtual std::vector<SEdge_t> get_recursive_edges(SNodeID src, std::vector<SProperty_t> edge_vertex_label_pairs)                                        = 0;
        virtual std::unordered_set<int64_t> get_edge_dst_vertices(SNodeID src, std::string_view edge_label, std::string_view vertex_label)                     = 0;
        virtual std::vector<SEdge_t> get_edges(std::string_view vertex_label, std::string_view edge_label, const std::function<bool(const SEdge_t &)> &) = 0;
        virtual std::unordered_set<int64_t> get_edge_dst_vertices(SNodeID src, std::string_view edge_label, const std::function<bool(const SEdge_t &)> & pred) = 0;

        virtual std::optional<SEdge_t> get_edge(int64_t src_vertex_id, std::string_view edge_label, int64_t dst_vertex_id)     = 0;
        virtual std::vector<SEdge_t> get_edges(const std::function<bool(const SEdge_t &)> &)                                   = 0;
        virtual std::vector<SVertex_t> get_vertices(const std::function<bool(const SVertex_t &)> &)                            = 0;
        virtual std::vector<SEdge_t> get_edges(std::string_view vertex_label, const std::function<bool(const SEdge_t &)> &)    = 0;
        virtual std::unordered_set<int64_t> get_edge_dst_vertices(SNodeID src, const std::function<bool(const SEdge_t &)> &)   = 0;
        virtual std::vector<SEdge_t> get_edges(std::string_view vertex_label, std::string_view edge_label, SNodeID dst)        = 0;
        virtual std::vector<SEdge_t> get_edges(uint32_t vertex_id, std::string_view edge_label, std::string_view vertex_label) = 0;

        virtual void rm_vertex(SNodeID vertex_id)                                                                                                    = 0;
        virtual void rm_edge(SNodeID src, SNodeID dst)                                                                                               = 0;
        virtual void rm_edge(SNodeID src, SNodeID dst, std::string_view edge_label)                                                                  = 0;
        virtual void add_vertex(SNodeID id, const std::vector<std::string_view> & labels, const std::vector<SProperty_t> & prop)                     = 0;
        virtual void add_vertex(const std::vector<std::string_view> & labels, const std::vector<SProperty_t> & prop)                                 = 0;
        virtual void add_edge(SNodeID src, SNodeID dst, std::string_view edge_label, const std::vector<SProperty_t> & prop, bool undirected = false) = 0;

      protected:
        std::shared_ptr<graphquery::logger::CLogSystem> m_log_system;
    };
} // namespace graphquery::database::storage

extern "C"
{
    void create_graph_model(graphquery::database::storage::ILPGModel ** graph_model, const std::shared_ptr<graphquery::logger::CLogSystem> & log_system);
}