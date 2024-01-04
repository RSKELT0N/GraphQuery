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

#if defined(_WIN32) || defined(_WIN64)
#define LIB_EXPORT __declspec(dllexport)
#else
#define LIB_EXPORT
#endif

#include <vector>
#include "db/storage/config.h"
#include "db/storage/diskdriver/diskdriver.h"
#include "db/storage/graph_model.h"

#include <optional>
#include <algorithm>
#include <map>
#include <unordered_map>

namespace graphquery::database::storage
{
    class CMemoryModelLPG final : public ILPGModel
    {
        template<typename T>
        using LabelGroup = std::vector<T>;

        enum class EActionState : int8_t
        {
            valid   = 0,
            invalid = -1
        };

        struct SGraphMetaData
        {
            char graph_name[CFG_GRAPH_NAME_LENGTH]       = {};
            char graph_type[CFG_GRAPH_MODEL_TYPE_LENGTH] = {};
            uint64_t vertices_c                          = {};
            uint64_t edges_c                             = {};
            uint16_t vertex_label_c                      = {};
            uint16_t edge_label_c                        = {};
        };

        struct SVertexEdgeLabelEntry
        {
            uint16_t label_id_ref = {};
            uint64_t item_c       = {};
            uint16_t pos          = {};
        };

        struct SVertexContainer
        {
            SVertex vertex_metadata                        = {};
            std::vector<SVertexEdgeLabelEntry> edge_labels = {};
            std::vector<LabelGroup<SEdge>> labelled_edges  = {};
        };

      public:
        CMemoryModelLPG();
        ~CMemoryModelLPG() override = default;

        void close() noexcept override;
        void relax(analytic::CRelax * relax) noexcept override;
        void save_graph() noexcept override;
        void rm_vertex(uint64_t vertex_id) override;
        void rm_edge(uint64_t src, uint64_t dst) override;
        void rm_edge(uint64_t src, uint64_t dst, std::string_view) override;

        [[nodiscard]] std::string_view get_name() const noexcept override;
        [[nodiscard]] inline const uint64_t & get_num_edges() const override;
        [[nodiscard]] inline const uint64_t & get_num_vertices() const override;
        [[nodiscard]] std::optional<SVertex> get_vertex(uint64_t vertex_id) override;
        [[nodiscard]] std::vector<SEdge> get_edge(uint64_t src, uint64_t dst) override;
        [[nodiscard]] std::optional<SEdge> get_edge(uint64_t src, uint64_t dst, std::string_view) override;
        [[nodiscard]] std::vector<SVertex> get_edges_by_label(std::string_view label_id) override;
        [[nodiscard]] std::vector<SVertex> get_vertices_by_label(std::string_view label_id) override;

        void load_graph(std::filesystem::path path, std::string_view graph) noexcept override;
        void create_graph(std::filesystem::path path, std::string_view graph) noexcept override;
        void update_edge(uint64_t edge_id, const std::initializer_list<std::pair<std::string, std::string>> & prop) override;
        void update_vertex(uint64_t vertex_id, const std::initializer_list<std::pair<std::string, std::string>> & prop) override;
        void add_vertex(std::string_view label, const std::initializer_list<std::pair<std::string, std::string>> & prop) override;
        void add_vertex(uint64_t id, std::string_view label, const std::initializer_list<std::pair<std::string, std::string>> & prop) override;
        void add_edge(uint64_t src, uint64_t dst, std::string_view label, const std::initializer_list<std::pair<std::string, std::string>> & prop) override;

      private:
        friend class CTransaction;
        [[nodiscard]] EActionState rm_vertex_entry(uint64_t vertex_id) noexcept;
        [[nodiscard]] EActionState rm_edge_entry(uint64_t src_vertex_id, uint64_t dst_vertex_id) noexcept;
        [[nodiscard]] EActionState rm_edge_entry(std::string_view, uint64_t src_vertex_id, uint64_t dst_vertex_id) noexcept;
        [[nodiscard]] EActionState add_vertex_entry(uint64_t id, std::string_view label, const std::vector<std::pair<std::string, std::string>> & prop) noexcept;
        [[nodiscard]] EActionState add_vertex_entry(std::string_view label, const std::vector<std::pair<std::string, std::string>> & prop) noexcept;
        [[nodiscard]] EActionState add_edge_entry(uint64_t src, uint64_t dst, std::string_view label, const std::vector<std::pair<std::string, std::string>> & prop) noexcept;

