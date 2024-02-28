/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file lpg_mmap.h
 * \brief Dervied instance of a memory model, supporting
 *        the labelled property graph functionality, with
 *        directly accessing memory map for graph writes/reads.
 *
 *        (under development)
 *
 *        TODO: Same for searching for vertex, keep iterating next until id and label id match.
 *        TODO: Once implemented, fix all queries and transactions to support new API.
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
#include "index_file.hpp"
#include "transaction.h"

#include <vector>
#include <optional>

#define DATABLOCK_EDGE_PAYLOAD_C      3
#define DATABLOCK_PROPERTY_PAYLOAD_C  3
#define DATABLOCK_LABEL_REF_PAYLOAD_C 3

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
         * \param vertices_c int64_t              - count of the vertices added to the graph
         * \param edges_c int64_t                 - count of the edges added to the graph
         * \param vertex_label_c uint16_t          - count of the vertex labels added to the graph
         * \param edge_label_c uint16_t            - count of the edge labels added to the graph
         * \param vertex_label_table_addr uint32_t - address offset for the vertex labels
         * \param edge_label_table_addr uint32_t   - address offset for the edge labels
         * \param label_size uint32_t              - size of one label for either vertices or edges
         ***************************************************************/
        struct SGraphMetaData_t
        {
            char graph_name[CFG_GRAPH_NAME_LENGTH]       = {};
            char graph_type[CFG_GRAPH_MODEL_TYPE_LENGTH] = {};
            int64_t vertices_c                           = {};
            int64_t edges_c                              = {};
            uint32_t vertex_label_table_addr             = {};
            uint32_t edge_label_table_addr               = {};
            uint32_t label_size                          = {};
            uint16_t vertex_label_c                      = {};
            uint16_t edge_label_c                        = {};
        };

        /****************************************************************
         * \struct SEdgeEntry_t
         * \brief Structure of an edge entry to the edge list.
         *
         * \param metadata SEdge_t        - metadata info of the edge
         ***************************************************************/
        struct SEdgeEntry_t
        {
            SEdge_t metadata = {};
        };

        /****************************************************************
         * \struct SVertexEntry_t
         * \brief Structure of a vertex entry to the vertex list.
         *
         * \param metadata SVertex_t      - metadata info the vertex
         * \param edge_idx uint32_t       - tail edge offset
         ***************************************************************/
        struct SVertexEntry_t
        {
            SVertex_t metadata = {};
            uint32_t edge_idx  = END_INDEX;
        };

      public:
        explicit CMemoryModelMMAPLPG(const std::shared_ptr<logger::CLogSystem> &);
        ~CMemoryModelMMAPLPG() override;

        void close() noexcept override;
        void save_graph() noexcept override;
        void rm_vertex(SNodeID src) override;
        void rm_edge(SNodeID src, SNodeID dst) override;
        void calc_outdegree(std::shared_ptr<uint32_t[]>) noexcept override;
        void rm_edge(SNodeID src, SNodeID dst, std::string_view edge_label) override;
        void edgemap(const std::unique_ptr<analytic::IRelax> & relax) noexcept override;

        [[nodiscard]] inline int64_t get_num_edges() override;
        [[nodiscard]] inline int64_t get_num_vertices() override;
        [[nodiscard]] std::string_view get_name() noexcept override;
        [[nodiscard]] std::optional<SVertex_t> get_vertex(SNodeID id) override;
        [[nodiscard]] std::vector<SEdge_t> get_edges_by_label(std::string_view label) override;
        [[nodiscard]] std::vector<SVertex_t> get_vertices_by_label(std::string_view label) override;

        [[nodiscard]] std::vector<SProperty_t> get_properties_by_id(int64_t id) override;
        [[nodiscard]] std::vector<SProperty_t> get_properties_by_property_id(uint32_t id) override;
        [[nodiscard]] std::vector<SProperty_t> get_properties_by_vertex(SNodeID src) override;
        [[nodiscard]] std::unordered_map<std::string, std::string> get_properties_by_id_map(int64_t id) override;
        [[nodiscard]] std::unordered_map<std::string, std::string> get_properties_by_property_id_map(uint32_t id) override;
        [[nodiscard]] std::unordered_map<std::string, std::string> get_properties_by_vertex_map(SNodeID src) override;

        [[nodiscard]] std::vector<SEdge_t> get_edges(SNodeID src, SNodeID dst) override;
        [[nodiscard]] std::vector<SEdge_t> get_edges(SNodeID src, std::string_view edge_label, std::string_view vertex_label) override;
        [[nodiscard]] std::unordered_set<int64_t> get_edge_dst_vertices(SNodeID src, std::string_view edge_label, std::string_view vertex_label) override;
        [[nodiscard]] std::vector<SEdge_t> get_recursive_edges(SNodeID src, std::vector<SProperty_t> edge_vertex_label_pairs) override;

        [[nodiscard]] std::vector<SEdge_t> get_edges(const std::function<bool(const SEdge_t &)> & pred) override;
        [[nodiscard]] std::vector<SVertex_t> get_vertices(const std::function<bool(const SVertex_t &)> & pred) override;
        [[nodiscard]] std::vector<SEdge_t> get_edges(std::string_view vertex_label, std::string_view edge_label, SNodeID dst) override;
        [[nodiscard]] std::vector<SEdge_t> get_edges(uint32_t vertex_id, std::string_view edge_label, std::string_view vertex_label) override;
        [[nodiscard]] std::vector<SEdge_t> get_edges(std::string_view vertex_label, const std::function<bool(const SEdge_t &)> & pred) override;
        [[nodiscard]] std::unordered_set<int64_t> get_edge_dst_vertices(SNodeID src, const std::function<bool(const SEdge_t &)> & pred) override;
        [[nodiscard]] std::vector<SEdge_t> get_edges(std::string_view vertex_label, std::string_view edge_label, const std::function<bool(const SEdge_t &)> & pred) override;

        void load_graph(std::filesystem::path path, std::string_view graph) noexcept override;
        void create_graph(std::filesystem::path path, std::string_view graph) noexcept override;
        void add_vertex(const std::vector<std::string_view> & label, const std::vector<SProperty_t> & prop) override;
        void add_vertex(SNodeID src, const std::vector<std::string_view> & labels, const std::vector<SProperty_t> & prop) override;
        void add_edge(SNodeID src, SNodeID dst, std::string_view label, const std::vector<SProperty_t> & prop, bool undirected) override;

      private:
        friend class CTransaction;
        using SVertexDataBlock   = SDataBlock_t<SVertexEntry_t, 1>;
        using SEdgeDataBlock     = SDataBlock_t<SEdgeEntry_t, DATABLOCK_EDGE_PAYLOAD_C>;
        using SPropertyDataBlock = SDataBlock_t<SProperty_t, DATABLOCK_PROPERTY_PAYLOAD_C>;
        using SLabelRefDataBlock = SDataBlock_t<uint16_t, DATABLOCK_LABEL_REF_PAYLOAD_C>;

        void inline setup_files(const std::filesystem::path & path, bool initialise) noexcept;
        void inline transaction_preamble() noexcept;
        void inline transaction_epilogue() noexcept;

        std::optional<SRef_t<SVertexDataBlock>> get_vertex_by_offset(uint32_t offset) noexcept;
        void read_index_list() noexcept;
        void define_luts() noexcept;
        void store_graph_metadata() noexcept;
        uint32_t store_label_entry(uint16_t label_id, uint32_t next_ref) noexcept;
        uint32_t store_property_entry(const SProperty_t & prop, uint32_t next_ref) noexcept;
        void store_index_entry(SNodeID id, const std::unordered_set<uint16_t> & label_ids, uint32_t vertex_offset) noexcept;
        uint32_t store_vertex_entry(SNodeID id, const std::unordered_set<uint16_t> & label_id, const std::vector<SProperty_t> & props) noexcept;
        uint32_t store_edge_entry(uint32_t next_ref, SNodeID src, SNodeID dst, uint16_t edge_label_id, const std::vector<SProperty_t> & props) noexcept;

        inline SRef_t<SGraphMetaData_t> read_graph_metadata() noexcept;
        inline SRef_t<SLabel_t> read_vertex_label_entry(uint32_t offset) noexcept;
        inline SRef_t<SLabel_t> read_edge_label_entry(uint32_t offset) noexcept;

        [[nodiscard]] EActionState_t rm_vertex_entry(SNodeID src) noexcept;
        [[nodiscard]] EActionState_t rm_edge_entry(SNodeID src, SNodeID dst) noexcept;
        [[nodiscard]] EActionState_t rm_edge_entry(SNodeID src, SNodeID dst, std::string_view edge_label) noexcept;

        [[nodiscard]] EActionState_t add_vertex_entry(int64_t id, const std::vector<std::string_view> & labels, const std::vector<SProperty_t> & props) noexcept;
        [[nodiscard]] EActionState_t add_vertex_entry(const std::vector<std::string_view> & labels, const std::vector<SProperty_t> & props) noexcept;
        [[nodiscard]] EActionState_t add_edge_entry(SNodeID src, SNodeID dst, std::string_view edge_label, const std::vector<SProperty_t> & props, bool undirected) noexcept;

        [[nodiscard]] uint16_t create_edge_label(std::string_view) noexcept;
        [[nodiscard]] uint16_t create_vertex_label(std::string_view) noexcept;
        [[nodiscard]] int64_t get_unassigned_vertex_id(size_t label_idx) const noexcept;
        [[nodiscard]] inline std::optional<uint16_t> check_if_edge_label_exists(const std::string_view &) noexcept;
        [[nodiscard]] inline std::optional<uint16_t> check_if_vertex_label_exists(const std::string_view &) noexcept;
        [[nodiscard]] inline std::unordered_set<uint16_t> get_vertex_labels(const std::vector<std::string_view> & labels, bool create_if_absent = false) noexcept;

        [[nodiscard]] std::optional<SRef_t<SVertexDataBlock>> get_vertex_by_id(SNodeID id) noexcept;
        [[nodiscard]] std::vector<int64_t> get_vertices_offset_by_label(std::string_view label);
        [[nodiscard]] std::vector<SEdge_t> get_edges_by_offset(uint32_t src_vertex_id, uint32_t dst_vertex_id);
        [[nodiscard]] std::vector<SEdge_t> get_edges_by_offset(uint32_t vertex_id, const std::function<bool(const SEdge_t &)> & pred);
        [[nodiscard]] std::vector<SEdge_t> get_edges_by_offset(uint32_t src_vertex_id, uint32_t dst_vertex_id, const std::function<bool(const SEdge_t &)> & pred);
        [[nodiscard]] std::vector<SEdge_t> get_edges_by_offset(uint32_t vertex_id, uint16_t edge_label_id, const std::function<bool(const SEdge_t &)> & pred);
        [[nodiscard]] std::vector<SEdge_t> get_edges_by_id(int64_t src, const std::function<bool(const SEdge_t &)> & pred);

        bool m_sync_needed;
        std::string m_graph_name;
        std::string m_graph_path;
        std::vector<std::vector<uint32_t>> m_label_vertex;
        std::unordered_map<std::string, uint16_t> m_v_label_map;
        std::unordered_map<std::string, uint16_t> m_e_label_map;

        uint8_t m_syncing;
        std::mutex m_sync_lock;
        uint32_t m_transaction_ref_c;
        std::condition_variable m_cv_sync;
        std::unique_lock<std::mutex> m_unq_lock;

        //~ Disk/file drivers for graph mapping from disk to memory
        CDiskDriver m_master_file;
        CIndexFile m_index_file;
        CDatablockFile<SVertexEntry_t> m_vertices_file;
        CDatablockFile<SEdgeEntry_t, DATABLOCK_EDGE_PAYLOAD_C> m_edges_file;
        CDatablockFile<SProperty_t, DATABLOCK_PROPERTY_PAYLOAD_C> m_properties_file;
        CDatablockFile<uint16_t, DATABLOCK_LABEL_REF_PAYLOAD_C> m_label_ref_file;
        std::shared_ptr<CTransaction> m_transactions = {};

        utils::CThreadPool<8> m_thread_pool;
        static constexpr uint8_t VERTEX_LABELS_MAX_AMT = 128;
        static constexpr uint8_t EDGE_LABELS_MAX_AMT   = 128;
        static constexpr uint32_t METADATA_START_ADDR  = 0x00000000;

        static constexpr const char * MASTER_FILE_NAME     = "master";
        static constexpr const char * INDEX_FILE_NAME      = "index";
        static constexpr const char * VERTICES_FILE_NAME   = "vertices";
        static constexpr const char * EDGES_FILE_NAME      = "edges";
        static constexpr const char * PROPERTIES_FILE_NAME = "properties";
        static constexpr const char * LABEL_REF_FILE_NAME  = "label_map";

        static constexpr uint32_t VERTEX_LABELS_START_ADDR = METADATA_START_ADDR + sizeof(SGraphMetaData_t);
        static constexpr uint32_t EDGE_LABELS_START_ADDR   = METADATA_START_ADDR + sizeof(SGraphMetaData_t) + sizeof(SLabel_t) * VERTEX_LABELS_MAX_AMT;

        const std::function<bool()> wait_on_syncing      = [this]() -> bool { return m_syncing == 0; };
        const std::function<bool()> wait_on_transactions = [this]() -> bool { return m_transaction_ref_c == 0; };
    };
} // namespace graphquery::database::storage
