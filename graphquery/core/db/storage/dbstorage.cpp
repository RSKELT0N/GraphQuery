#include "dbstorage.h"

#include "db/system.h"

#include <cassert>
#include <sys/fcntl.h>
#include <sys/mman.h>

graphquery::database::storage::CDBStorage::CDBStorage() : m_db_disk(O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED) {}

graphquery::database::storage::CDBStorage::~CDBStorage()
{
    if(m_existing_db_loaded)
        close();
}

void
graphquery::database::storage::CDBStorage::close() noexcept
{
    m_db_superblock = {};

    if(m_existing_db_loaded)
    {
        m_db_graph_table.clear();
        m_db_disk.close();
        _log_system->info("Database file has been closed and memory has been flushed");
        m_existing_db_loaded = false;
    }
}

void
graphquery::database::storage::CDBStorage::init(std::string_view file_path)
{
    if(m_existing_db_loaded)
        close();

    if(!m_db_disk.check_if_file_exists(file_path))
        setUp(file_path.cbegin());
    else load(file_path.cbegin());
}

void
graphquery::database::storage::CDBStorage::setUp(std::string file_path)
{
    _log_system->info(fmt::format("Initialising new database file: {}", file_path));
    m_db_disk.set_file_path(file_path);
    m_db_disk.create(MASTER_DB_FILE_SIZE);
    m_db_disk.open();

    define_db_superblock();
    define_db_graph_table();
    store_db_superblock();
    store_db_graph_table();

    m_existing_db_loaded = true;
}

void
graphquery::database::storage::CDBStorage::define_db_superblock() noexcept
{
    m_db_superblock = {};
    SDBInfo_t metadata = {};

    metadata.graph_entry_size = sizeof(SGraph_Entry_t);
    metadata.graph_table_size = sizeof(SGraph_Entry_t) * GRAPH_ENTRIES_AMT;
    metadata.graph_table_start_addr = DB_SUPERBLOCK_START_ADDR + sizeof(SDB_Superblock_t);

    m_db_superblock.version = 1;
    m_db_superblock.db_info = metadata;
    m_db_superblock.magic_check_sum = 0xF13D00;
    m_db_superblock.timestamp = std::time(nullptr);
}

void
graphquery::database::storage::CDBStorage::store_db_superblock() noexcept
{
    assert(m_db_disk.CheckIfInitialised());

    m_db_disk.seek(0);
    m_db_disk.write(&this->m_db_superblock, sizeof(SDB_Superblock_t), 1);
}

void
graphquery::database::storage::CDBStorage::define_db_graph_table() noexcept
{
    m_db_graph_table.clear();
}

void
graphquery::database::storage::CDBStorage::store_db_graph_table() noexcept
{
    assert(m_db_disk.CheckIfInitialised());

    const auto graph_entry_amt = m_db_superblock.db_info.graph_table_size / m_db_superblock.db_info.graph_entry_size;

    m_db_disk.seek(m_db_superblock.db_info.graph_table_start_addr);
    m_db_disk.write(&m_db_graph_table[0], m_db_superblock.db_info.graph_entry_size, graph_entry_amt);
}

void
graphquery::database::storage::CDBStorage::load(std::string file_path)
{
    m_db_disk.set_file_path(file_path);
    m_db_disk.open();
    load_db_superblock();
    load_db_graph_table();
    _log_system->info(fmt::format("Database file ({}) has been loaded into memory", file_path));

    m_existing_db_loaded = true;
}

void
graphquery::database::storage::CDBStorage::load_db_superblock() noexcept
{
    assert(m_db_disk.CheckIfInitialised() == true);

    m_db_disk.seek(DB_SUPERBLOCK_START_ADDR);
    m_db_disk.read(&m_db_superblock, sizeof(SDB_Superblock_t), 1);
}

