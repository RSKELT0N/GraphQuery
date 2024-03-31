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

#define DATABLOCK_EDGE_PAYLOAD_C      3 // ~ Amount of edges for edge block.
#define DATABLOCK_PROPERTY_PAYLOAD_C  3 // ~ Amount of edges for property block.
#define DATABLOCK_LABEL_REF_PAYLOAD_C 3 // ~ Amount of edges for label ref block.

#define VERTEX_INITIALISED_STATE_BIT 0 // ~ Vertex state bit 0 (initialised (1) unitialised (0)), used to check if the vertex is initialised or not.
#define VERTEX_MARKED_STATE_BIT      1 // ~ Vertex state bit 1 (marked (1) unmarked(0)), used to check if vertex has been marked for deletion.
#define VERTEX_VALID_STATE_BIT       2 // ~ Vertex state bit 1 (valid (1) invalid(0)), used to check if vertex can be retrieved.

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
            Id_t vertices_c                           = {};
            Id_t edges_c                              = {};
            uint32_t vertex_label_table_addr             = {};
            uint32_t edge_label_table_addr               = {};
            uint32_t label_size                          = {};
            uint16_t vertex_label_c                      = {};
            uint16_t edge_label_c                        = {};
            uint8_t flush_needed                         = {};
            uint8_t prune_needed                         = {};
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
            Id_t edge_idx  = END_INDEX;
        };

      public:
        explicit CMemoryModelMMAPLPG(const std::shared_ptr<logger::CLogSystem> &, const bool & _sync_state_);
        ~CMemoryModelMMAPLPG() override;

        void close() noexcept override;
        void create_rollback(std::string_view) noexcept override;
        void rollback(uint8_t rollback_entry) noexcept override;
        void sync_graph() noexcept override;
        void rm_vertex(Id_t src) override;
        void rm_edge(Id_t src, Id_t dst) override;
        uint32_t out_degree(Id_t id) noexcept override;
        uint32_t in_degree(Id_t id) noexcept override;
        void calc_outdegree(uint32_t[]) noexcept override;
        void calc_indegree(uint32_t[]) noexcept override;
        void calc_vertex_sparse_map(Id_t []) noexcept override;
        double get_avg_out_degree() noexcept override;
        uint32_t out_degree_by_id(Id_t id) noexcept override;
        void rm_edge(Id_t src, Id_t dst, std::string_view edge_label) override;
        void edgemap(const std::unique_ptr<analytic::IRelax> & relax) noexcept override;
        [[nodiscard]] std::vector<std::string> fetch_rollback_table() const noexcept override;
        std::unique_ptr<std::vector<std::vector<int64_t>>> make_inverse_graph() noexcept override;
        void src_edgemap(Id_t vertex_offset, const std::function<void(int64_t src, int64_t dst)> &) override;

        [[nodiscard]] inline int64_t get_num_edges() override;
        [[nodiscard]] inline int64_t get_num_vertices() override;
        [[nodiscard]] int64_t get_total_num_vertices() noexcept override;
        [[nodiscard]] inline uint16_t get_num_vertex_labels() override;
        [[nodiscard]] inline uint16_t get_num_edge_labels() override;
        [[nodiscard]] std::string_view get_name() noexcept override;
        [[nodiscard]] std::optional<SVertex_t> get_vertex(Id_t id) override;
        [[nodiscard]] std::optional<Id_t> get_vertex_idx(Id_t id) noexcept override;
        [[nodiscard]] std::vector<SEdge_t> get_edges_by_label(std::string_view label) override;
        [[nodiscard]] std::vector<SVertex_t> get_vertices_by_label(std::string_view label) override;

        [[nodiscard]] std::vector<SProperty_t> get_properties_by_id(int64_t id) override;
        [[nodiscard]] std::vector<SProperty_t> get_properties_by_property_id(uint32_t id) override;
        [[nodiscard]] std::vector<SProperty_t> get_properties_by_vertex(Id_t src) override;
        [[nodiscard]] std::unordered_map<std::string, std::string> get_properties_by_id_map(int64_t id) override;
        [[nodiscard]] std::unordered_map<std::string, std::string> get_properties_by_property_id_map(uint32_t id) override;
        [[nodiscard]] std::unordered_map<std::string, std::string> get_properties_by_vertex_map(Id_t src) override;

        [[nodiscard]] std::vector<SEdge_t> get_edges(Id_t src, Id_t dst) override;
        [[nodiscard]] std::vector<SEdge_t> get_edges(Id_t src, std::string_view edge_label, std::string_view vertex_label) override;
        [[nodiscard]] std::unordered_set<Id_t> get_edge_dst_vertices(Id_t src, std::string_view edge_label, std::string_view vertex_label) override;
        [[nodiscard]] std::vector<SEdge_t> get_recursive_edges(Id_t src, std::vector<SProperty_t> edge_vertex_label_pairs) override;

        [[nodiscard]] std::optional<SEdge_t> get_edge(Id_t src_vertex_id, std::string_view edge_label, int64_t dst_vertex_id) override;
        [[nodiscard]] std::vector<SEdge_t> get_edges(const std::function<bool(const SEdge_t &)> & pred) override;
        [[nodiscard]] std::vector<SVertex_t> get_vertices(const std::function<bool(const SVertex_t &)> & pred) override;
        [[nodiscard]] std::vector<SEdge_t> get_edges(std::string_view vertex_label, std::string_view edge_label, Id_t dst) override;
        [[nodiscard]] std::vector<SEdge_t> get_edges_by_offset(Id_t vertex_id, std::string_view edge_label, std::string_view vertex_label) override;
        [[nodiscard]] std::vector<SEdge_t> get_edges(std::string_view vertex_label, const std::function<bool(const SEdge_t &)> & pred) override;
        [[nodiscard]] std::unordered_set<Id_t> get_edge_dst_vertices(Id_t src, const std::function<bool(const SEdge_t &)> & pred) override;
        [[nodiscard]] std::vector<SEdge_t> get_edges(std::string_view vertex_label, std::string_view edge_label, const std::function<bool(const SEdge_t &)> & pred) override;
        [[nodiscard]] std::vector<SEdge_t> get_edges(std::string_view vertex_label, std::string_view edge_label) override;
        [[nodiscard]] std::vector<SEdge_t> get_edges(std::string_view vertex_label, std::string_view edge_label, std::string_view dst_vertex_label) override;

        void load_graph(std::filesystem::path path, std::string_view graph) noexcept override;
        void create_graph(std::filesystem::path path, std::string_view graph) noexcept override;
        void add_vertex(const std::vector<std::string_view> & label, const std::vector<SProperty_t> & prop) override;
        void add_vertex(Id_t src, const std::vector<std::string_view> & labels, const std::vector<SProperty_t> & prop) override;
        void add_edge(Id_t src, Id_t dst, std::string_view label, const std::vector<SProperty_t> & prop, bool undirected) override;

      private:
        friend class CTransaction;
        using SVertexDataBlock   = SDataBlock_t<SVertexEntry_t, 1>;
        using SEdgeDataBlock     = SDataBlock_t<SEdgeEntry_t, DATABLOCK_EDGE_PAYLOAD_C>;
        using SPropertyDataBlock = SDataBlock_t<SProperty_t, DATABLOCK_PROPERTY_PAYLOAD_C>;
        using SLabelRefDataBlock = SDataBlock_t<uint16_t, DATABLOCK_LABEL_REF_PAYLOAD_C>;

        void rollback() noexcept;
        void inline reset_graph() noexcept;
        void inline setup_files(const std::filesystem::path & path, bool initialise) noexcept;
        void inline transaction_preamble() noexcept;
        void inline transaction_epilogue() noexcept;
        void persist_graph_changes() noexcept;

        std::optional<SRef_t<SVertexDataBlock>> get_vertex_by_offset(uint32_t offset) noexcept;
        void read_index_list() noexcept;
        void define_luts() noexcept;
        void store_graph_metadata() noexcept;
        [[nodiscard]] Id_t store_label_entry(uint16_t label_id, Id_t next_ref) noexcept;
        [[nodiscard]] Id_t store_property_entry(const SProperty_t & prop, Id_t next_ref) noexcept;
        [[nodiscard]] bool store_index_entry(Id_t id, const std::unordered_set<uint16_t> & label_ids, uint32_t vertex_offset) noexcept;
        [[nodiscard]] bool store_vertex_entry(Id_t id, const std::unordered_set<uint16_t> & label_id, const std::vector<SProperty_t> & props) noexcept;
        void store_edge_entry(Id_t src, Id_t dst, uint16_t edge_label_id, const std::vector<SProperty_t> & props, bool undirected) noexcept;

        template<bool write = false>
        inline SRef_t<SGraphMetaData_t, write> read_graph_metadata() noexcept;

        template<bool write = false> inline SRef_t<SLabel_t, write> read_vertex_label_entry(uint32_t offset) noexcept;
        template<bool write = false> inline SRef_t<SLabel_t, write> read_edge_label_entry(uint32_t offset) noexcept;

        [[nodiscard]] EActionState_t rm_vertex_entry(Id_t src) noexcept;
        [[nodiscard]] EActionState_t rm_edge_entry(Id_t src, Id_t dst) noexcept;
        [[nodiscard]] EActionState_t rm_edge_entry(Id_t src, Id_t dst, std::string_view edge_label) noexcept;

        [[nodiscard]] EActionState_t add_vertex_entry(Id_t id, const std::vector<std::string_view> & labels, const std::vector<SProperty_t> & props) noexcept;
        [[nodiscard]] EActionState_t add_vertex_entry(const std::vector<std::string_view> & labels, const std::vector<SProperty_t> & props) noexcept;
        [[nodiscard]] EActionState_t add_edge_entry(Id_t src, Id_t dst, std::string_view edge_label, const std::vector<SProperty_t> & props, bool undirected) noexcept;

        [[nodiscard]] bool contains_vertex_label_id(int64_t vertex_offset, uint16_t label_id) noexcept;
        [[nodiscard]] uint16_t create_edge_label(std::string_view) noexcept;
        [[nodiscard]] uint16_t create_vertex_label(std::string_view) noexcept;
        [[nodiscard]] inline std::optional<uint16_t> check_if_edge_label_exists(const std::string_view &) noexcept;
        [[nodiscard]] inline std::optional<uint16_t> check_if_vertex_label_exists(const std::string_view &) noexcept;
        [[nodiscard]] inline std::unordered_set<uint16_t> get_vertex_labels(const std::vector<std::string_view> & labels, bool create_if_absent = false) noexcept;

        [[nodiscard]] std::optional<SRef_t<SVertexDataBlock>> get_vertex_by_id(Id_t id) noexcept;
        [[nodiscard]] std::vector<int64_t> get_vertices_offset_by_label(std::string_view label);
        [[nodiscard]] std::vector<SEdge_t> get_edges_by_offset(uint32_t src_vertex_id, uint32_t dst_vertex_id);
        [[nodiscard]] std::vector<SEdge_t> get_edges_by_offset(uint32_t vertex_id, const std::function<bool(const SEdge_t &)> & pred);
        [[nodiscard]] std::vector<SEdge_t> get_edges_by_offset(uint32_t src_vertex_id, uint32_t dst_vertex_id, const std::function<bool(const SEdge_t &)> & pred);
        [[nodiscard]] std::vector<SEdge_t> get_edges_by_offset(uint32_t vertex_id, uint16_t edge_label_id, const std::function<bool(const SEdge_t &)> & pred);
        [[nodiscard]] std::vector<SEdge_t> get_edges_by_id(Id_t src, const std::function<bool(const SEdge_t &)> & pred);

        std::string m_graph_name;
        std::string m_graph_path;
        std::vector<std::vector<Id_t>> m_label_vertex;
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
