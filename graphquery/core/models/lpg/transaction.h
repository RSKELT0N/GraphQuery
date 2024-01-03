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

#include "lpg.h"
// #include "db/system.h"

#include <filesystem>

namespace graphquery::database::storage
{
    class CTransaction final
    {
      private:
        enum class ETransactionType : uint8_t
        {
            vertex,
            edge
        };

        struct SHeaderBlock
        {
            uint32_t transaction_c;
            uint64_t eof_addr;
        } __attribute__((packed));

        struct SVertexTransaction
        {
            ETransactionType type            = ETransactionType::vertex;
            uint8_t remove                   = {};
            uint64_t optional_id             = {};
            char label[CFG_LPG_LABEL_LENGTH] = {};
            uint16_t property_c              = {};

            explicit SVertexTransaction(const uint64_t optional_id = -1, const uint8_t remove = 0, const std::string_view label = "", const uint16_t property_c = 0)
            {
                this->remove      = remove;
                this->optional_id = optional_id;
                strcpy(&this->label[0], label.data());
                this->property_c = property_c;
            }
        } __attribute__((packed));

        struct SEdgeTransaction
        {
            ETransactionType type            = ETransactionType::edge;
            uint8_t remove                   = {};
            uint64_t src                     = {};
            uint64_t dst                     = {};
            char label[CFG_LPG_LABEL_LENGTH] = {};
            uint16_t property_c              = {};

            explicit
            SEdgeTransaction(const uint64_t src = 0, const uint64_t dst = 0, const uint8_t remove = 0, const std::string_view label = "", const uint16_t property_c = 0)
            {
                this->remove = remove;
                this->src    = src;
                this->dst    = dst;
                strcpy(&this->label[0], label.data());
                this->property_c = property_c;
            }
        } __attribute__((packed));

      public:
        CTransaction(const std::filesystem::path & local_path, CMemoryModelLPG * lpg);
        ~CTransaction() = default;

        void close() noexcept;
        void init() noexcept;
        void reset() noexcept;
        void handle_transactions() noexcept;
        void commit_rm_vertex(uint64_t id) noexcept;
        void commit_rm_edge(uint64_t src, uint64_t dst) noexcept;

        void commit_vertex(std::string_view label, const std::vector<std::pair<std::string, std::string>> & props, uint64_t optional_id = -1) noexcept;
        void commit_edge(uint64_t src, uint64_t dst, std::string_view label, const std::vector<std::pair<std::string, std::string>> & props) noexcept;

      private:
        void load();
        void set_up();
        void define_transaction_header();
        void store_transaction_header();
        void read_transaction_header();

        void process_vertex_transaction(const SVertexTransaction &, const std::vector<std::pair<std::string, std::string>> & props) const noexcept;
        void process_edge_transaction(const SEdgeTransaction &, const std::vector<std::pair<std::string, std::string>> & props) const noexcept;

        CMemoryModelLPG * m_lpg;
        CDiskDriver m_transaction_file;
        SHeaderBlock m_header_block {};
        static constexpr int64_t TRANSACTION_FILE_SIZE         = KB(1);
        static constexpr const char * TRANSACTION_FILE_NAME    = "transactions";
        static constexpr int64_t TRANSACTION_HEADER_START_ADDR = 0x00000000;
        static constexpr int64_t TRANSACTIONS_START_ADDR       = sizeof(SHeaderBlock);
    };
} // namespace graphquery::database::storage
