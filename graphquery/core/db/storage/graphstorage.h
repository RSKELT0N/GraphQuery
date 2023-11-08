/************************************************************
* \author Ryan Skelton
* \date 18/09/2023
* \file storage.h
* \brief Storage header file providing an API class to access
*        graph underlying on-disk and in-memory datamodels.
************************************************************/

#pragma once

#include "db/storage/diskdriver.h"

#include <memory>
#include <string>
#include <filesystem>
#include <vector>

namespace graphquery::database::storage
{
    class IGraphDiskStorage
    {
    public:
        IGraphDiskStorage() = default;
        virtual ~IGraphDiskStorage() = default;

        virtual void Load() = 0;
        friend class CGraphStorage;
    };

    class IGraphMemory
    {
    public:
        IGraphMemory() = default;
        virtual ~IGraphMemory() = default;

        virtual void Load() noexcept = 0;
        friend class CGraphStorage;
    };

    class CGraphStorage final
    {
    private:
        //~ Current configuration of database and graph entry.
        //~ Length for a graph entry name
        static constexpr uint8_t GRAPH_NAME_LENGTH = 20;

        struct SGraph_Entry_t
        {
            char graph_name[GRAPH_NAME_LENGTH] = {};
            char graph_master_file_name[GRAPH_NAME_LENGTH] = {};

        } __attribute__((packed));

        struct SDBInfo_t
        {
            uint64_t graph_table_size;
            uint64_t graph_entry_size;
            uint64_t graph_table_start_addr;
        } __attribute__((packed));

        struct SMasterDB_t
        {
            uint64_t magic_check_sum;
            uint64_t version;
            uint64_t timestamp;
            SDBInfo_t db_info;
        } __attribute__((packed));

    public:
        CGraphStorage();
        ~CGraphStorage();

        void Load();
        void Init(std::string_view file_path);
        [[nodiscard]] bool IsExistingDBLoaded() const noexcept;

    private:
        void DefineMasterDBFile() noexcept;
        void DefineGraphTable() noexcept;
        void StoreMasterDBFile() noexcept;
        void StoreGraphTable() noexcept;

    private:

        //~ Instance of the current SDBMaster structure.
        SMasterDB_t m_db_master;
        //~ Array of the existing graphs
        std::unique_ptr<std::vector<SGraph_Entry_t>> m_graph_table;
        //~ Instance of the on-disk loader for the graph db.
        std::unique_ptr<graphquery::database::storage::IGraphDiskStorage> m_graph_loader;
        //~ Instance of the in-memory graph representation for the graph db.
        std::unique_ptr<graphquery::database::storage::IGraphMemory> m_graph_memory;
        //~ Instance of the DiskDriver for the DB master file.
        graphquery::database::storage::CDiskDriver m_disk;
        //~ Bool to check if a current database is loaded.
        bool m_existing_db_loaded;

        //~ Max amount of graph entries
        static constexpr uint8_t GRAPH_ENTRIES_AMT = 5;
        //~ Storage size MAX for database master file.
        static constexpr uint32_t GRAPH_MASTER_FILE_SIZE = (sizeof(SMasterDB_t) + (sizeof(SGraph_Entry_t) * GRAPH_ENTRIES_AMT));
    };
}
