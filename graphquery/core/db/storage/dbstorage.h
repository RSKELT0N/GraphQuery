/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file dbstorage.h
 * \brief Storage header file providing an API class to access
 *        graph underlying on-disk and in-memory datamodels.
 ************************************************************/

#pragma once

#include "dataset.h"
#include "dylib.hpp"
#include "graph_model.h"
#include "db/storage/config.h"
#include "diskdriver/memory_ref.h"

#include <string_view>

namespace graphquery::database::storage
{
    class CDBStorage final
    {
      public:
        struct SGraph_Entry_t
        {
            char graph_name[CFG_GRAPH_NAME_LENGTH]       = {};
            char graph_type[CFG_GRAPH_MODEL_TYPE_LENGTH] = {};

            SGraph_Entry_t() = default;
            SGraph_Entry_t(const std::string_view name, const std::string_view type)
            {
                strncpy(&graph_name[0], name.data(), CFG_GRAPH_NAME_LENGTH);
                strncpy(&graph_type[0], type.data(), CFG_GRAPH_MODEL_TYPE_LENGTH);
            }
        };

        struct SDBInfo_t
        {
            int64_t graph_entry_size       = {};
            int64_t graph_table_start_addr = {};
            uint8_t graph_table_c          = {};
        };

        struct SDB_Superblock_t
        {
            int64_t magic_check_sum = {};
            int64_t version         = {};
            int64_t timestamp       = {};
            SDBInfo_t db_info       = {};
        };

        CDBStorage();
        ~CDBStorage();
        void close() noexcept;
        void load(std::string_view db_name);
        void set_up(std::string_view db_name);
        void open_graph(std::string_view name) noexcept;
        void init(const std::filesystem::path & path, std::string_view db_name);
        void create_graph(std::string_view name, std::string_view type) noexcept;
        void load_dataset(std::filesystem::path dataset_path) const noexcept;
        [[nodiscard]] bool check_if_graph_exists(std::string_view graph_name) const noexcept;

        void close_graph() noexcept;
        [[nodiscard]] std::string get_db_info() noexcept;
        [[nodiscard]] std::shared_ptr<ILPGModel *> get_graph() const noexcept;
        [[nodiscard]] const bool & get_is_db_loaded() const noexcept;
        [[nodiscard]] const bool & get_is_graph_loaded() const noexcept;
        [[nodiscard]] const std::unordered_map<std::string, SGraph_Entry_t> & get_graph_table() const noexcept;

      private:
        void store_db_superblock() noexcept;
        void store_graph_entry(const SGraph_Entry_t & entry) noexcept;
        SRef_t<SDB_Superblock_t> read_db_superblock() noexcept;
        SRef_t<SGraph_Entry_t> read_graph_entry(uint8_t entry_offset) noexcept;

        void define_graph_map() noexcept;
        [[nodiscard]] bool define_graph_model(std::string_view name, std::string_view type) noexcept;

        CDiskDriver m_db_file;                                 //~ Instance of the DiskDriver for the DB master file.
        std::unique_ptr<CDataset> m_dataset_loader;            //~ Instance of the dataset loader.
        std::unique_ptr<dylib> m_graph_model_lib    = nullptr; //~ Library of the currently loaded graph model.
        std::shared_ptr<ILPGModel *> m_loaded_graph = {};      //~ Instance of the currently linked graph model.

        bool m_existing_db_loaded    = false;                              //~ Bool to check if a current database is loaded.
        bool m_existing_graph_loaded = false;                              //~ Bool to check if a current graph is loaded.
        std::unordered_map<std::string, SGraph_Entry_t> m_graph_entry_map; //~ mapping graph name to entry record.

        static constexpr uint8_t GRAPH_ENTRIES_AMT         = 0;            //~ Max amount of graph entries
        static constexpr uint64_t DB_SUPERBLOCK_START_ADDR = 0x0;          //~ MasterDB struct entry;
        static constexpr const char * MASTER_DB_FILE_NAME  = "master.gdb"; //~ Storage size MAX for database master file.
    };
} // namespace graphquery::database::storage
