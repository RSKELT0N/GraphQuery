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

#include "db/storage/diskdriver/diskdriver.h"

#include <filesystem>

namespace graphquery::database::storage
{
    class CTransaction
    {
    private:
        static constexpr int32_t TRANSACTION_ACTION_SIZE = 200;
        static constexpr int32_t TRANSACTION_LOG_SIZE    = 500;

        struct SHeaderBlock
        {
            int32_t transaction_size;
            int32_t transaction_c;
            int32_t action_size;
            int32_t log_size;
        } __attribute__((packed));

        struct STransaction
        {
            char action[TRANSACTION_ACTION_SIZE] = {};
            char bytes[TRANSACTION_ACTION_SIZE]  = {};
        } __attribute__((packed));

    public:
        explicit CTransaction(const std::filesystem::path & local_path);
        ~CTransaction() = default;

        void init() noexcept;

      private:
        template<typename T>
        void store_transaction(const std::string & action, T log)
        {
            static_assert(sizeof(log) < TRANSACTION_LOG_SIZE);

            STransaction transaction = {};
            strncpy(&transaction.action[0], action.c_str(), TRANSACTION_ACTION_SIZE);
            memcpy(&transaction.bytes[0], &log, sizeof(log));


        }

        void load();
        void set_up();
        void define_transaction_header();
        void store_transaction_header();

        void read_transaction_header();
        void read_transactions();

        SHeaderBlock m_header_block;
        std::vector<STransaction> m_transactions;

        CDiskDriver m_transaction_file;
        const std::filesystem::path m_path;

        static constexpr int64_t TRANSACTION_FILE_SIZE     = KB(1);
        static constexpr std::string TRANSACTION_FILE_NAME = "transactions";
    };
} // namespace graphquery::database::storage
