/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file lpg_mmap.h
 * \brief Dervied instance of a memory model, supporting
 *        the labelled property graph functionality, with
 *        directly accessing memory map for graph writes/reads.
 *
 *        (under development)
 ************************************************************/

#pragma once

#if defined(_WIN32) || defined(_WIN64)
#define LIB_EXPORT __declspec(dllexport)
#else
#define LIB_EXPORT
#endif

#include "db/storage/graph_model.h"

#include <utility>
#include <vector>
#include <optional>
#include <algorithm>
#include <unordered_map>
#include <semaphore>
#include <set>

namespace graphquery::database::storage
{
    class CMemoryModelMMAPLPG final : public ILPGModel
    {
      public:
        enum class EActionState_t : int8_t
        {
            valid   = 0, // ~ valid state return (no issues found during compute)
            invalid = -1 // ~ invalid state return (issues were found during compute)
        };

        enum EIndexValue_t : uint64_t
        {
            MARKED_INDEX      = 0xFFFFFFFF, // ~ data block marked for deletion (not considered)
            END_INDEX         = 0xFFFFFFF0, // ~ last of the linked data blocks
            UNALLOCATED_INDEX = 0xFFFFFF00  // ~ data block that has not been allocated
        };

      private:
        struct SGraphMetaData_t
        {
            char graph_name[CFG_GRAPH_NAME_LENGTH]       = {}; // ~ name of the currently loaded graph
            char graph_type[CFG_GRAPH_MODEL_TYPE_LENGTH] = {}; // ~ type of the graph (this)
            uint64_t vertices_c                          = {}; // ~ count of the vertices added to the graph
            uint64_t edges_c                             = {}; // ~ count of the edges added to the graph
            uint16_t vertex_label_c                      = {}; // ~ count of the vertex labels added to the graph
            uint16_t edge_label_c                        = {}; // ~ count of the edge labels added to the graph
        };

        struct SIndexMetadata_t
        {
            uint64_t index_c               = {}; // ~ amount of indices stored
            uint32_t index_size            = {}; // ~ size of one index
            uint64_t index_list_start_addr = {}; // ~ start addr of index list
        };

        struct SBlockFileMetadata
        {
            uint64_t data_block_c       = {}; // ~ amount of currently stored data blocks
            uint32_t data_block_size    = {}; // ~ size of one data block
            uint64_t data_blocks_offset = {}; // ~ start addr of data block entries
        };

        struct SLabel_t
        {
            char label_s[CFG_LPG_LABEL_LENGTH] = {}; // ~ str of label name
            uint64_t item_c                    = {}; // ~ count of the generic items under this label type
            uint16_t label_id                  = {}; // ~ unique identifier for relative label usage
        };

        struct SIndexEntry_t
        {
            uint64_t id     = {}; // ~ generic identifier for the payload
            uint64_t offset = {}; // ~ respective offset for the payload
            uint16_t label_id {}; // ~ generic label id for the index container
        };

        struct SEdgeEntry_t
        {
            SEdge_t metadata        = {};                // ~ metadata info of the edge
            uint64_t properties_idx = UNALLOCATED_INDEX; // ~ offset index for properties
        };

        struct SVertexEntry_t
        {
            SVertex_t metadata      = {};                // ~ metadata info the vertex
            uint64_t edge_idx       = UNALLOCATED_INDEX; // ~ tail edge offset
            uint64_t properties_idx = UNALLOCATED_INDEX; // ~ offset index for properties
        };

        template<typename T>
            requires std::is_trivially_copyable_v<T>
        struct SDataBlock_t
        {
            uint64_t idx  = {};                // ~ index of the data block
            uint64_t next = UNALLOCATED_INDEX; // ~ state of the next linked block.
            T payload     = {};                // ~ stored payload contained in data block
        };

      public:
        CMemoryModelMMAPLPG();
        ~CMemoryModelMMAPLPG() override = default;

        void close() noexcept override;

        void save_graph() noexcept override;
        void rm_vertex(uint64_t vertex_id) override;
        void rm_edge(uint64_t src, uint64_t dst) override;
        void calc_outdegree(std::shared_ptr<uint32_t[]>) noexcept override;
        void rm_edge(uint64_t src, uint64_t dst, std::string_view) override;
        void edgemap(const std::unique_ptr<analytic::IRelax> & relax) noexcept override;

        [[nodiscard]] inline uint64_t get_num_edges() override;
        [[nodiscard]] inline uint64_t get_num_vertices() override;
        [[nodiscard]] std::string_view get_name() noexcept override;
        [[nodiscard]] std::optional<SVertex_t> get_vertex(uint64_t vertex_id) override;
        [[nodiscard]] std::vector<SEdge_t> get_edges(uint64_t src, uint64_t dst) override;
        [[nodiscard]] std::vector<SVertex_t> get_edges_by_label(std::string_view label_id) override;
        [[nodiscard]] std::vector<SVertex_t> get_vertices_by_label(std::string_view label_id) override;
        [[nodiscard]] std::optional<SEdge_t> get_edge(uint64_t src, uint64_t dst, std::string_view edge_label) override;
        [[nodiscard]] std::vector<SEdge_t> get_edges(uint64_t src, std::string_view edge_label, std::string_view vertex_label) override;
        [[nodiscard]] std::vector<SEdge_t> get_edges(uint64_t src, std::initializer_list<std::pair<std::string_view, std::string_view>> edge_vertex_label_pairs) override;
        [[nodiscard]] std::optional<SPropertyContainer_t> get_vertex_properties(uint64_t id) override;

