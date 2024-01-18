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
        template<typename T>
        using LabelGroup = std::vector<T>;

        ILPGModel()           = default;
        ~ILPGModel() override = default;

        struct SLabel
        {
            char label_s[CFG_LPG_LABEL_LENGTH] = {};
            uint64_t item_c                    = {};
            uint16_t label_id                  = {};
        };

        struct SVertexEdgeLabelEntry
        {
            uint64_t item_c       = {};
            uint16_t label_id_ref = {};
            uint16_t pos          = {};
        };

        struct SEdgeContainer
        {
            typedef struct metadata_t
            {
                uint64_t dst      = {};
                uint16_t label_id = {};
            } metadata_t;

            metadata_t metadata     = {};
            uint64_t properties_ref = {};
        };

        struct SVertexContainer
        {
            typedef struct metadata_t
            {
                uint64_t id           = {};
                uint16_t label_id     = {};
                uint16_t edge_label_c = {};
                uint32_t neighbour_c  = {};
            } metadata_t;

            metadata_t metadata                                    = {};
            std::vector<SVertexEdgeLabelEntry> edge_labels         = {};
            std::vector<LabelGroup<SEdgeContainer>> labelled_edges = {};
            uint64_t properties_ref                                = {};
        };

        struct SProperty
        {
            char key[CFG_LPG_PROPERTY_KEY_LENGTH]     = {0};
            char value[CFG_LPG_PROPERTY_VALUE_LENGTH] = {0};

            SProperty() = default;

            SProperty(const std::string_view k, const std::string_view v)
            {
                strncpy(key, k.data(), CFG_LPG_PROPERTY_KEY_LENGTH);
                strncpy(value, v.data(), CFG_LPG_PROPERTY_VALUE_LENGTH);
            }
        };

        struct SPropertyContainer
        {
            uint64_t ref_id                   = {};
            uint16_t property_c               = {};
            std::vector<SProperty> properties = {};

            SPropertyContainer() = default;
            SPropertyContainer(const uint64_t _id, const std::vector<SProperty> & props): ref_id(_id), property_c(props.size()), properties(props) {}
        };

        [[nodiscard]] virtual uint64_t get_num_edges() const                                         = 0;
        [[nodiscard]] virtual uint64_t get_num_vertices() const                                      = 0;
        virtual std::optional<SVertexContainer> get_vertex(uint64_t vertex_id)                       = 0;
        virtual std::vector<SEdgeContainer> get_edge(uint64_t src, uint64_t dst)                     = 0;
        virtual std::optional<SEdgeContainer> get_edge(uint64_t src, uint64_t dst, std::string_view) = 0;
        virtual std::vector<SVertexContainer> get_vertices_by_label(std::string_view label_id)       = 0;
        virtual std::vector<SVertexContainer> get_edges_by_label(std::string_view label_id)          = 0;

        virtual void rm_vertex(uint64_t vertex_id)                                                                                                                   = 0;
        virtual void rm_edge(uint64_t src, uint64_t dst)                                                                                                             = 0;
        virtual void rm_edge(uint64_t src, uint64_t dst, std::string_view)                                                                                           = 0;
        virtual void update_edge(uint64_t edge_id, const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop)                                = 0;
        virtual void update_vertex(uint64_t vertex_id, const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop)                            = 0;
        virtual void add_vertex(uint64_t id, std::string_view label, const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop)              = 0;
        virtual void add_vertex(std::string_view label, const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop)                           = 0;
        virtual void add_edge(uint64_t src, uint64_t dst, std::string_view label, const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop) = 0;
    };
} // namespace graphquery::database::storage

extern "C"
{
    void create_graph_model(graphquery::database::storage::ILPGModel ** graph_model);
}
