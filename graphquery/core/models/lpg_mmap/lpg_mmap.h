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

#include "db/utils/thread_pool.hpp"
#include "db/storage/graph_model.h"
#include "block_file.hpp"
#include "transaction.h"

#include <utility>
#include <vector>
#include <optional>

#define DATABLOCK_EDGE_PAYLOAD_C     3
#define DATABLOCK_PROPERTY_PAYLOAD_C 3

namespace graphquery::database::storage
{
    class CMemoryModelMMAPLPG final : public ILPGModel
    {
      public:
        /****************************************************************
         * \enum EActionState_t
         * \brief Declares the possible return states for this class.
         *         Supplies a state wether correctness was attained or not.
         *
         *  \param valid   - valid state return (no issues found during compute)
         *  \param invalid - invalid state return (issues were found during compute)
         ***************************************************************/
        enum class EActionState_t : int8_t
        {
            valid   = 0,
            invalid = -1
        };

      private:
        /****************************************************************
         * \struct SGraphMetaData_t
         * \brief Describes the metadata for the graph, holding neccessary
         *        information to access the graph correctly.
         *
         * \param graph_name char[]                - name of the currently loaded graph
         * \param graph_type char[]                - type of the graph (this)
         * \param vertices_c uint64_t              - count of the vertices added to the graph
         * \param edges_c uint64_t                 - count of the edges added to the graph
         * \param vertex_label_c uint16_t          - count of the vertex labels added to the graph
         * \param edge_label_c uint16_t            - count of the edge labels added to the graph
         * \param vertex_label_table_addr uint64_t - address offset for the vertex labels
         * \param edge_label_table_addr uint64_t   - address offset for the edge labels
         * \param label_size uint64_t              - size of one label for either vertices or edges
         ***************************************************************/
        struct SGraphMetaData_t
        {
            char graph_name[CFG_GRAPH_NAME_LENGTH]        = {};
            char graph_type[CFG_GRAPH_MODEL_TYPE_LENGTH]  = {};
            std::atomic<uint64_t> vertices_c              = {};
            std::atomic<uint64_t> edges_c                 = {};
            std::atomic<uint64_t> vertex_label_table_addr = {};
            std::atomic<uint64_t> edge_label_table_addr   = {};
            std::atomic<uint64_t> label_size              = {};
            std::atomic<uint16_t> vertex_label_c          = {};
            std::atomic<uint16_t> edge_label_c            = {};
        };

        /****************************************************************
         * \struct SIndexMetadata_t
         * \brief Describes the metadata for the index table, holding neccessary
         *        information to access the index file correctly.
         *
         * \param index_c uint64_t               - amount of indices stored
         * \param index_list_start_addr uint64_t - start addr of index list
         * \param index_size uint32_t            - size of one index
         ***************************************************************/
        struct SIndexMetadata_t
        {
            std::atomic<uint64_t> index_list_start_addr = {};
            std::atomic<uint32_t> index_c               = {};
            std::atomic<uint32_t> index_size            = {};
        };

        /****************************************************************
         * \struct SLabel_t
         * \brief Structure of a label type within the graph
         *
         * \param label_s char[]    - str of label name
         * \param item_c uint64_t   - count of the generic items under this label type
         * \param label_id uint16_t - unique identifier for relative label usage
         ***************************************************************/
        struct SLabel_t
        {
            char label_s[CFG_LPG_LABEL_LENGTH] = {};
            std::atomic<uint64_t> item_c       = {};
            std::atomic<uint16_t> label_id     = {};
        };

        /****************************************************************
         * \struct SIndexEntry_t
         * \brief Structure of an index entry to the index table.
         *
         * \param id uint64_t       - generic identifier for the payload
         * \param offset uint64_t   - respective offset for the payload
         * \param label_id uint16_t - generic label id for the index container
         ***************************************************************/
        struct SIndexEntry_t
        {
            std::atomic<uint64_t> id     = {};
            std::atomic<uint32_t> offset = END_INDEX;
        };

