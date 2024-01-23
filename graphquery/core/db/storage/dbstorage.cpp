#include "dbstorage.h"

#include "db/system.h"

#include <fcntl.h>
#include <string_view>
#include <sys/mman.h>

#include <cassert>

graphquery::database::storage::CDBStorage::
CDBStorage():
    m_db_disk(O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED)
{
    m_loaded_graph = std::make_shared<ILPGModel *>();
}

graphquery::database::storage::CDBStorage::~
CDBStorage()
{
    if (m_existing_db_loaded)
        close();
}

void
graphquery::database::storage::CDBStorage::close() noexcept
{
    m_db_superblock = {};

    if (m_existing_db_loaded)
    {
        if (m_existing_graph_loaded)
            close_graph();

        m_db_graph_table.clear();
        m_db_disk.close();
        _log_system->info("Database file has been closed and memory has been flushed");
        m_existing_db_loaded = false;
    }
}

void
graphquery::database::storage::CDBStorage::init(const std::filesystem::path & path, const std::string_view db_name)
{
    if (m_existing_db_loaded)
        close();

    const auto & db_file_name = fmt::format("{}.gdb", db_name);
    const auto & db_path      = std::filesystem::path(path) / db_name;

    if (!CDiskDriver::check_if_folder_exists(db_path.string()))
        CDiskDriver::create_folder(path, db_name);

    m_db_disk.set_path(db_path);

    if (!CDiskDriver::check_if_file_exists(db_path.string(), db_file_name))
        set_up(db_file_name);
    else
        load(db_file_name);
}

void
graphquery::database::storage::CDBStorage::set_up(std::string_view db_name)
{
    _log_system->info(fmt::format("Initialising new database file: {}", db_name));

    CDiskDriver::create_file(m_db_disk.get_path(), db_name, MASTER_DB_FILE_SIZE);
    m_db_disk.open(db_name);

    define_db_superblock();
    define_db_graph_table();
    store_db_superblock();
    store_db_graph_table();

    m_existing_db_loaded = true;
}

void
graphquery::database::storage::CDBStorage::define_db_superblock() noexcept
{
    m_db_superblock    = {};
    SDBInfo_t metadata = {};

    metadata.graph_entry_size       = sizeof(SGraph_Entry_t);
    metadata.graph_table_size       = sizeof(SGraph_Entry_t) * GRAPH_ENTRIES_AMT;
    metadata.graph_table_start_addr = DB_SUPERBLOCK_START_ADDR + sizeof(SDB_Superblock_t);

    m_db_superblock.version         = 1;
    m_db_superblock.db_info         = metadata;
    m_db_superblock.magic_check_sum = 0xF13D00;
    m_db_superblock.timestamp       = std::time(nullptr);
}

void
graphquery::database::storage::CDBStorage::store_db_superblock() noexcept
{
    assert(m_db_disk.check_if_initialised());

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
    assert(m_db_disk.check_if_initialised());

    const auto graph_entry_amt = m_db_superblock.db_info.graph_table_size / m_db_superblock.db_info.graph_entry_size;

    m_db_disk.seek(m_db_superblock.db_info.graph_table_start_addr);
    m_db_disk.write(&m_db_graph_table[0], m_db_superblock.db_info.graph_entry_size, graph_entry_amt);
}

void
graphquery::database::storage::CDBStorage::load(std::string_view db_name)
{
    m_db_disk.open(db_name);
    load_db_superblock();
    load_db_graph_table();

    _log_system->info(fmt::format("Database file ({}) has been loaded into memory", db_name));

    m_existing_db_loaded = true;
}

void
graphquery::database::storage::CDBStorage::load_db_superblock() noexcept
{
    assert(m_db_disk.check_if_initialised() == true);

    m_db_disk.seek(DB_SUPERBLOCK_START_ADDR);
    m_db_disk.read(&m_db_superblock, sizeof(SDB_Superblock_t), 1);
}

void
graphquery::database::storage::CDBStorage::load_db_graph_table() noexcept
{
    assert(m_db_disk.check_if_initialised() == true);

    const auto graph_entry_amt = m_db_superblock.db_info.graph_table_size / m_db_superblock.db_info.graph_entry_size;
    m_db_graph_table.resize(graph_entry_amt);

    m_db_disk.seek(m_db_superblock.db_info.graph_table_start_addr);
    m_db_disk.read(&m_db_graph_table[0], m_db_superblock.db_info.graph_entry_size, graph_entry_amt);
}

