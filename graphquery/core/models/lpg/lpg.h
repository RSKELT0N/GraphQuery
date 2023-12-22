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
#include "db/storage/memory_model.h"

namespace graphquery::database::storage
{
    class CMemoryModelLPG final : public IMemoryModel, public CLPGModel
    {
      private:
        struct SVertexLabel_t
        {
            char label_s[LPG_LABEL_LENGTH];
            int16_t label_id  = {};
            int64_t vertex_c  = {};
        } __attribute__((packed));

        struct SGraphMetaData_t
        {
            char graph_name[GRAPH_NAME_LENGTH]       = {};
            char graph_type[GRAPH_MODEL_TYPE_LENGTH] = {};
            int64_t vertices_c                       = {};
            int64_t edges_c                          = {};
            int16_t vertex_label_c                   = {};
            int16_t edge_label_c                     = {};
        } __attribute__((packed));

      public:
        CMemoryModelLPG();
        ~CMemoryModelLPG() override = default;

        void close() noexcept override;
        void delete_vertex(int64_t vertex_id) override;
        void delete_edge(int64_t src, int64_t dst) override;
        void update_edge(int64_t edge_id, SProperty & properties) override;
        void update_vertex(int64_t vertex_id, SProperty & properties) override;
        void add_vertex(std::string_view label, SProperty & properties) override;
        void add_edge(int64_t src, int64_t dst, SProperty & properties) override;
        void load_graph(std::filesystem::path path, std::string_view graph) noexcept override;
        void create_graph(std::filesystem::path path, std::string_view graph) noexcept override;

        int64_t get_num_edges() override;
        int64_t get_num_vertices() override;
        SVertex get_vertex(int64_t vertex_id) override;
        SVertex get_edge(int64_t edge_id) override;
        std::vector<SVertex> get_vertices_by_label(int64_t label_id) override;
        std::vector<SVertex> get_edges_by_label(int64_t label_id) override;

      private:
        void add_vertex_entry(SVertex & vertex, std::string_view label) noexcept;
        [[nodiscard]] int16_t get_vertex_label_if_exists(std::string_view) const noexcept;
        [[nodiscard]] SVertexLabel_t create_vertex_label(std::string_view label, int16_t label_id, int64_t vertex_c) const noexcept;

        void define_graph_header() noexcept;
        void store_vertex_labels() noexcept;
        void store_graph_header() noexcept;
        void read_graph_header() noexcept;
        void read_vertex_labels() noexcept;

        CDiskDriver m_master_file;
        CDiskDriver m_connections_file;
        SGraphMetaData_t m_graph_metadata = {};
        std::vector<SVertexLabel_t> m_vertex_labels;
        std::vector<std::vector<SVertex>> m_labelled_vertices;

        std::string m_graph_name;
        static constexpr int16_t MASTER_FILE_SIZE          = KB(1);
        static constexpr int16_t CONNECTIONS_FILE_SIZE     = KB(1);
        static constexpr const char * MASTER_FILE_NAME      = "master";
        static constexpr const char * CONNECTIONS_FILE_NAME = "connections";

        static constexpr int64_t GRAPH_METADATA_START_ADDR      = 0x00000000;
        static constexpr int64_t GRAPH_VERTEX_LABELS_START_ADDR = GRAPH_METADATA_START_ADDR + sizeof(SGraphMetaData_t);
    };
} // namespace graphquery::database::storage