        /****************************************************************
         * \struct SEdgeEntry_t
         * \brief Structure of an edge entry to the edge list.
         *
         * \param metadata SEdge_t        - metadata info of the edge
         * \param properties_idx uint64_t - offset index for properties
         ***************************************************************/
        struct SEdgeEntry_t
        {
            SEdge_t metadata                     = {};
            std::atomic<uint32_t> properties_idx = END_INDEX;
        };

        /****************************************************************
         * \struct SVertexEntry_t
         * \brief Structure of a vertex entry to the vertex list.
         *
         * \param metadata SVertex_t      - metadata info the vertex
         * \param edge_idx uint64_t       - tail edge offset
         * \param properties_idx uint64_t - offset index for properties
         ***************************************************************/
        struct SVertexEntry_t
        {
            SVertex_t metadata                   = {};
            std::atomic<uint32_t> edge_idx       = END_INDEX;
            std::atomic<uint32_t> properties_idx = END_INDEX;
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
        [[nodiscard]] std::vector<SProperty_t> get_vertex_properties(uint64_t id) override;
        [[nodiscard]] std::vector<SEdge_t> get_edges_by_label(std::string_view label) override;
        [[nodiscard]] std::vector<SVertex_t> get_vertices_by_label(std::string_view label) override;

        [[nodiscard]] std::vector<SEdge_t> get_edges(uint64_t src, uint64_t dst) override;
        [[nodiscard]] std::optional<SEdge_t> get_edge(uint64_t src, uint64_t dst, std::string_view edge_label) override;
        [[nodiscard]] std::vector<SEdge_t> get_edges(uint64_t src, std::string_view edge_label, std::string_view vertex_label) override;
        [[nodiscard]] std::vector<SVertex_t> get_vertices(std::function<bool(const SVertex_t &)> pred) override;
        [[nodiscard]] std::vector<SEdge_t> get_edges(std::function<bool(const SEdge_t &)>) override;
        [[nodiscard]] std::vector<SEdge_t> get_edges(uint64_t src, std::function<bool(const SEdge_t &)>) override;
        [[nodiscard]] std::vector<SEdge_t> get_recursive_edges(uint64_t src,
                                                               std::initializer_list<std::pair<std::string_view, std::string_view>> edge_vertex_label_pairs) override;

        void load_graph(std::filesystem::path path, std::string_view graph) noexcept override;
        void create_graph(std::filesystem::path path, std::string_view graph) noexcept override;
        void add_vertex(std::string_view label, const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop) override;
        void add_vertex(uint64_t id, std::string_view label, const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop) override;
        void add_edge(uint64_t src, uint64_t dst, std::string_view label, const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop) override;

      private:
        friend class CTransaction;
        using SVertexDataBlock   = SDataBlock_t<SVertexEntry_t, 1>;
        using SEdgeDataBlock     = SDataBlock_t<SEdgeEntry_t, DATABLOCK_EDGE_PAYLOAD_C>;
        using SPropertyDataBlock = SDataBlock_t<SProperty_t, DATABLOCK_PROPERTY_PAYLOAD_C>;

        void inline transaction_preamble() noexcept;
        void inline transaction_epilogue() noexcept;

        void read_index_list() noexcept;
        void define_vertex_lut() noexcept;
        void store_graph_metadata() noexcept;
        void store_index_metadata() noexcept;
        uint32_t store_property_entry(const SProperty_t & prop, uint32_t next_ref) noexcept;
        void store_index_entry(uint64_t id, uint16_t label_id, uint32_t vertex_offset) noexcept;
        uint32_t store_vertex_entry(uint64_t id, uint16_t label_id, const std::vector<SProperty_t> & props) noexcept;
        uint32_t store_edge_entry(uint32_t next_ref, uint64_t src, uint64_t dst, uint16_t label_id, const std::vector<SProperty_t> & props) noexcept;

        inline SRef_t<SGraphMetaData_t> read_graph_metadata() noexcept;
        inline SRef_t<SIndexMetadata_t> read_index_metadata() noexcept;
        inline SRef_t<SIndexEntry_t> read_index_entry(uint32_t offset) noexcept;
        inline SRef_t<SLabel_t> read_vertex_label_entry(uint32_t offset) noexcept;
        inline SRef_t<SLabel_t> read_edge_label_entry(uint32_t offset) noexcept;