void
graphquery::database::storage::CDBStorage::load_db_graph_table() noexcept
{
    assert(m_db_disk.CheckIfInitialised() == true);

    const auto graph_entry_amt = m_db_superblock.db_info.graph_table_size / m_db_superblock.db_info.graph_entry_size;
    m_db_graph_table.resize(graph_entry_amt);

    m_db_disk.seek(m_db_superblock.db_info.graph_table_start_addr);
    m_db_disk.read(&m_db_graph_table[0], m_db_superblock.db_info.graph_entry_size, graph_entry_amt);
}

const std::string
graphquery::database::storage::CDBStorage::get_db_info() const noexcept
{
    assert(m_existing_db_loaded);
    static const auto time = static_cast<time_t>(m_db_superblock.timestamp);
    static const auto time_formatted = ctime(&time);

    return fmt::format("Version: {}\nDate Created: {}CheckSum: {}\nGraphs: {}\n",
                           m_db_superblock.version,
                           time_formatted,
                           m_db_superblock.magic_check_sum,
                           m_db_superblock.db_info.graph_table_size / m_db_superblock.db_info.graph_entry_size);
}

graphquery::database::storage::CDBStorage::SGraph_Entry_t
graphquery::database::storage::CDBStorage::define_graph_entry(const std::string & name, const std::string & type) noexcept
{
    SGraph_Entry_t entry;
    memcpy(entry.graph_name, name.c_str(), GRAPH_NAME_LENGTH);
    memcpy(entry.graph_type, type.c_str(), GRAPH_MODEL_TYPE_LENGTH);

    return entry;
}

bool
graphquery::database::storage::CDBStorage::define_graph_model(const std::string & name, const std::string & type) noexcept
{
    try
    {
        m_data_model_lib = std::make_unique<dylib>(dylib(fmt::format("{}/{}", PROJECT_ROOT, "lib/models"), type));
        m_data_model_lib->get_function<void(std::unique_ptr<IGraphModel> &)>("create_graph_model")(m_loaded_graph);
        m_loaded_graph->init(name);
    }
    catch(std::runtime_error & e)
    {
        _log_system->error(fmt::format("Issue linking library and creating graph model of type ({}) Error: {}", type, e.what()));
        return false;
    }

    return true;
}

void
graphquery::database::storage::CDBStorage::close_graph() noexcept
{
    m_loaded_graph->Close();
    m_loaded_graph.reset();
    m_data_model_lib.reset();
    _log_system->info(fmt::format("Graph has been unloaded from memory and changes have been flushed"));
}

void
graphquery::database::storage::CDBStorage::create_graph(const std::string & name, const std::string & type) noexcept
{
    if(m_existing_db_loaded)
    {
        if(m_loaded_graph)
            close_graph();

        if(define_graph_model(name, type))
        {
            create_graph_entry(name, type);
            _log_system->info(fmt::format("Graph [{}] of model type [{}] has been added and opened", name, type));
        }

    } else _log_system->warning("Database has not been loaded for a graph to added");
}

const std::vector<graphquery::database::storage::CDBStorage::SGraph_Entry_t> &
graphquery::database::storage::CDBStorage::get_graph_table() const noexcept
{
    return m_db_graph_table;
}

const bool&
graphquery::database::storage::CDBStorage::get_is_db_loaded() const noexcept
{
    return m_existing_db_loaded;
}

void
graphquery::database::storage::CDBStorage::open_graph(std::string name, std::string type) noexcept
{
    if(m_existing_db_loaded)
    {
        if(m_loaded_graph)
            close_graph();

        if(define_graph_model(name, type))
            _log_system->info(fmt::format("Opening Graph [{}] of model type [{}] as the current context", name, type));
    } else _log_system->warning("Database has not been loaded for a graph to opened");
}

void
graphquery::database::storage::CDBStorage::create_graph_entry(const std::string & name, const std::string & type) noexcept
{
    m_db_graph_table.emplace_back(define_graph_entry(name, type));
    m_db_superblock.db_info.graph_table_size += m_db_superblock.db_info.graph_entry_size;

    store_db_superblock();
    store_db_graph_table();
}