        void load_graph(std::filesystem::path path, std::string_view graph) noexcept override;
        void create_graph(std::filesystem::path path, std::string_view graph) noexcept override;
        void update_edge(uint64_t edge_id, const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop) override;
        void update_vertex(uint64_t vertex_id, const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop) override;
        void add_vertex(std::string_view label, const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop) override;
        void add_vertex(uint64_t id, std::string_view label, const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop) override;
        void add_edge(uint64_t src, uint64_t dst, std::string_view label, const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop) override;

      private:
        friend class CTransaction;
        [[nodiscard]] EActionState_t rm_vertex_entry(uint64_t vertex_id) noexcept;
        [[nodiscard]] EActionState_t rm_edge_entry(uint64_t src_vertex_id, uint64_t dst_vertex_id) noexcept;
        [[nodiscard]] EActionState_t rm_edge_entry(std::string_view, uint64_t src_vertex_id, uint64_t dst_vertex_id) noexcept;
        [[nodiscard]] EActionState_t add_vertex_entry(uint64_t id, std::string_view label, const std::vector<SProperty_t> & prop) noexcept;
        [[nodiscard]] EActionState_t add_vertex_entry(std::string_view label, const std::vector<SProperty_t> & prop) noexcept;
        [[nodiscard]] EActionState_t add_edge_entry(uint64_t src, uint64_t dst, std::string_view label, const std::vector<SProperty_t> & prop) noexcept;

        [[nodiscard]] uint16_t create_edge_label(std::string_view) noexcept;
        [[nodiscard]] uint16_t create_vertex_label(std::string_view) noexcept;

        [[nodiscard]] inline std::optional<uint16_t> check_if_edge_label_exists(std::string_view) noexcept;
        [[nodiscard]] inline std::optional<uint16_t> check_if_vertex_label_exists(std::string_view) noexcept;

        [[nodiscard]] const SLabel_t & get_edge_label(std::string_view) noexcept;
        [[nodiscard]] const SLabel_t & get_vertex_label(std::string_view) noexcept;

        [[nodiscard]] uint64_t get_unassigned_vertex_id(size_t label_idx) const noexcept;
        [[nodiscard]] uint64_t get_unassigned_vertex_label_id() const noexcept;
        [[nodiscard]] uint64_t get_unassigned_edge_label_id() const noexcept;
        [[nodiscard]] bool check_if_vertex_exists(uint64_t id) noexcept;
        [[nodiscard]] std::optional<SDataBlock_t<SVertexEntry_t> *> get_vertex_by_id(uint64_t id) noexcept;
        static std::vector<SProperty_t> transform_properties(const std::vector<std::pair<std::string_view, std::string_view>> &) noexcept;

        void inline access_preamble() noexcept;
        void store_graph_metadata() noexcept;
        void store_index_metadata() noexcept;
        void store_vertices_metadata() noexcept;
        void store_edges_metadata() noexcept;

        void store_index_entry(uint64_t id, uint16_t label_id, uint64_t vertex_offset) noexcept;

        void read_index_list() noexcept;
        SIndexEntry_t * read_index_entry(uint64_t offset) noexcept;
        SDataBlock_t<SVertexEntry_t> * read_vertex_entry(uint64_t offset) noexcept;
        inline SGraphMetaData_t * read_graph_metadata() noexcept;
        inline SBlockFileMetadata * read_vertices_metadata() noexcept;
        inline SBlockFileMetadata * read_edges_metadata() noexcept;
        inline SIndexMetadata_t * read_index_metadata() noexcept;

        bool m_flush_needed;
        std::string m_graph_name;
        std::binary_semaphore m_save_lock;
        std::shared_ptr<logger::CLogSystem> m_log_system;

        //~ Disk/file drivers for graph mapping from disk to memory
        CDiskDriver m_master_file;
        CDiskDriver m_vertices_file;
        CDiskDriver m_edges_file;
        CDiskDriver m_index_file;

        LabelGroup<std::unordered_map<uint64_t, int64_t>> m_vertex_lut;

        static constexpr const char * MASTER_FILE_NAME   = "master";
        static constexpr const char * VERTICES_FILE_NAME = "vertices";
        static constexpr const char * EDGES_FILE_NAME    = "edges";
        static constexpr const char * INDEX_FILE_NAME    = "index";

        static constexpr uint16_t DEFAULT_FILE_SIZE            = KB(1);
        static constexpr uint8_t VERTEX_LABELS_MAX_AMT         = 128;
        static constexpr uint8_t EDGE_LABELS_MAX_AMT           = 128;
        static constexpr uint64_t DEFAULT_FILE_START_ADDR      = 0x00000000;
        static constexpr uint64_t GRAPH_METADATA_START_ADDR    = 0x00000000;
        static constexpr uint64_t INDEX_METADATA_START_ADDR    = 0x00000000;
        static constexpr uint64_t VERTICES_METADATA_START_ADDR = 0x00000000;
        static constexpr uint64_t EDGES_METADATA_START_ADDR    = 0x00000000;

        static constexpr uint64_t VERTEX_LABELS_START_ADDR = GRAPH_METADATA_START_ADDR + sizeof(SGraphMetaData_t);
        static constexpr uint64_t EDGE_LABELS_START_ADDR   = GRAPH_METADATA_START_ADDR + sizeof(SGraphMetaData_t) + sizeof(SLabel_t) * VERTEX_LABELS_MAX_AMT;
        static constexpr uint64_t INDEX_LIST_START_ADDR    = sizeof(SIndexMetadata_t);
    };
} // namespace graphquery::database::storage
