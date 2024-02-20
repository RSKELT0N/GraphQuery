#include "dbstorage.h"

#include "db/utils/lib.h"
#include "dataset_ldbc.hpp"
#include "db/system.h"

#include <string_view>
#include <cassert>
#include <cstdint>
#include <cstdint>
#include <cstdint>
#include <cstdint>

graphquery::database::storage::CDBStorage::
CDBStorage()
{
    m_loaded_graph   = std::make_shared<ILPGModel *>();
    m_dataset_loader = std::make_unique<CDatasetLDBC>(m_loaded_graph);
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
    if (m_existing_db_loaded)
    {
        if (m_existing_graph_loaded)
            close_graph();

        m_existing_db_loaded = false;
        m_db_file.close();
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

    m_db_file.set_path(db_path);

    if (!CDiskDriver::check_if_file_exists(db_path.string(), db_file_name))
        set_up(db_file_name);
    else
        load(db_file_name);
}

void
graphquery::database::storage::CDBStorage::set_up(std::string_view db_name)
{
    _log_system->info(fmt::format("Initialising new database file: {}", db_name));

    CDiskDriver::create_file(m_db_file.get_path(), db_name);
    m_db_file.open(db_name);

    store_db_superblock();
    define_graph_map();
    m_existing_db_loaded = true;
}

void
graphquery::database::storage::CDBStorage::store_db_superblock() noexcept
{
    SRef_t<SDB_Superblock_t> superblock_ptr = read_db_superblock();

    superblock_ptr->version                        = 1;
    superblock_ptr->magic_check_sum                = 0xF13D00;
    superblock_ptr->timestamp                      = std::time(nullptr);
    superblock_ptr->db_info.graph_table_c          = 0;
    superblock_ptr->db_info.graph_entry_size       = sizeof(SGraph_Entry_t);
    superblock_ptr->db_info.graph_table_start_addr = DB_SUPERBLOCK_START_ADDR + sizeof(SDB_Superblock_t);
}

void
graphquery::database::storage::CDBStorage::store_graph_entry(const SGraph_Entry_t & entry) noexcept
{
    assert(m_db_file.check_if_initialised());
    const uint8_t entry_idx = read_db_superblock()->db_info.graph_table_c++;
    auto graph_entry_ptr    = read_graph_entry(entry_idx);

    strncpy(&graph_entry_ptr->graph_name[0], entry.graph_name, CFG_GRAPH_NAME_LENGTH);
    strncpy(&graph_entry_ptr->graph_type[0], entry.graph_type, CFG_GRAPH_MODEL_TYPE_LENGTH);
    m_graph_entry_map.emplace(entry.graph_name, entry);
}

void
graphquery::database::storage::CDBStorage::load(std::string_view db_name)
{
    m_db_file.open(db_name);
    define_graph_map();

    m_existing_db_loaded = true;
    _log_system->info(fmt::format("Database file ({}) has been loaded into memory", db_name));
}

graphquery::database::storage::SRef_t<graphquery::database::storage::CDBStorage::SDB_Superblock_t>
graphquery::database::storage::CDBStorage::read_db_superblock() noexcept
{
    assert(m_db_file.check_if_initialised() == true);
    return m_db_file.ref<SDB_Superblock_t>(DB_SUPERBLOCK_START_ADDR);
}

graphquery::database::storage::SRef_t<graphquery::database::storage::CDBStorage::SGraph_Entry_t>
graphquery::database::storage::CDBStorage::read_graph_entry(const uint8_t entry_offset) noexcept
{
    assert(m_db_file.check_if_initialised() == true);
    static const int64_t base_addr  = read_db_superblock()->db_info.graph_table_start_addr;
    static const int64_t entry_size = read_db_superblock()->db_info.graph_entry_size;
    const int64_t effective_addr    = base_addr + (entry_size * entry_offset);
    return m_db_file.ref<SGraph_Entry_t>(effective_addr);
}

std::string
graphquery::database::storage::CDBStorage::get_db_info() noexcept
{
    assert(m_existing_db_loaded);
    auto superblock_ptr              = read_db_superblock();
    static const time_t time         = superblock_ptr->timestamp;
    static const auto time_formatted = ctime(&time);

    return fmt::format("Version: {}\nDate Created: {}CheckSum: {}\nGraphs: {}\n", superblock_ptr->version, time_formatted, superblock_ptr->magic_check_sum, superblock_ptr->db_info.graph_table_c);
}

void
graphquery::database::storage::CDBStorage::define_graph_map() noexcept
{
    const auto entry_c = read_db_superblock()->db_info.graph_table_c;
    m_graph_entry_map.reserve(entry_c);

    SRef_t<SGraph_Entry_t> curr_entry_ptr = read_graph_entry(0);

    for (int8_t i = 0; i < entry_c; i++, ++curr_entry_ptr)
        m_graph_entry_map[curr_entry_ptr->graph_name] = SGraph_Entry_t(curr_entry_ptr->graph_name, curr_entry_ptr->graph_type);
}

bool
graphquery::database::storage::CDBStorage::define_graph_model(const std::string_view name, const std::string_view type) noexcept
{
    try
    {
        m_graph_model_lib = std::make_unique<dylib>(dylib(fmt::format("{}/{}", PROJECT_ROOT, "lib/models"), type.data()));
        m_graph_model_lib->get_function<void(ILPGModel **, const std::shared_ptr<logger::CLogSystem> &)>("create_graph_model")(m_loaded_graph.get(), _log_system);
        (*m_loaded_graph)->init(m_db_file.get_path().parent_path().string(), name);

        m_existing_graph_loaded = true;
    }
    catch (std::runtime_error & e)
    {
        _log_system->error(fmt::format("Issue linking library and creating memory model of type ({}) Error: {}", type, e.what()));
        return false;
    }

    return true;
}

void
graphquery::database::storage::CDBStorage::close_graph() noexcept
{
    m_existing_graph_loaded = false;
    delete *m_loaded_graph;
    m_graph_model_lib.reset();
    _log_system->info(fmt::format("Graph has been unloaded from memory and changes have been synced"));
}

void
graphquery::database::storage::CDBStorage::create_graph(const std::string_view name, const std::string_view type) noexcept
{
    if (check_if_graph_exists(name))
    {
        _log_system->warning("Cannot create graph that already exists, use open_graph function");
        return;
    }

    if (m_existing_db_loaded)
    {
        if (m_existing_graph_loaded)
            close_graph();

        const auto [defined, elapsed] = utils::measure<bool>(&CDBStorage::define_graph_model, this, name.data(), type.data());

        if (defined)
        {
            store_graph_entry(SGraph_Entry_t(name, type.data()));
            _log_system->info(fmt::format("Graph [{}] of memory model type [{}] has been created and opened within {}s", name, type, elapsed.count()));
        }
    }
    else
        _log_system->warning("Database has not been loaded for a graph to added");
}

void
graphquery::database::storage::CDBStorage::load_dataset(std::filesystem::path dataset_path) const noexcept
{
    if (!m_existing_graph_loaded)
    {
        _log_system->warning("Cannot load any dataset to a non-existing graph loaded.");
        return;
    }
    m_dataset_loader->set_path(dataset_path);
    const auto [elapsed] = utils::measure(&CDataset::load, m_dataset_loader.get());

    _log_system->info(fmt::format("Dataset has been inserted into loaded graph within {}s", elapsed.count()));
}

const std::unordered_map<std::string, graphquery::database::storage::CDBStorage::SGraph_Entry_t> &
graphquery::database::storage::CDBStorage::get_graph_table() const noexcept
{
    return m_graph_entry_map;
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
graphquery::database::storage::CDBStorage::open_graph(const std::string_view name) noexcept
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

        const auto graph_entry        = m_graph_entry_map.at(name.data());
        const auto [defined, elapsed] = utils::measure<bool>(&CDBStorage::define_graph_model, this, name.data(), graph_entry.graph_type);

        if (defined)
            _log_system->info(fmt::format("Opening Graph [{}] of memory model type [{}] as the current context within {}s", name, graph_entry.graph_type, elapsed.count()));
    }
    else
        _log_system->warning("Database has not been loaded for a graph to opened");
}

bool
graphquery::database::storage::CDBStorage::check_if_graph_exists(const std::string_view graph_name) const noexcept
{
    return m_graph_entry_map.contains(graph_name.data());
}