        [[nodiscard]] EActionState_t rm_vertex_entry(uint64_t vertex_id) noexcept;
        [[nodiscard]] EActionState_t rm_edge_entry(uint64_t src_vertex_id, uint64_t dst_vertex_id) noexcept;
        [[nodiscard]] EActionState_t rm_edge_entry(std::string_view, uint64_t src_vertex_id, uint64_t dst_vertex_id) noexcept;

        [[nodiscard]] EActionState_t add_vertex_entry(uint64_t id, std::string_view label, const std::vector<SProperty_t> & props) noexcept;
        [[nodiscard]] EActionState_t add_vertex_entry(std::string_view label, const std::vector<SProperty_t> & props) noexcept;
        [[nodiscard]] EActionState_t add_edge_entry(uint64_t src, uint64_t dst, std::string_view label, const std::vector<SProperty_t> & props) noexcept;

        [[nodiscard]] uint16_t create_edge_label(std::string_view) noexcept;
        [[nodiscard]] uint16_t create_vertex_label(std::string_view) noexcept;
        [[nodiscard]] uint64_t get_unassigned_vertex_id(size_t label_idx) const noexcept;
        [[nodiscard]] inline std::optional<uint16_t> check_if_edge_label_exists(std::string_view) noexcept;
        [[nodiscard]] inline std::optional<uint16_t> check_if_vertex_label_exists(std::string_view) noexcept;
        [[nodiscard]] std::optional<SRef_t<SVertexDataBlock>> get_vertex_by_id(uint64_t id) noexcept;

        static std::vector<SProperty_t> transform_properties(const std::vector<std::pair<std::string_view, std::string_view>> &) noexcept;

        bool m_sync_needed;
        std::string m_graph_name;
        std::vector<std::vector<uint64_t>> m_label_map;

        uint8_t m_syncing;
        std::mutex m_sync_lock;
        uint32_t m_transaction_ref_c;
        std::condition_variable m_cv_sync;
        std::unique_lock<std::mutex> m_unq_lock;
        std::shared_ptr<logger::CLogSystem> m_log_system;

        //~ Disk/file drivers for graph mapping from disk to memory
        CDiskDriver m_master_file;
        CDiskDriver m_index_file;
        CDatablockFile<SVertexEntry_t, 1> m_vertices_file;
        CDatablockFile<SEdgeEntry_t, DATABLOCK_EDGE_PAYLOAD_C> m_edges_file;
        CDatablockFile<SProperty_t, DATABLOCK_PROPERTY_PAYLOAD_C> m_properties_file;
        std::shared_ptr<CTransaction> m_transactions = {};

        utils::CThreadPool<8> m_thread_pool;
        static constexpr uint8_t VERTEX_LABELS_MAX_AMT = 128;
        static constexpr uint8_t EDGE_LABELS_MAX_AMT   = 128;
        static constexpr uint64_t METADATA_START_ADDR  = 0x00000000;

        static constexpr const char * MASTER_FILE_NAME     = "master";
        static constexpr const char * INDEX_FILE_NAME      = "index";
        static constexpr const char * VERTICES_FILE_NAME   = "vertices";
        static constexpr const char * EDGES_FILE_NAME      = "edges";
        static constexpr const char * PROPERTIES_FILE_NAME = "properties";

        static constexpr uint64_t INDEX_LIST_START_ADDR    = sizeof(SIndexMetadata_t);
        static constexpr uint64_t VERTEX_LABELS_START_ADDR = METADATA_START_ADDR + sizeof(SGraphMetaData_t);
        static constexpr uint64_t EDGE_LABELS_START_ADDR   = METADATA_START_ADDR + sizeof(SGraphMetaData_t) + sizeof(SLabel_t) * VERTEX_LABELS_MAX_AMT;

        const std::function<bool()> wait_on_syncing      = [this]() -> bool { return m_syncing == 0; };
        const std::function<bool()> wait_on_transactions = [this]() -> bool { return m_transaction_ref_c == 0; };
    };
} // namespace graphquery::database::storage
