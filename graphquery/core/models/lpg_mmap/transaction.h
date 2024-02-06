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

#include "db/storage/config.h"
#include "db/storage/graph_model.h"

#include <filesystem>

namespace graphquery::database::storage
{
    class CTransaction final
    {
        enum class ETransactionType : uint8_t
        {
            vertex,
            edge
        };

        struct SHeaderBlock
        {
            uint64_t transaction_c           = {};
            uint64_t transactions_start_addr = {};
            uint64_t eof_addr                = {};
        } __attribute__((packed));

        struct SVertexCommit
        {
            char label[CFG_LPG_LABEL_LENGTH] = {""};
            uint64_t optional_id             = {0};
            uint16_t property_c              = {0};
            uint8_t remove                   = {0};
        } __attribute__((packed));

        struct SEdgeCommit
        {
            char label[CFG_LPG_LABEL_LENGTH] = {""};
            uint64_t src                     = {0};
            uint64_t dst                     = {0};
            uint16_t property_c              = {0};
            uint8_t remove                   = {0};
        } __attribute__((packed));

        template<typename T>
        struct STransaction
        {
            ETransactionType type = ETransactionType::vertex;
            T commit              = {};
        } __attribute__((packed));

      public:
        CTransaction(const std::filesystem::path & local_path, ILPGModel * lpg);
        ~CTransaction() = default;

        void close() noexcept;
        void init() noexcept;
        void reset() noexcept;
        void handle_transactions() noexcept;
        void commit_rm_vertex(uint64_t id) noexcept;
        void commit_rm_edge(uint64_t src, uint64_t dst, std::string_view label = "") noexcept;

        void commit_vertex(std::string_view label, const std::vector<ILPGModel::SProperty_t> & props, uint64_t optional_id = -1) noexcept;
        void commit_edge(uint64_t src, uint64_t dst, std::string_view label, const std::vector<ILPGModel::SProperty_t> & props) noexcept;

      private:
        using SVertexTransaction = STransaction<SVertexCommit>;
        using SEdgeTransaction   = STransaction<SEdgeCommit>;

        void load();
        void set_up();
        void define_transaction_header();

        template<typename T>
        inline SRef_t<T> read_transaction(uint64_t seek);
        inline SRef_t<SHeaderBlock> read_transaction_header();

        void process_vertex_transaction(SRef_t<SVertexTransaction> &, const std::vector<ILPGModel::SProperty_t> & props) const noexcept;
        void process_edge_transaction(SRef_t<SEdgeTransaction> &, const std::vector<ILPGModel::SProperty_t> & props) const noexcept;

        ILPGModel * m_lpg;
        CDiskDriver m_transaction_file;
        static constexpr int64_t INITIAL_TRANSACTION_FILE_SIZE = KB(4);
        static constexpr const char * TRANSACTION_FILE_NAME    = "transactions";
        static constexpr uint64_t TRANSACTION_HEADER_START_ADDR = 0x00000000;
        static constexpr uint64_t TRANSACTIONS_START_ADDR       = sizeof(SHeaderBlock);
    };
} // namespace graphquery::database::storage
