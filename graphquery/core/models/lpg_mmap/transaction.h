/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file transaction.h
 * \brief Transaction header file, providing function defintitions
 *        for storing database logs towards a local file. For
 *        publishing and retrieving actions made, to ensure
 *        persistent storage.
 ************************************************************/

#pragma once

#include "db/storage/graph_model.h"

#include <filesystem>

namespace graphquery::database::storage
{
    class CTransaction final
    {
      public:
        enum class ETransactionType : uint8_t
        {
            vertex,
            edge
        };

        struct SHeaderBlock
        {
            uint64_t transaction_c               = {0};
            uint64_t transactions_start_addr     = {0};
            uint64_t rollback_entries_start_addr = {0};
            uint64_t eof_addr                    = {0};
            uint64_t priv_eof_addr               = {0};
            uint64_t valid_eof_addr              = {0};
            uint16_t running_transactions        = {0};
            uint8_t rollback_entry_c             = {0};
        };

        struct SRollbackEntry
        {
            char name[CFG_GRAPH_ROLLBACK_NAME_LENGTH] = {""};
            uint64_t eor_addr                         = {0};
        };

        struct SVertexCommit
        {
            Id_t optional_id    = {0};
            uint16_t label_c    = {0};
            uint16_t property_c = {0};
            uint8_t remove      = {0};
        };

        struct SEdgeCommit
        {
            Id_t src                              = {0};
            Id_t dst                              = {0};
            char edge_label[CFG_LPG_LABEL_LENGTH] = {""};
            uint16_t property_c                   = {0};
            uint8_t remove                        = {0};
            uint8_t undirected                    = {0};
        };

        template<typename T>
        struct STransaction
        {
            ETransactionType type = ETransactionType::vertex;
            T commit              = {};
            uint16_t size         = {0};
            uint8_t committed     = {0};
        };

        explicit CTransaction(const std::filesystem::path & local_path, ILPGModel * lpg, const std::shared_ptr<logger::CLogSystem> &, const bool & sync_state_);
        ~CTransaction() = default;

        void close() noexcept;
        void init() noexcept;
        void reset() noexcept;
        void close_transaction_gracefully() noexcept;
        void update_graph_state() noexcept;
        void rollback(uint64_t, int64_t start_addr) noexcept;
        void handle_transactions() noexcept;
        uint64_t get_valid_eor_addr() noexcept;
        int64_t get_transaction_start_addr() noexcept;
        void store_rollback_entry(std::string_view name) noexcept;
        SRef_t<SRollbackEntry> read_rollback_entry(uint8_t) noexcept;

        [[nodiscard]] uint64_t log_rm_vertex(Id_t src) noexcept;
        [[nodiscard]] uint64_t log_rm_edge(Id_t src, Id_t dst, std::string_view edge_label = "") noexcept;

        [[nodiscard]] uint64_t
        log_vertex(const std::vector<std::string_view> & labels, const std::vector<ILPGModel::SProperty_t> & props, Id_t optional_id = std::numeric_limits<Id_t>::max()) noexcept;

        [[nodiscard]] uint64_t log_edge(Id_t src, Id_t dst, std::string_view edge_label, const std::vector<ILPGModel::SProperty_t> & props, bool undirected) noexcept;

        template<typename T>
        void commit_transaction(uint64_t transaction_addr) noexcept;
        [[nodiscard]] std::vector<std::string> fetch_rollback_table() noexcept;

        using SVertexTransaction = STransaction<SVertexCommit>;
        using SEdgeTransaction   = STransaction<SEdgeCommit>;

      private:
        void load();
        void set_up();
        void store_transaction_header();

        template<typename T, bool write = false>
        inline SRef_t<T, write> read_transaction(uint64_t seek);
        SRef_t<SHeaderBlock> read_transaction_header();

        void storage_persist() const noexcept;
        static inline std::vector<std::string_view> slabel_to_strview_vector(const std::vector<ILPGModel::SLabel> & vec) noexcept;
        void process_edge_transaction(SRef_t<SEdgeTransaction> &, const std::vector<ILPGModel::SProperty_t> & props) const noexcept;
        void process_vertex_transaction(SRef_t<SVertexTransaction> &, const std::vector<ILPGModel::SLabel> & src_labels, const std::vector<ILPGModel::SProperty_t> & props) const noexcept;

        ILPGModel * m_lpg;
        const bool & _sync_state_;
        CDiskDriver m_transaction_file;
        std::shared_ptr<logger::CLogSystem> m_log_system;
        static constexpr const char * TRANSACTION_FILE_NAME    = "transactions";
        static constexpr uint8_t ROLLBACK_MAX_AMOUNT           = 5;
        static constexpr int32_t TRANSACTION_HEADER_START_ADDR = 0x00000000;
        static constexpr int64_t ROLLBACK_ENTRIES_START_ADDR   = TRANSACTION_HEADER_START_ADDR + sizeof(SHeaderBlock);
        static constexpr int64_t TRANSACTIONS_START_ADDR       = _align_(ROLLBACK_ENTRIES_START_ADDR + ROLLBACK_MAX_AMOUNT * sizeof(SRollbackEntry));
    };

    template<typename T>
    void CTransaction::commit_transaction(const uint64_t transaction_addr) noexcept
    {
        auto ref = m_transaction_file.ref<STransaction<T>>(static_cast<int64_t>(transaction_addr));
        utils::atomic_store(&ref->committed, 1);

        auto header_ptr = read_transaction_header();
        utils::atomic_fetch_dec(&header_ptr->running_transactions);
        utils::atomic_store(&header_ptr->valid_eof_addr, std::max(header_ptr->valid_eof_addr, transaction_addr + ref->size));
        storage_persist();
    }
} // namespace graphquery::database::storage