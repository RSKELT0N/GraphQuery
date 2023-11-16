/************************************************************
* \author Ryan Skelton
* \date 18/09/2023
* \file dbstorage.h
* \brief Storage header file providing an API class to access
*        graph underlying on-disk and in-memory datamodels.
************************************************************/

#pragma once

#include "db/storage/diskdriver.h"
#include "db/storage/config.h"

#include <filesystem>
#include <vector>

namespace graphquery::database::storage
{
    class CDBStorage final
    {
    private:
        struct SGraph_Entry_t
        {
            char graph_name[GRAPH_NAME_LENGTH] = {};
            char graph_type[GRAPH_MODEL_TYPE_LENGTH] = {};

        } __attribute__((packed));

        struct SDBInfo_t
        {
            int64_t graph_table_size = {};
            int64_t graph_entry_size = {};
            int64_t graph_table_start_addr = {};
        } __attribute__((packed));

        struct SDB_Superblock_t
        {
            int64_t magic_check_sum = {};
            int64_t version = {};
            int64_t timestamp = {};
            SDBInfo_t db_info = {};
        } __attribute__((packed));

    public:
        CDBStorage();
        ~CDBStorage();

        void Close() noexcept;
        void Load(std::string file_path);
        void SetUp(std::string file_path);
        void Init(std::string_view file_path);
        void CreateGraph(std::string name, std::string type) noexcept;
        void OpenGraph(std::string name, std::string type) noexcept;
        [[nodiscard]] bool IsExistingDBLoaded() const noexcept;
        [[nodiscard]] const std::string GetDBInfo() const noexcept;

    private:
        void StoreDBGraphTable() noexcept;
        void LoadDBGraphTable() noexcept;
        void DefineDBGraphTable() noexcept;

        void StoreDBSuperblock() noexcept;
        void LoadDBSuperblock() noexcept;
        void DefineDBSuperblock() noexcept;

        SGraph_Entry_t DefineGraph(std::string name, std::string type) noexcept;

        //~ Bool to check if a current database is loaded.
        bool m_existing_db_loaded = false;

        //~ Instance of the DiskDriver for the DB master file.
        CDiskDriver m_db_disk;
        //~ Instance of the current SDBMaster structure.
        SDB_Superblock_t m_db_superblock = {};
        //~ Array of the existing graphs
        std::vector<SGraph_Entry_t> m_db_graph_table = {};

        //~ Max amount of graph entries
        static constexpr uint8_t GRAPH_ENTRIES_AMT = 0;
        //~ MasterDB struct entry;
        static constexpr uint64_t DB_SUPERBLOCK_START_ADDR = 0x0;
        //~ Storage size MAX for database master file.
        static constexpr uint32_t MASTER_DB_FILE_SIZE = (sizeof(SDB_Superblock_t) + (sizeof(SGraph_Entry_t) * GRAPH_ENTRIES_AMT));
    };
}
