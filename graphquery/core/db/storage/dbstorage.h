/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file dbstorage.h
 * \brief Storage header file providing an API class to access
 *        graph underlying on-disk and in-memory datamodels.
 ************************************************************/

#pragma once

#include "dylib.hpp"
#include "db/storage/config.h"
#include "graph_model.h"

#include <string_view>
#include <vector>

namespace graphquery::database::storage
{
    class CDBStorage final
    {
      public:
        struct SGraph_Entry_t
        {
            char graph_name[CFG_GRAPH_NAME_LENGTH]       = {};
            char graph_type[CFG_GRAPH_MODEL_TYPE_LENGTH] = {};
        } __attribute__((packed));

        struct SDBInfo_t
        {
            int64_t graph_table_size       = {};
            int64_t graph_entry_size       = {};
            int64_t graph_table_start_addr = {};
        } __attribute__((packed));

        struct SDB_Superblock_t
        {
            int64_t magic_check_sum = {};
            int64_t version         = {};
            int64_t timestamp       = {};
            SDBInfo_t db_info       = {};
        } __attribute__((packed));

        CDBStorage();
        ~CDBStorage();
        void close() noexcept;
        void load(std::string_view db_name);
        void set_up(std::string_view db_name);
        void init(std::filesystem::path file_path);
        void create_graph(const std::string & name, const std::string & type) noexcept;
        void open_graph(std::string name, std::string type) noexcept;

        void close_graph() noexcept;
        [[nodiscard]] const std::vector<SGraph_Entry_t> & get_graph_table() const noexcept;
        [[nodiscard]] std::shared_ptr<ILPGModel *> get_graph() const noexcept;
        [[nodiscard]] const bool & get_is_db_loaded() const noexcept;
        [[nodiscard]] const bool & get_is_graph_loaded() const noexcept;
        [[nodiscard]] std::string get_db_info() const noexcept;
        void test() noexcept;

      private:
        void store_db_graph_table() noexcept;
        void load_db_graph_table() noexcept;
        void define_db_graph_table() noexcept;
        void store_db_superblock() noexcept;
        void load_db_superblock() noexcept;
        void define_db_superblock() noexcept;

        SGraph_Entry_t define_graph(std::string name, std::string type) noexcept;
        [[nodiscard]] SGraph_Entry_t define_graph_entry(const std::string & name, const std::string & type) noexcept;
        [[nodiscard]] bool define_graph_model(const std::string & name, const std::string & type) noexcept;
        void create_graph_entry(const std::string & name, const std::string & type) noexcept;

        CDiskDriver m_db_disk;                             //~ Instance of the DiskDriver for the DB master file.
        SDB_Superblock_t m_db_superblock             = {}; //~ Instance of the current SDBMaster structure.
        std::vector<SGraph_Entry_t> m_db_graph_table = {}; //~ Array of the existing graphs
        std::shared_ptr<ILPGModel *> m_loaded_graph  = {}; //~ Instance of the currently linked graph model.
        std::unique_ptr<dylib> m_graph_model_lib;          //~ Library of the currently loaded graph model.

        bool m_existing_db_loaded                          = false;        //~ Bool to check if a current database is loaded.
        bool m_existing_graph_loaded                       = false;        //~ Bool to check if a current graph is loaded.
        static constexpr uint8_t GRAPH_ENTRIES_AMT         = 0;            //~ Max amount of graph entries
        static constexpr uint64_t DB_SUPERBLOCK_START_ADDR = 0x0;          //~ MasterDB struct entry;
        static constexpr const char * MASTER_DB_FILE_NAME  = "master.gdb"; //~ Storage size MAX for database master file.
        static constexpr uint32_t MASTER_DB_FILE_SIZE = (sizeof(SDB_Superblock_t) + (sizeof(SGraph_Entry_t) * GRAPH_ENTRIES_AMT)); //~ Storage size MAX for database master
        // file.
    };
} // namespace graphquery::database::storage