std::string
graphquery::database::storage::CDBStorage::get_db_info() const noexcept
{
    assert(m_existing_db_loaded);
    static const auto time           = static_cast<time_t>(m_db_superblock.timestamp);
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
    memcpy(entry.graph_name, name.c_str(), CFG_GRAPH_NAME_LENGTH);
    memcpy(entry.graph_type, type.c_str(), CFG_GRAPH_MODEL_TYPE_LENGTH);

    return entry;
}

bool
graphquery::database::storage::CDBStorage::define_graph_model(const std::string & name, const std::string & type) noexcept
{
    try
    {
        m_graph_model_lib = std::make_unique<dylib>(dylib(fmt::format("{}/{}", PROJECT_ROOT, "lib/models"), type));
        m_graph_model_lib->get_function<void(ILPGModel **)>("create_graph_model")(m_loaded_graph.get());
        (*m_loaded_graph)->init(m_db_disk.get_path().parent_path().string(), name);
        m_existing_graph_loaded = true;
    }
    catch (std::runtime_error & e)
    {
        _log_system->error(fmt::format("Issue linking library and creating memory "
                                       "model of type ({}) Error: {}",
                                       type,
                                       e.what()));
        return false;
    }

    return true;
}

void
graphquery::database::storage::CDBStorage::close_graph() noexcept
{
    (*m_loaded_graph)->save_graph();
    (*m_loaded_graph)->close();
    delete (*m_loaded_graph);
    m_graph_model_lib.reset();
    _log_system->info(fmt::format("Graph has been unloaded from memory and changes have been flushed"));
    m_existing_graph_loaded = false;
}

void
graphquery::database::storage::CDBStorage::create_graph(const std::string & name, const std::string & type) noexcept
{
    if (check_if_graph_exists(name))
    {
        _log_system->warning("Cannot create graph that already exists, use open_graph function");
        return;
    }

    if (m_existing_db_loaded)
    {
        if (*m_loaded_graph)
            close_graph();

        if (define_graph_model(name, type))
        {
            create_graph_entry(name, type);
            _log_system->info(fmt::format("Graph [{}] of memory model type "
                                          "[{}] has been created and opened",
                                          name,
                                          type));
        }
    }
    else
        _log_system->warning("Database has not been loaded for a graph to added");
}

const std::vector<graphquery::database::storage::CDBStorage::SGraph_Entry_t> &
graphquery::database::storage::CDBStorage::get_graph_table() const noexcept
{
    return m_db_graph_table;
}

std::shared_ptr<graphquery::database::storage::ILPGModel *>
graphquery::database::storage::CDBStorage::get_graph() const noexcept
{
    return m_loaded_graph;
}

const bool &
graphquery::database::storage::CDBStorage::get_is_db_loaded() const noexcept
{
    return m_existing_db_loaded;
}

const bool &
graphquery::database::storage::CDBStorage::get_is_graph_loaded() const noexcept
{
    return m_existing_graph_loaded;
}

void
graphquery::database::storage::CDBStorage::open_graph(std::string name, std::string type) noexcept
{
    if (!check_if_graph_exists(name))
    {
        _log_system->warning("Cannot open graph that does not exist, initialise graph first");
        return;
    }

    if (m_existing_db_loaded)
    {
        if (m_existing_graph_loaded)
            close_graph();

        if (define_graph_model(name, type))
        {
            _log_system->info(fmt::format("Opening Graph [{}] of memory model "
                                          "type [{}] as the current context",
                                          name,
                                          type));
        }
    }
    else
        _log_system->warning("Database has not been loaded for a graph to opened");
}

bool
graphquery::database::storage::CDBStorage::check_if_graph_exists(std::string_view graph_name) const noexcept
{
    const auto & exists = std::find_if(m_db_graph_table.begin(),
                                       m_db_graph_table.end(),
                                       [&graph_name](const SGraph_Entry_t & entry) { return strncmp(entry.graph_name, graph_name.data(), CFG_GRAPH_NAME_LENGTH) == 0; });

    return exists != m_db_graph_table.end();
}

void
graphquery::database::storage::CDBStorage::create_graph_entry(const std::string & name, const std::string & type) noexcept
{
    m_db_graph_table.emplace_back(define_graph_entry(name, type));
    m_db_superblock.db_info.graph_table_size += m_db_superblock.db_info.graph_entry_size;

    store_db_superblock();
    store_db_graph_table();
}
