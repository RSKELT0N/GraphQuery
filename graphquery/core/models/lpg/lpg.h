/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file lpg.h
 * \brief Dervied instance of a memory model, supporting
 *        the labelled property graph functionality.
 *
 *        (under development)
 ************************************************************/

#pragma once

#include <vector>
#if defined(_WIN32) || defined(_WIN64)
#define LIB_EXPORT __declspec(dllexport)
#else
#define LIB_EXPORT
#endif

#include "db/storage/config.h"
#include "db/storage/diskdriver/diskdriver.h"
#include "db/storage/graph_model.h"

#include <optional>
#include <algorithm>
#include <map>

namespace graphquery::database::storage
{
    class CMemoryModelLPG final : public ILPGModel
    {
      private:
        struct SGraphMetaData
        {
            char graph_name[GRAPH_NAME_LENGTH]       = {};
            char graph_type[GRAPH_MODEL_TYPE_LENGTH] = {};
            int64_t vertices_c                       = {};
            int64_t edges_c                          = {};
            int16_t vertex_label_c                   = {};
            int16_t edge_label_c                     = {};
        };

        struct SEdgeLabelEntry
        {
            int64_t label_id_ref = {};
            int64_t item_c       = {};
        };

        struct SVertexContainer
        {
            SVertex vertex_metadata                        = {};
            std::vector<SEdgeLabelEntry> edge_labels       = {};
            std::vector<std::vector<SEdge>> labelled_edges = {};
        };

      public:
        CMemoryModelLPG();
        ~CMemoryModelLPG() override = default;

        void close() noexcept override;
        void save_graph() noexcept override;
        void delete_vertex(int64_t vertex_id) override;
        void delete_edge(int64_t src, int64_t dst) override;
        [[nodiscard]] std::string_view get_name() const noexcept override;
        void load_graph(std::filesystem::path path, std::string_view graph) noexcept override;
        void create_graph(std::filesystem::path path, std::string_view graph) noexcept override;
        void update_edge(int64_t edge_id, const std::initializer_list<std::pair<std::string, std::string>> & prop) override;
        void update_vertex(int64_t vertex_id, const std::initializer_list<std::pair<std::string, std::string>> & prop) override;
        void add_vertex(std::string_view label, const std::initializer_list<std::pair<std::string, std::string>> & prop) override;
        void add_edge(int64_t src, int64_t dst, std::string_view label, const std::initializer_list<std::pair<std::string, std::string>> & prop) override;

        const int64_t & get_num_edges() const override;
        SEdge get_edge(int64_t edge_id) override;
        const int64_t & get_num_vertices() const override;
        SVertex get_vertex(int64_t vertex_id) override;
        std::vector<SVertex> get_edges_by_label(int64_t label_id) override;
        std::vector<SVertex> get_vertices_by_label(int64_t label_id) override;

      private:
        friend class CTransaction;
        void add_vertex_entry(std::string_view label, const std::vector<std::pair<std::string, std::string>> & prop) noexcept;
        void add_edge_entry(int64_t src, int64_t dst, std::string_view label, const std::vector<std::pair<std::string, std::string>> & prop) noexcept;

        [[nodiscard]] std::optional<int64_t> check_if_edge_label_exists(std::string_view) const noexcept;
        [[nodiscard]] std::optional<int64_t> check_if_vertex_label_exists(std::string_view) const noexcept;

        [[nodiscard]] int64_t create_edge_label(std::string_view) noexcept;
        [[nodiscard]] int64_t create_vertex_label(std::string_view) noexcept;

        [[nodiscard]] std::vector<SVertexContainer>::iterator get_vertex_by_id(int64_t id) noexcept;
        [[nodiscard]] SLabel create_label(std::string_view label, int64_t label_id, int64_t item_c) const noexcept;

        void define_graph_header() noexcept;

        void store_graph_header() noexcept;
        void store_vertex_labels() noexcept;
        void store_edge_labels() noexcept;
        void store_labelled_vertices() noexcept;

        void read_graph_header() noexcept;
        void read_vertex_labels() noexcept;
        void read_edge_labels() noexcept;
        void read_labelled_vertices() noexcept;

        CDiskDriver m_master_file;
        CDiskDriver m_connections_file;
        SGraphMetaData m_graph_metadata = {};

        std::vector<SLabel> m_vertex_labels;
        std::vector<SLabel> m_edge_labels;
        std::map<int64_t, int64_t> m_vertex_map;

        std::vector<std::vector<SVertexContainer>> m_labelled_vertices;
        std::vector<std::vector<SProperty>> m_vertex_properties;
        std::vector<std::vector<SProperty>> m_edge_properties;

        std::string m_graph_name;
        bool m_flush_needed;
        static std::shared_ptr<logger::CLogSystem> m_log_system;
        static constexpr int16_t MASTER_FILE_SIZE           = KB(1);
        static constexpr int16_t CONNECTIONS_FILE_SIZE      = KB(1);
        static constexpr const char * MASTER_FILE_NAME      = "master";
        static constexpr const char * CONNECTIONS_FILE_NAME = "connections";

        static constexpr int64_t MASTER_METADATA_START_ADDR      = 0x00000000;
        static constexpr int64_t CONNECTIONS_START_ADDR          = 0x00000000;
        static constexpr int64_t MASTER_VERTEX_LABELS_START_ADDR = MASTER_METADATA_START_ADDR + sizeof(SGraphMetaData);
    };
} // namespace graphquery::database::storage
