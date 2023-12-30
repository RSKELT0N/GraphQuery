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
#include "db/storage/diskdriver/diskdriver.h"

#include <filesystem>
#include <db/storage/config.h>

namespace graphquery::database::storage
{
    class CTransaction
    {
      private:
        enum class ETransactionType : uint8_t
        {
            vertex,
            edge
        };

        struct SHeaderBlock
        {
            int32_t transaction_c;
            int64_t eof_addr;
        } __attribute__((packed));

        struct SVertexTransaction
        {
            ETransactionType type        = ETransactionType::vertex;
            char label[LPG_LABEL_LENGTH] = {};
            int64_t property_c           = {};
        } __attribute__((packed));

        struct SEdgeTransaction
        {
            ETransactionType type        = ETransactionType::edge;
            int64_t src                  = {};
            int64_t dst                  = {};
            char label[LPG_LABEL_LENGTH] = {};
            int64_t property_c           = {};
        } __attribute__((packed));

      public:
        CTransaction(const std::filesystem::path & local_path, CMemoryModelLPG * lpg);
        ~CTransaction() = default;

        void init() noexcept;
        void handle_transactions() noexcept;
        void commit_vertex(std::string_view label, const std::vector<std::pair<std::string, std::string>> & props) noexcept;
        void commit_edge(int64_t src, int64_t dst, std::string_view label, const std::vector<std::pair<std::string, std::string>> & props) noexcept;

      private:
        void load();
        void set_up();
        void define_transaction_header();
        void store_transaction_header();
        void read_transaction_header();

        CMemoryModelLPG * m_lpg;
        CDiskDriver m_transaction_file;
        SHeaderBlock m_header_block{};
        static constexpr int64_t TRANSACTION_FILE_SIZE      = KB(1);
        static constexpr const char * TRANSACTION_FILE_NAME = "transactions";
        static constexpr int64_t TRANSACTION_HEADER_START_ADDR = 0x00000000;
    };
} // namespace graphquery::database::storage
