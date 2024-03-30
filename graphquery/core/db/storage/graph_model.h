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
        explicit ILPGModel(const std::shared_ptr<logger::CLogSystem> & log_system, const bool & sync_state_):
            _sync_state_(sync_state_), m_log_system(log_system)
        {
        }

        ~ILPGModel() override = default;

        struct SLabel
        {
            char label[CFG_LPG_LABEL_LENGTH] = {""};
        };

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
            Id_t src               = {};
            Id_t dst               = {};
            Id_t property_id       = {};
            uint16_t edge_label_id = {};
            uint16_t property_c    = {};

            SEdge_t() = default;

            SEdge_t(const SEdge_t & cpy)
            {
                this->src           = cpy.src;
                this->dst           = cpy.dst;
                this->property_id   = cpy.property_id;
                this->edge_label_id = cpy.edge_label_id;
                this->property_c    = cpy.property_c;
            }

            SEdge_t & operator=(const SEdge_t & cpy)
            {
                this->src           = cpy.src;
                this->dst           = cpy.dst;
                this->property_id   = cpy.property_id;
                this->edge_label_id = cpy.edge_label_id;
                this->property_c    = cpy.property_c;
                return *this;
            }
        };

        struct SVertex_t
        {
            Id_t id               = {};
            Id_t property_id      = {};
            Id_t label_id         = {};
            uint32_t outdegree    = {};
            uint32_t indegree     = {};
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
    	    char key[CFG_LPG_PROPERTY_KEY_LENGTH]     = {0};
    	    char value[CFG_LPG_PROPERTY_VALUE_LENGTH] = {0};

    	    SProperty_t() = default;

    	    SProperty_t(const std::string_view &k, const std::string_view &v)
    	    {
        	size_t keyLength = std::min(k.length(), static_cast<size_t>(CFG_LPG_PROPERTY_KEY_LENGTH - 1));
        	size_t valueLength = std::min(v.length(), static_cast<size_t>(CFG_LPG_PROPERTY_VALUE_LENGTH - 1));

        	std::copy_n(k.data(), keyLength, key);
        	key[keyLength] = '\0';

        	std::copy_n(v.data(), valueLength, value);
       		value[valueLength] = '\0';
    	    }
	};

        [[nodiscard]] virtual uint16_t get_num_vertex_labels() = 0;
        [[nodiscard]] virtual uint16_t get_num_edge_labels() = 0;
        virtual std::optional<SVertex_t> get_vertex(Id_t vertex_id) = 0;
        virtual std::vector<SEdge_t> get_edges_by_label(std::string_view label_id) = 0;
        virtual std::vector<SVertex_t> get_vertices_by_label(std::string_view label_id) = 0;

        virtual std::vector<SProperty_t> get_properties_by_id(int64_t id) = 0;
        virtual std::vector<SProperty_t> get_properties_by_property_id(uint32_t id) = 0;
        virtual std::vector<SProperty_t> get_properties_by_vertex(Id_t id) = 0;
        virtual std::unordered_map<std::string, std::string> get_properties_by_id_map(int64_t id) = 0;
        virtual std::unordered_map<std::string, std::string> get_properties_by_property_id_map(uint32_t id) = 0;
        virtual std::unordered_map<std::string, std::string> get_properties_by_vertex_map(Id_t id) = 0;

        virtual std::vector<SEdge_t> get_edges(Id_t src, Id_t dst) = 0;
        virtual std::vector<SEdge_t> get_edges(Id_t src, std::string_view edge_label, std::string_view vertex_label) = 0;
        virtual std::unordered_set<Id_t> get_edge_dst_vertices(Id_t src, std::string_view edge_label, std::string_view vertex_label) = 0;
        virtual std::vector<SEdge_t> get_recursive_edges(Id_t src, std::vector<SProperty_t> edge_vertex_label_pairs) = 0;

        virtual std::optional<SEdge_t> get_edge(Id_t src_vertex_id, std::string_view edge_label, int64_t dst_vertex_id) = 0;
        virtual std::vector<SEdge_t> get_edges(const std::function<bool(const SEdge_t &)> &) = 0;
        virtual std::vector<SVertex_t> get_vertices(const std::function<bool(const SVertex_t &)> &) = 0;
        virtual std::vector<SEdge_t> get_edges(std::string_view vertex_label, const std::function<bool(const SEdge_t &)> &) = 0;
        virtual std::unordered_set<Id_t> get_edge_dst_vertices(Id_t src, const std::function<bool(const SEdge_t &)> &) = 0;
        virtual std::vector<SEdge_t> get_edges(std::string_view vertex_label, std::string_view edge_label, const std::function<bool(const SEdge_t &)> &) = 0;
        virtual std::vector<SEdge_t> get_edges(std::string_view vertex_label, std::string_view edge_label) = 0;
        virtual std::vector<SEdge_t> get_edges(std::string_view vertex_label, std::string_view edge_label, std::string_view dst_vertex_label) = 0;
        virtual std::vector<SEdge_t> get_edges(std::string_view vertex_label, std::string_view edge_label, Id_t dst) = 0;
        virtual std::vector<SEdge_t> get_edges_by_offset(Id_t vertex_id, std::string_view edge_label, std::string_view vertex_label) = 0;

        virtual void rm_vertex(Id_t vertex_id) = 0;
        virtual void rm_edge(Id_t src, Id_t dst) = 0;
        virtual void rm_edge(Id_t src, Id_t dst, std::string_view edge_label) = 0;
        virtual void add_vertex(Id_t id, const std::vector<std::string_view> & labels, const std::vector<SProperty_t> & prop) = 0;
        virtual void add_vertex(const std::vector<std::string_view> & labels, const std::vector<SProperty_t> & prop) = 0;
        virtual void add_edge(Id_t src, Id_t dst, std::string_view edge_label, const std::vector<SProperty_t> & prop, bool undirected = false) = 0;

    protected:
        const bool & _sync_state_;
        std::shared_ptr<graphquery::logger::CLogSystem> m_log_system;
    };
} // namespace graphquery::database::storage

extern "C"
{
void create_graph_model(graphquery::database::storage::ILPGModel ** graph_model, const std::shared_ptr<graphquery::logger::CLogSystem> & log_system, const bool & _sync_state_);
}