        [[nodiscard]] uint16_t create_edge_label(std::string_view) noexcept;
        [[nodiscard]] uint16_t create_vertex_label(std::string_view) noexcept;
        [[nodiscard]] uint16_t create_vertex_edge_label(SVertexContainer &, uint16_t) noexcept;
        [[nodiscard]] SLabel create_label(std::string_view, uint16_t, uint64_t) const noexcept;

        [[nodiscard]] inline std::optional<uint16_t> check_if_edge_label_exists(std::string_view) const noexcept;
        [[nodiscard]] inline std::optional<uint16_t> check_if_vertex_label_exists(std::string_view) const noexcept;
        [[nodiscard]] inline std::optional<uint16_t> check_if_vertex_edge_label_exists(const SVertexContainer &, uint16_t) const noexcept;

        [[nodiscard]] const SLabel & get_edge_label(std::string_view) noexcept;
        [[nodiscard]] const SLabel & get_vertex_label(std::string_view) noexcept;
        [[nodiscard]] const SVertexEdgeLabelEntry & get_vertex_edge_label(const SVertexContainer &, uint16_t) noexcept;

        void remove_marked_vertices() noexcept;
        void remove_marked_labels() noexcept;

        void define_graph_header() noexcept;
        void define_vertex_label_map() noexcept;
        void define_edge_label_map() noexcept;

        void store_graph_header() noexcept;
        void store_vertex_labels() noexcept;
        void store_edge_labels() noexcept;
        void store_labelled_vertices() noexcept;

        void read_graph_header() noexcept;
        void read_vertex_labels() noexcept;
        void read_edge_labels() noexcept;
        void read_labelled_vertices() noexcept;

        [[nodiscard]] uint64_t get_unassigned_vertex_id(size_t label_idx) const noexcept;
        [[nodiscard]] uint64_t get_unassigned_vertex_label_id() const noexcept;
        [[nodiscard]] uint64_t get_unassigned_edge_label_id() const noexcept;
        [[nodiscard]] std::optional<std::vector<SVertexContainer>::iterator> get_vertex_by_id(uint64_t id) noexcept;

        std::mutex m_update;
        bool m_flush_needed;
        std::string m_graph_name;
        std::shared_ptr<logger::CLogSystem> m_log_system;

        //~ Disk/file drivers for graph mapping from disk to memory
        CDiskDriver m_master_file;
        CDiskDriver m_connections_file;
        SGraphMetaData m_graph_metadata = {};

        //~ Graph data in-memory
        std::vector<SLabel> m_vertex_labels;
        std::vector<SLabel> m_edge_labels;
        LabelGroup<std::vector<SVertexContainer>> m_labelled_vertices;
        LabelGroup<std::vector<SProperty>> m_vertex_properties;
        LabelGroup<std::vector<SProperty>> m_edge_properties;

        //~ Indexing structures
        std::unordered_map<uint64_t, size_t> m_vertex_label_map;
        std::unordered_map<uint16_t, size_t> m_edge_label_map;
        LabelGroup<std::unordered_map<uint64_t, size_t>> m_vertex_map;

        std::vector<std::vector<SVertexContainer>::iterator> m_marked_vertices;
        std::vector<std::vector<SLabel>::iterator> m_marked_vertex_labels;
        std::vector<std::vector<SLabel>::iterator> m_marked_edge_labels;

        static constexpr const char * MASTER_FILE_NAME      = "master";
        static constexpr const char * CONNECTIONS_FILE_NAME = "connections";

        static constexpr int16_t MASTER_FILE_SIZE                = KB(1);
        static constexpr int16_t CONNECTIONS_FILE_SIZE           = KB(1);
        static constexpr uint64_t MASTER_START_ADDR              = 0x00000000;
        static constexpr uint64_t CONNECTIONS_START_ADDR         = 0x00000000;
        static constexpr int64_t MASTER_VERTEX_LABELS_START_ADDR = MASTER_START_ADDR + sizeof(SGraphMetaData);
    };
} // namespace graphquery::database::storage
