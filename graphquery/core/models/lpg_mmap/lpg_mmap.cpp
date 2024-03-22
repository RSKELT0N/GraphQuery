#include "lpg_mmap.h"

#include "db/utils/lib.h"

#include <cassert>
#include <string_view>
#include <optional>
#include <vector>

graphquery::database::storage::CMemoryModelMMAPLPG::CMemoryModelMMAPLPG(const std::shared_ptr<logger::CLogSystem> & log_system, const bool & sync_state_):
    ILPGModel(log_system, sync_state_), m_syncing(0), m_transaction_ref_c(0)
{
    m_label_vertex = std::vector<std::vector<Id_t>>();
    m_unq_lock     = std::unique_lock(m_sync_lock);
}

graphquery::database::storage::CMemoryModelMMAPLPG::~
CMemoryModelMMAPLPG()
{
    sync_graph();
    close();
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::close() noexcept
{
    m_transactions->close();
    m_master_file.close();
}

std::string_view
graphquery::database::storage::CMemoryModelMMAPLPG::get_name() noexcept
{
    return read_graph_metadata()->graph_name;
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::create_rollback(const std::string_view name) noexcept
{
    m_transactions->store_rollback_entry(name);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::rollback(const uint8_t rollback_entry) noexcept
{
    m_cv_sync.wait(m_unq_lock, wait_on_transactions);
    utils::atomic_store(&m_syncing, 1);
    // ~ Reset graph to initial state.
    reset_graph();
    // ~ Rollback graph based on the entry index.
    auto r_ptr              = m_transactions->read_rollback_entry(rollback_entry);
    const auto rollback_eor = r_ptr->eor_addr;

    m_log_system->info(fmt::format("Starting db rollback ({})", r_ptr->name));
    const auto & [elapsed] = utils::measure(&CTransaction::rollback, m_transactions, rollback_eor);
    m_log_system->info(fmt::format("Rollback has completed within {}s", elapsed.count()));
    utils::atomic_store(&m_syncing, 0);
    m_cv_sync.notify_all();
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::rollback() noexcept
{
    m_cv_sync.wait(m_unq_lock, wait_on_transactions);
    utils::atomic_store(&m_syncing, 1);
    // ~ Reset graph to initial state.
    reset_graph();

    // ~ Rollback graph based on the end valid address.
    const auto rollback_eor = m_transactions->get_valid_eor_addr();
    m_transactions->rollback(rollback_eor);
    utils::atomic_store(&m_syncing, 0);
    m_cv_sync.notify_all();
}

std::vector<std::string>
graphquery::database::storage::CMemoryModelMMAPLPG::fetch_rollback_table() const noexcept
{
    return m_transactions->fetch_rollback_table();
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::sync_graph() noexcept
{
    if (read_graph_metadata()->flush_needed)
    {
        m_cv_sync.wait(m_unq_lock, wait_on_transactions);
        utils::atomic_store(&m_syncing, 1);

        if (read_graph_metadata()->prune_needed)
        {
            persist_graph_changes();
            utils::atomic_store(&read_graph_metadata()->prune_needed, false);
        }

        utils::atomic_store(&m_syncing, 0);
        m_cv_sync.notify_all();

        utils::atomic_store(&read_graph_metadata()->flush_needed, false);
        m_log_system->debug("Graph has been synced");
    }
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::create_graph(std::filesystem::path path, const std::string_view graph) noexcept
{
    this->m_graph_name = graph;
    this->m_graph_path = path;

    //~ Create initial model files
    (void) CDiskDriver::create_file(path, MASTER_FILE_NAME);

    setup_files(path, true);

    //~ Initialise graph memory.
    m_transactions->init();

    store_graph_metadata();
    m_index_file.store_metadata();
    m_vertices_file.store_metadata();
    m_edges_file.store_metadata();
    m_properties_file.store_metadata();
    m_label_ref_file.store_metadata();
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::load_graph(const std::filesystem::path path, const std::string_view graph) noexcept
{
    this->m_graph_name = graph;
    this->m_graph_path = path;

    setup_files(path, false);

    //~ Load graph memory.
    m_transactions->init();
    read_index_list();
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::reset_graph() noexcept
{
    // ~ Reset master file
    m_master_file.resize_override(CDiskDriver::DEFAULT_FILE_SIZE);
    m_master_file.clear_contents();
    store_graph_metadata();

    // ~ Reset graph data
    m_vertices_file.reset();
    m_edges_file.reset();
    m_index_file.reset();
    m_properties_file.reset();
    m_label_ref_file.reset();

    // ~ Reset running in-memory data
    m_label_vertex.clear();
    m_v_label_map.clear();
    m_e_label_map.clear();
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::setup_files(const std::filesystem::path & path, const bool initialise) noexcept
{
    //~ Set path for master file and initialise transactions
    m_master_file.set_path(path);
    m_master_file.open(MASTER_FILE_NAME);
    m_transactions = std::make_shared<CTransaction>(path, this, m_log_system, _sync_state_);

    //~ Open mapping to model files
    m_index_file.open(path, INDEX_FILE_NAME, initialise);
    m_vertices_file.open(path, VERTICES_FILE_NAME, initialise);
    m_edges_file.open(path, EDGES_FILE_NAME, initialise);
    m_properties_file.open(path, PROPERTIES_FILE_NAME, initialise);
    m_label_ref_file.open(path, LABEL_REF_FILE_NAME, initialise);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::store_graph_metadata() noexcept
{
    auto metadata = read_graph_metadata();
    strcpy(&metadata->graph_name[0], m_graph_name.c_str());
    strncpy(&metadata->graph_type[0], "lpg_mmap", CFG_GRAPH_MODEL_TYPE_LENGTH);
    metadata->vertices_c              = 0;
    metadata->edges_c                 = 0;
    metadata->vertex_label_c          = 0;
    metadata->edge_label_c            = 0;
    metadata->vertex_label_table_addr = VERTEX_LABELS_START_ADDR;
    metadata->edge_label_table_addr   = EDGE_LABELS_START_ADDR;
    metadata->label_size              = sizeof(SLabel_t);
    metadata->flush_needed            = false;
    metadata->prune_needed            = false;
}

bool
graphquery::database::storage::CMemoryModelMMAPLPG::store_vertex_entry(const Id_t id, const std::unordered_set<uint16_t> & label_ids, const std::vector<SProperty_t> & props) noexcept
{
    auto data_block_ptr                     = m_vertices_file.attain_data_block();
    const uint32_t entry_offset             = data_block_ptr->idx;

    if (!store_index_entry(id, label_ids, entry_offset))
    {
        m_vertices_file.append_free_data_block(entry_offset);
        return false;
    }

    data_block_ptr->state   = 1 << VERTEX_INITIALISED_STATE_BIT | 1 << VERTEX_VALID_STATE_BIT;
    data_block_ptr->version = END_INDEX;

    data_block_ptr->payload.edge_idx              = END_INDEX;
    data_block_ptr->payload.metadata.id           = id;
    data_block_ptr->payload.metadata.outdegree    = 0;
    data_block_ptr->payload.metadata.property_c   = props.size();
    data_block_ptr->payload.metadata.edge_label_c = 0;

    Id_t next_label_ref = END_INDEX;

    for (const auto label_id : label_ids)
        next_label_ref = store_label_entry(label_id, next_label_ref);

    data_block_ptr->payload.metadata.label_id = next_label_ref;

    Id_t next_props_ref = END_INDEX;

    for (const SProperty_t & prop : props)
        next_props_ref = store_property_entry(prop, next_props_ref);

    data_block_ptr->payload.metadata.property_id = next_props_ref;

    return true;
}

bool
graphquery::database::storage::CMemoryModelMMAPLPG::store_index_entry(const Id_t id, const std::unordered_set<uint16_t> & label_ids, const uint32_t vertex_offset) noexcept
{
    if (!m_index_file.store_entry(id, vertex_offset))
        return false;

    for (const auto label_id : label_ids)
        m_label_vertex[label_id].emplace_back(vertex_offset);
    return true;
}

uint32_t
graphquery::database::storage::CMemoryModelMMAPLPG::store_edge_entry(const uint32_t next_ref,
                                                                     const Id_t src,
                                                                     const Id_t dst,
                                                                     const uint16_t edge_label_id,
                                                                     const std::vector<SProperty_t> & props) noexcept
{
    SRef_t<SEdgeDataBlock, true> data_block_ptr = m_edges_file.attain_data_block(next_ref);
    const auto entry_offset               = data_block_ptr->idx;

    size_t payload_offset = utils::atomic_load(&data_block_ptr->payload_amt);
    for (; payload_offset < data_block_ptr->state.size(); payload_offset++)
    {
        if (data_block_ptr->state.test(payload_offset) == 0)
        {
            data_block_ptr->state.set(payload_offset);
            utils::atomic_fetch_inc(&data_block_ptr->payload_amt);
            break;
        }
    }

    data_block_ptr->state[payload_offset]                          = true;
    data_block_ptr->payload[payload_offset].metadata.dst           = dst;
    data_block_ptr->payload[payload_offset].metadata.src           = src;
    data_block_ptr->payload[payload_offset].metadata.edge_label_id = edge_label_id;
    data_block_ptr->payload[payload_offset].metadata.property_c    = props.size();

    Id_t next_props_ref = END_INDEX;

    for (const SProperty_t & prop : props)
        next_props_ref = store_property_entry(prop, next_props_ref);

    data_block_ptr->payload[payload_offset].metadata.property_id = next_props_ref;
    return entry_offset;
}

graphquery::database::storage::Id_t
graphquery::database::storage::CMemoryModelMMAPLPG::store_label_entry(const uint16_t label_id, const Id_t next_ref) noexcept
{
    SRef_t<SLabelRefDataBlock, true> data_block_ptr = m_label_ref_file.attain_data_block(next_ref);
    const Id_t entry_offset                   = data_block_ptr->idx;

    size_t payload_offset = utils::atomic_load(&data_block_ptr->payload_amt);
    for (; payload_offset < data_block_ptr->state.size(); payload_offset++)
    {
        if (data_block_ptr->state.test(payload_offset) == 0)
        {
            data_block_ptr->state.set(payload_offset);
            utils::atomic_fetch_inc(&data_block_ptr->payload_amt);
            break;
        }
    }

    data_block_ptr->state[payload_offset] = true;
    utils::atomic_store(&data_block_ptr->payload[payload_offset], label_id);
    return entry_offset;
}

graphquery::database::storage::Id_t
graphquery::database::storage::CMemoryModelMMAPLPG::store_property_entry(const SProperty_t & prop, const Id_t next_ref) noexcept
{
    SRef_t<SPropertyDataBlock, true> data_block_ptr = m_properties_file.attain_data_block(next_ref);
    const Id_t entry_offset                   = data_block_ptr->idx;

    size_t payload_offset = utils::atomic_load(&data_block_ptr->payload_amt);
    for (; payload_offset < data_block_ptr->state.size(); payload_offset++)
    {
        if (data_block_ptr->state.test(payload_offset) == 0)
        {
            data_block_ptr->state.set(payload_offset);
            utils::atomic_fetch_inc(&data_block_ptr->payload_amt);
            break;
        }
    }

    data_block_ptr->state[payload_offset] = true;
    strncpy(&data_block_ptr->payload[payload_offset].key[0], prop.key, CFG_LPG_PROPERTY_KEY_LENGTH);
    strncpy(&data_block_ptr->payload[payload_offset].value[0], prop.value, CFG_LPG_PROPERTY_VALUE_LENGTH);

    return entry_offset;
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::transaction_preamble() noexcept
{
    m_cv_sync.wait(m_unq_lock, wait_on_syncing);
    utils::atomic_fetch_inc(&m_transaction_ref_c);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::transaction_epilogue() noexcept
{
    utils::atomic_fetch_dec(&m_transaction_ref_c);
}

bool
graphquery::database::storage::CMemoryModelMMAPLPG::contains_vertex_label_id(const int64_t vertex_offset, const uint16_t label_id) noexcept
{
    std::optional<SRef_t<SVertexDataBlock>> v_ptr = get_vertex_by_offset(vertex_offset);

    if (!v_ptr.has_value())
        return false;

    auto label_head = v_ptr->ref->payload.metadata.label_id;

    while (label_head != END_INDEX)
    {
        auto l_ptr = m_label_ref_file.read_entry(label_head);

        for (size_t i = 0; i < l_ptr->state.size(); i++)
        {
            if (l_ptr->state.test(i) && l_ptr->payload[i] == label_id)
                return true;
        }
        label_head = l_ptr->next;
    }
    return false;
}

uint16_t
graphquery::database::storage::CMemoryModelMMAPLPG::create_edge_label(const std::string_view label_str) noexcept
{
    assert(!label_str.empty());

    const uint16_t label_id = utils::atomic_fetch_inc(&read_graph_metadata()->edge_label_c);
    const auto label_ptr    = read_edge_label_entry<true>(label_id);

    strncpy(&label_ptr.ref->label_s[0], label_str.data(), CFG_LPG_LABEL_LENGTH - 1);
    label_ptr.ref->item_c           = 0;
    label_ptr.ref->label_id         = label_id;
    m_e_label_map[label_str.data()] = label_id;

    return label_id;
}

uint16_t
graphquery::database::storage::CMemoryModelMMAPLPG::create_vertex_label(const std::string_view label_str) noexcept
{
    const uint16_t label_id = utils::atomic_fetch_inc(&read_graph_metadata()->vertex_label_c);
    const auto label_ptr    = read_vertex_label_entry<true>(label_id);

    strncpy(&label_ptr.ref->label_s[0], label_str.data(), CFG_LPG_LABEL_LENGTH - 1);
    label_ptr.ref->item_c   = 0;
    label_ptr.ref->label_id = label_id;
    m_label_vertex.emplace_back();
    m_v_label_map[label_str.data()] = label_id;

    return label_id;
}

std::optional<uint16_t>
graphquery::database::storage::CMemoryModelMMAPLPG::check_if_edge_label_exists(const std::string_view & label_str) noexcept
{
    if (!m_e_label_map.contains(label_str.data()))
        return std::nullopt;

    return m_e_label_map.at(label_str.data());
}

std::optional<uint16_t>
graphquery::database::storage::CMemoryModelMMAPLPG::check_if_vertex_label_exists(const std::string_view & label_str) noexcept
{
    if (!m_v_label_map.contains(label_str.data()))
        return std::nullopt;

    return m_v_label_map.at(label_str.data());
}

std::unordered_set<uint16_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertex_labels(const std::vector<std::string_view> & labels, const bool create_if_absent) noexcept
{
    std::unordered_set<uint16_t> ret   = {};
    std::optional<uint16_t> curr_label = {};
    for (const auto & label : labels)
    {
        curr_label = check_if_vertex_label_exists(label);

        if (!curr_label.has_value())
        {
            if (create_if_absent)
                ret.insert(create_vertex_label(label));

            continue;
        }
        ret.emplace(*curr_label);
    }
    return ret;
}

graphquery::database::storage::CMemoryModelMMAPLPG::EActionState_t
graphquery::database::storage::CMemoryModelMMAPLPG::add_vertex_entry(const Id_t id, const std::vector<std::string_view> & labels, const std::vector<SProperty_t> & props) noexcept
{
    std::unordered_set<uint16_t> label_ids = get_vertex_labels(labels, true);

    if (get_vertex_by_id(id).has_value())
        return EActionState_t::invalid;

    if (!store_vertex_entry(id, label_ids, props))
        return EActionState_t::invalid;

    utils::atomic_fetch_pre_inc(&read_graph_metadata()->vertices_c);

    for (const auto label_id : label_ids)
        utils::atomic_fetch_inc(&read_vertex_label_entry(label_id)->item_c);

    return EActionState_t::valid;
}

graphquery::database::storage::CMemoryModelMMAPLPG::EActionState_t
graphquery::database::storage::CMemoryModelMMAPLPG::add_vertex_entry(const std::vector<std::string_view> & labels, [[maybe_unused]] const std::vector<SProperty_t> & props) noexcept
{
    std::unordered_set<uint16_t> label_ids = get_vertex_labels(labels, true);

    const auto vertex_id = utils::atomic_fetch_inc(&read_graph_metadata()->vertices_c);
    if (!store_vertex_entry(vertex_id, label_ids, props))
    {
        utils::atomic_fetch_dec(&read_graph_metadata()->vertices_c);
        return EActionState_t::invalid;
    }

    for (const auto label_id : label_ids)
        utils::atomic_fetch_inc(&read_vertex_label_entry(label_id)->item_c);

    return EActionState_t::valid;
}

graphquery::database::storage::CMemoryModelMMAPLPG::EActionState_t
graphquery::database::storage::CMemoryModelMMAPLPG::add_edge_entry(const Id_t src,
                                                                   const Id_t dst,
                                                                   const std::string_view edge_label,
                                                                   const std::vector<SProperty_t> & props,
                                                                   const bool undirected) noexcept
{
    auto src_vertex_exists = get_vertex_by_id(src);
    auto dst_vertex_exists = get_vertex_by_id(dst);

    if (!(src_vertex_exists.has_value() && dst_vertex_exists.has_value()))
        return EActionState_t::invalid;

    const std::optional<uint16_t> edge_label_exists = check_if_edge_label_exists(edge_label);
    const auto edge_label_id                        = edge_label_exists.has_value() ? *edge_label_exists : create_edge_label(edge_label);

    auto tail_edge   = utils::atomic_load(&src_vertex_exists.value()->payload.edge_idx);
    auto edge_offset = store_edge_entry(tail_edge, src_vertex_exists.value()->idx, dst_vertex_exists.value()->idx, edge_label_id, props);

    // ~ Update vertex tail and connect edges
    utils::atomic_store(&src_vertex_exists.value()->payload.edge_idx, edge_offset);
    utils::atomic_fetch_inc(&src_vertex_exists.value()->payload.metadata.outdegree);
    utils::atomic_fetch_inc(&dst_vertex_exists.value()->payload.metadata.indegree);
    utils::atomic_fetch_inc(&read_graph_metadata()->edges_c);

    if (undirected)
    {
        tail_edge   = utils::atomic_load(&dst_vertex_exists.value()->payload.edge_idx);
        edge_offset = store_edge_entry(tail_edge, dst_vertex_exists.value()->idx, src_vertex_exists.value()->idx, edge_label_id, props);

        // ~ Update vertex tail and connect edges
        utils::atomic_store(&dst_vertex_exists.value()->payload.edge_idx, edge_offset);
        utils::atomic_fetch_inc(&src_vertex_exists.value()->payload.metadata.indegree);
        utils::atomic_fetch_inc(&dst_vertex_exists.value()->payload.metadata.outdegree);
        utils::atomic_fetch_inc(&read_graph_metadata()->edges_c);
    }

    return EActionState_t::valid;
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::add_vertex(const Id_t src, const std::vector<std::string_view> & labels, const std::vector<SProperty_t> & prop)
{
    transaction_preamble();
    uint64_t commit_addr = m_transactions->log_vertex(labels, prop, src);
    if (add_vertex_entry(src, labels, prop) == EActionState_t::invalid)
    {
        transaction_epilogue();
        m_log_system->warning(fmt::format("Issue adding vertex"));
        rollback();
        return;
    }
    transaction_epilogue();
    m_transactions->commit_transaction<CTransaction::SVertexCommit>(commit_addr);
    utils::atomic_store(&read_graph_metadata()->flush_needed, true);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::add_edge(const Id_t src, const Id_t dst, const std::string_view edge_label, const std::vector<SProperty_t> & prop, bool undirected)
{
    transaction_preamble();
    uint64_t commit_addr = m_transactions->log_edge(src, dst, edge_label, prop, undirected);
    if (add_edge_entry(src, dst, edge_label, prop, undirected) == EActionState_t::invalid)
    {
        transaction_epilogue();
        m_log_system->warning(fmt::format("Issue adding edge({}) to vertex({})", dst, src));
        rollback();
        return;
    }

    transaction_epilogue();
    m_transactions->commit_transaction<CTransaction::SEdgeCommit>(commit_addr);
    utils::atomic_store(&read_graph_metadata()->flush_needed, true);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::add_vertex(const std::vector<std::string_view> & labels, const std::vector<SProperty_t> & prop)
{
    transaction_preamble();
    uint64_t commit_addr = m_transactions->log_vertex(labels, prop);
    (void) add_vertex_entry(labels, prop);

    transaction_epilogue();
    m_transactions->commit_transaction<CTransaction::SVertexCommit>(commit_addr);
    utils::atomic_store(&read_graph_metadata()->flush_needed, true);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::rm_vertex(Id_t src)
{
    transaction_preamble();
    uint64_t commit_addr = m_transactions->log_rm_vertex(src);
    if (rm_vertex_entry(src) == EActionState_t::invalid)
    {
        transaction_epilogue();
        m_log_system->warning(fmt::format("Issue removing vertex({})", src));
        rollback();
        return;
    }

    transaction_epilogue();
    m_transactions->commit_transaction<CTransaction::SVertexCommit>(commit_addr);
    utils::atomic_store(&read_graph_metadata()->flush_needed, true);
    utils::atomic_store(&read_graph_metadata()->prune_needed, true);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::rm_edge(Id_t src, Id_t dst)
{
    transaction_preamble();
    uint64_t commit_addr = m_transactions->log_rm_edge(src, dst);
    if (rm_edge_entry(src, dst) == EActionState_t::invalid)
    {
        transaction_epilogue();
        m_log_system->warning(fmt::format("Issue remvoing edge({}) to vertex({})", dst, src));
        rollback();
        return;
    }

    transaction_epilogue();
    m_transactions->commit_transaction<CTransaction::SEdgeCommit>(commit_addr);
    utils::atomic_store(&read_graph_metadata()->flush_needed, true);
    utils::atomic_store(&read_graph_metadata()->prune_needed, true);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::rm_edge(Id_t src, Id_t dst, const std::string_view edge_label)
{
    transaction_preamble();
    uint64_t commit_addr = m_transactions->log_rm_edge(src, dst, edge_label);
    if (rm_edge_entry(src, dst, edge_label) == EActionState_t::invalid)
    {
        transaction_epilogue();
        m_log_system->warning(fmt::format("Issue remvoing edge({}) to vertex({})", dst, src));
        rollback();
        return;
    }

    transaction_epilogue();
    m_transactions->commit_transaction<CTransaction::SEdgeCommit>(commit_addr);
    utils::atomic_store(&read_graph_metadata()->flush_needed, true);
}

std::optional<graphquery::database::storage::ILPGModel::SVertex_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertex(const Id_t src)
{
    auto ptr = get_vertex_by_id(src);

    if (!ptr.has_value())
        return std::nullopt;

    return ptr->ref->payload.metadata;
}

std::optional<graphquery::database::storage::Id_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertex_idx(const Id_t id) noexcept
{
    auto vertex = get_vertex_by_id(id);

    if(!vertex)
        return std::nullopt;

    return (*vertex)->idx;
}

std::vector<graphquery::database::storage::ILPGModel::SVertex_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertices(const std::function<bool(const SVertex_t &)> & pred)
{
    const uint32_t datablock_c = utils::atomic_load(&m_vertices_file.read_metadata()->data_block_c);

    std::vector<SVertex_t> ret = {};
    ret.reserve(datablock_c);

    for (uint32_t i = 0; i < datablock_c; i++)
    {
        auto curr_vertex_ptr = m_vertices_file.read_entry(i);

        if (unlikely(!(curr_vertex_ptr->state & 1 << VERTEX_INITIALISED_STATE_BIT)))
            continue;

        if (pred(curr_vertex_ptr->payload.metadata))
            ret.emplace_back(curr_vertex_ptr->payload.metadata);
    }

    ret.shrink_to_fit();
    return ret;
}

std::optional<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edge(const Id_t src_vertex_id, const std::string_view edge_label, const int64_t dst_vertex_id)
{
    auto edge_label_id = check_if_edge_label_exists(edge_label);

    if (!edge_label_id.has_value())
        return std::nullopt;

    auto pred = [edge_label_id, dst_vertex_id](const SEdge_t & edge) -> bool { return dst_vertex_id == edge.dst && edge.edge_label_id == *edge_label_id; };

    auto edges = get_edges_by_offset(src_vertex_id, pred);

    if (edges.empty())
        return std::nullopt;

    return edges[0];
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges(const std::function<bool(const SEdge_t &)> & pred)
{
    const Id_t datablock_c = utils::atomic_load(&m_edges_file.read_metadata()->data_block_c);

    std::vector<SEdge_t> ret = {};
    ret.reserve(datablock_c);

    SRef_t<SEdgeDataBlock> gbl_edge_ptr = m_edges_file.read_entry(0);

#pragma omp declare reduction(merge : std::vector<SEdge_t> : omp_out.insert(omp_out.end(), omp_in.begin(), omp_in.end())) initializer(omp_priv = std::vector<SEdge_t>())
#pragma omp parallel for default(none) shared(gbl_edge_ptr, datablock_c, pred) schedule(dynamic) reduction(merge : ret)
    for (Id_t i = 0; i < datablock_c; i++)
    {
        const auto curr_edge_ptr = gbl_edge_ptr + i;

        for (size_t j = 0; j < curr_edge_ptr->state.size(); j++)
        {
            if (likely(curr_edge_ptr->state.test(j)))
            {
                if (pred(curr_edge_ptr->payload[j].metadata))
                    ret.emplace_back(curr_edge_ptr->payload[j].metadata);
            }
        }
    }

    return ret;
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges(const std::string_view vertex_label, const std::string_view edge_label, const Id_t dst)
{
    const auto dst_vertex_exists = get_vertex_by_id(dst);

    if (!dst_vertex_exists.has_value())
        return {};

    auto dst_vertex_idx = dst_vertex_exists->ref->idx;
    return get_edges(vertex_label, edge_label, [dst_vertex_idx](const SEdge_t & edge) -> bool { return edge.dst == dst_vertex_idx; });
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges_by_offset(const Id_t vertex_id, std::string_view edge_label, std::string_view vertex_label)
{
    const auto edge_label_exists   = check_if_edge_label_exists(edge_label);
    const auto vertex_label_exists = check_if_vertex_label_exists(vertex_label);

    if (!(edge_label_exists.has_value() && vertex_label_exists.has_value()))
        return {};

    const auto edge_label_id   = edge_label_exists.value();
    const auto vertex_label_id = vertex_label_exists.value();
    auto gbl_v_ptr             = m_vertices_file.read_entry(0);
    auto gbl_label_ref_ptr     = m_label_ref_file.read_entry(0);

    return get_edges_by_offset(vertex_id,
                               [&gbl_v_ptr, &gbl_label_ref_ptr, &edge_label_id, &vertex_label_id](const SEdge_t & edge) -> bool
                               {
                                   const auto dst_vertex_ptr = gbl_v_ptr + edge.dst;
                                   auto label_head           = dst_vertex_ptr->payload.metadata.label_id;

                                   while (label_head != END_INDEX)
                                   {
                                       const auto label_ptr = gbl_label_ref_ptr + label_head;

                                       for (size_t i = 0; i < label_ptr->state.size(); i++)
                                       {
                                           if (!label_ptr->state.test(i))
                                               continue;

                                           if (label_ptr->payload[i] == vertex_label_id && edge.edge_label_id == edge_label_id)
                                               return true;
                                       }
                                       label_head = label_ptr->next;
                                   }

                                   return false;
                               });
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges(const std::string_view vertex_label, const std::function<bool(const SEdge_t &)> & pred)
{
    std::vector<int64_t> label_vertices = get_vertices_offset_by_label(vertex_label);
    std::vector<SEdge_t> ret;
    ret.reserve(label_vertices.size());

#pragma omp declare reduction(merge : std::vector<SEdge_t> : omp_out.insert(omp_out.end(), omp_in.begin(), omp_in.end())) initializer(omp_priv = std::vector<SEdge_t>())
#pragma omp parallel for default(none) shared(label_vertices, pred) schedule(dynamic) reduction(merge : ret)
    for (size_t i = 0; i < label_vertices.size(); i++)
    {
        const auto edges = get_edges_by_offset(label_vertices[i], pred);
        ret.insert(ret.begin(), edges.begin(), edges.end());
    }
    return ret;
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges(const std::string_view vertex_label, const std::string_view edge_label, const std::function<bool(const SEdge_t &)> & pred)
{
    uint16_t label_id = {};
    if (const std::optional<uint16_t> exists = check_if_edge_label_exists(edge_label); !exists.has_value())
        return {};
    else
        label_id = exists.value();

    const std::vector<int64_t> label_vertices = get_vertices_offset_by_label(vertex_label);

    std::vector<SEdge_t> ret;
    ret.reserve(label_vertices.size());

    auto label_pred = [&label_id, &pred](const SEdge_t & edge) -> bool { return edge.edge_label_id == label_id && pred(edge); };

    // Parallel loop with reduction clause
    for (size_t i = 0; i < label_vertices.size(); i++)
    {
        auto edges = get_edges_by_offset(label_vertices[i], label_pred);
        ret.insert(ret.end(), edges.begin(), edges.end());
    }

    return ret;
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges(const std::string_view vertex_label, const std::string_view edge_label)
{
    uint16_t label_id = {};
    if (const std::optional<uint16_t> exists = check_if_edge_label_exists(edge_label); !exists.has_value())
        return {};
    else
        label_id = exists.value();

    std::vector<int64_t> label_vertices = get_vertices_offset_by_label(vertex_label);

    std::vector<SEdge_t> ret;
    ret.reserve(label_vertices.size());

    auto label_pred = [&label_id](const SEdge_t & edge) -> bool { return edge.edge_label_id == label_id; };

    // Parallel loop with reduction clause
    for (const long label_vertex : label_vertices)
    {
        auto edges = get_edges_by_offset(label_vertex, label_pred);
        ret.insert(ret.end(), edges.begin(), edges.end());
    }

    return ret;
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges(const std::string_view vertex_label, const std::string_view edge_label, const std::string_view dst_vertex_label)
{
    uint16_t edge_label_id  = {};
    uint16_t dst_v_label_id = {};

    if (const std::optional<uint16_t> e_exists = check_if_edge_label_exists(edge_label), v_exists = check_if_vertex_label_exists(dst_vertex_label); !(
        e_exists.has_value() && v_exists.has_value()))
        return {};
    else
    {
        edge_label_id  = e_exists.value();
        dst_v_label_id = v_exists.value();
    }

    std::vector<int64_t> label_vertices = get_vertices_offset_by_label(vertex_label);

    std::vector<SEdge_t> ret;
    ret.reserve(label_vertices.size());
    auto gbl_v_ptr         = m_vertices_file.read_entry(0);
    auto gbl_label_ref_ptr = m_label_ref_file.read_entry(0);

    auto label_pred = [&gbl_v_ptr, &gbl_label_ref_ptr, edge_label_id, dst_v_label_id](const SEdge_t & edge) -> bool
    {
        const auto dst_vertex_ptr = gbl_v_ptr + edge.dst;
        auto label_head           = dst_vertex_ptr->payload.metadata.label_id;

        while (label_head != END_INDEX)
        {
            const auto label_ptr = gbl_label_ref_ptr + label_head;

            for (size_t i = 0; i < label_ptr->state.size(); i++)
            {
                if (!label_ptr->state.test(i))
                    continue;

                if (label_ptr->payload[i] == dst_v_label_id && edge.edge_label_id == edge_label_id)
                    return true;
            }
            label_head = label_ptr->next;
        }

        return false;
    };

    // Parallel loop with reduction clause
    for (const long label_vertex : label_vertices)
    {
        auto edges = get_edges_by_offset(label_vertex, label_pred);
        ret.insert(ret.end(), edges.begin(), edges.end());
    }

    return ret;
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges_by_offset(const uint32_t vertex_id, const std::function<bool(const SEdge_t &)> & pred)
{
    auto vertex_ptr          = get_vertex_by_offset(vertex_id);
    std::vector<SEdge_t> ret = {};

    if (!vertex_ptr.has_value())
        return ret;

    const auto neighbours = utils::atomic_load(&vertex_ptr.value()->payload.metadata.outdegree);
    ret.reserve(neighbours);

    Id_t curr         = utils::atomic_load(&vertex_ptr.value()->payload.edge_idx);
    auto gbl_edge_ptr = m_edges_file.read_entry(0);

    while (curr != END_INDEX)
    {
        auto curr_edge_ptr = gbl_edge_ptr + curr;

        for (size_t i = 0; i < curr_edge_ptr->state.size(); i++)
        {
            if (likely(curr_edge_ptr->state.test(i)))
            {
                if (pred(curr_edge_ptr->payload[i].metadata))
                    ret.emplace_back(curr_edge_ptr->payload[i].metadata);
            }
        }
        curr = curr_edge_ptr->next;
    }
    return ret;
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges_by_offset(const uint32_t src_vertex_id, const uint32_t dst_vertex_id)
{
    auto src_vertex_ptr      = get_vertex_by_offset(src_vertex_id);
    auto dst_vertex_ptr      = get_vertex_by_offset(dst_vertex_id);
    std::vector<SEdge_t> ret = {};

    if (!(src_vertex_ptr.has_value() && dst_vertex_ptr.has_value()))
        return ret;

    ret.reserve(utils::atomic_load(&src_vertex_ptr.value()->payload.metadata.outdegree));
    uint32_t curr           = utils::atomic_load(&src_vertex_ptr.value()->payload.edge_idx);
    auto dst_vertex_ptr_idx = utils::atomic_load(&dst_vertex_ptr->ref->idx);

    while (curr != END_INDEX)
    {
        auto curr_edge_ptr = m_edges_file.read_entry(curr);

        for (size_t i = 0; i < curr_edge_ptr->state.size(); i++)
        {
            if (likely(curr_edge_ptr->state.test(i)))
            {
                if (curr_edge_ptr->payload[i].metadata.dst == dst_vertex_ptr_idx)
                    ret.emplace_back(curr_edge_ptr->payload[i].metadata);
            }
        }

        curr = curr_edge_ptr->next;
    }

    ret.shrink_to_fit();
    return ret;
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges_by_offset(const uint32_t src_vertex_id, const uint32_t dst_vertex_id, const std::function<bool(const SEdge_t &)> & pred)
{
    auto src_vertex_ptr      = get_vertex_by_offset(src_vertex_id);
    auto dst_vertex_ptr      = get_vertex_by_offset(dst_vertex_id);
    std::vector<SEdge_t> ret = {};

    if (!(src_vertex_ptr.has_value() && dst_vertex_ptr.has_value()))
        return ret;

    ret.reserve(utils::atomic_load(&src_vertex_ptr.value()->payload.metadata.outdegree));
    uint32_t curr           = utils::atomic_load(&src_vertex_ptr.value()->payload.edge_idx);
    auto dst_vertex_ptr_idx = utils::atomic_load(&dst_vertex_ptr->ref->idx);

    const auto pred_dst_idx = [&dst_vertex_ptr_idx, &pred](const SEdge_t & edge) -> bool { return edge.dst == dst_vertex_ptr_idx && pred(edge); };

    while (curr != END_INDEX)
    {
        auto curr_edge_ptr = m_edges_file.read_entry(curr);

        for (size_t i = 0; i < curr_edge_ptr->state.size(); i++)
        {
            if (likely(curr_edge_ptr->state.test(i)))
            {
                if (pred_dst_idx(curr_edge_ptr->payload[i].metadata))
                    ret.emplace_back(curr_edge_ptr->payload[i].metadata);
            }
        }

        curr = curr_edge_ptr->next;
    }

    ret.shrink_to_fit();
    return ret;
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges_by_offset(const uint32_t vertex_id, const uint16_t edge_label_id, const std::function<bool(const SEdge_t &)> & pred)
{
    const auto edge_label_pred = [&edge_label_id, &pred](const SEdge_t & edge) -> bool { return edge.edge_label_id == edge_label_id && pred(edge); };

    return get_edges_by_offset(vertex_id, edge_label_pred);
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges_by_id(const Id_t src, const std::function<bool(const SEdge_t &)> & pred)
{
    const auto vertex_ptr    = get_vertex_by_id(src);
    std::vector<SEdge_t> ret = {};

    if (unlikely(!vertex_ptr.has_value()))
        return ret;

    ret.reserve(utils::atomic_load(&vertex_ptr->ref->payload.metadata.outdegree));
    uint32_t curr = utils::atomic_load(&vertex_ptr->ref->payload.edge_idx);

    while (curr != END_INDEX)
    {
        auto curr_edge_ptr = m_edges_file.read_entry(curr);

        for (size_t i = 0; i < curr_edge_ptr->state.size(); i++)
        {
            if (likely(curr_edge_ptr->state.test(i)))
            {
                if (pred(curr_edge_ptr->payload[i].metadata))
                    ret.emplace_back(curr_edge_ptr->payload[i].metadata);
            }
        }
        curr = curr_edge_ptr->next;
    }

    ret.shrink_to_fit();
    return ret;
}

std::unique_ptr<std::vector<std::vector<int64_t>>>
graphquery::database::storage::CMemoryModelMMAPLPG::make_inverse_graph() noexcept
{
    auto inv_graph = std::make_unique<std::vector<std::vector<int64_t>>>();
    auto vblock_c  = m_vertices_file.read_metadata()->data_block_c;
    auto eblock_c  = m_edges_file.read_metadata()->data_block_c;

    inv_graph->resize(vblock_c);
    SRef_t<SEdgeDataBlock> e_ptr = m_edges_file.read_entry(0);

    for (Id_t i = 0; i < eblock_c; i++)
    {
        const auto lcl_e_ptr = e_ptr + i;
        for (size_t j = 0; j < lcl_e_ptr->state.size(); j++)
        {
            if (!lcl_e_ptr->state.test(j))
                continue;

            (*inv_graph)[e_ptr->payload[j].metadata.dst].emplace_back(e_ptr->payload[j].metadata.src);
        }
    }

    return inv_graph;
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::edgemap(const std::unique_ptr<analytic::IRelax> & relax) noexcept
{
    const auto datablock_c = utils::atomic_load(&m_edges_file.read_metadata()->data_block_c);
    auto gbl_curr_edge_ptr = m_edges_file.read_entry(0);

#pragma omp parallel for default(none) shared(gbl_curr_edge_ptr, relax, datablock_c) num_threads(128)
    for (Id_t i = 0; i < datablock_c; i++)
    {
        const auto curr_edge_ptr = gbl_curr_edge_ptr + i;

        if (curr_edge_ptr->state == 0)
            continue;

        for (size_t j = 0; j < curr_edge_ptr->state.size(); j++)
        {
            if (likely(curr_edge_ptr->state.test(j)))
                relax->relax(curr_edge_ptr->payload[j].metadata.src, curr_edge_ptr->payload[j].metadata.dst);
        }
    }
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::src_edgemap(const Id_t vertex_offset, const std::function<void(int64_t src, int64_t dst)> & relax)
{
    std::optional<SRef_t<SVertexDataBlock>> v_ptr_opt = get_vertex_by_offset(vertex_offset);

    if (!v_ptr_opt.has_value())
        return;

    auto v_ptr = std::move(*v_ptr_opt);

    auto edge_head = v_ptr->payload.edge_idx;
    while (edge_head != END_INDEX)
    {
        auto e_ptr = m_edges_file.read_entry(edge_head);

        for (size_t i = 0; i < e_ptr->state.size(); i++)
        {
            if (!e_ptr->state.test(i))
                continue;

            relax(vertex_offset, e_ptr->payload[i].metadata.dst);
        }
        edge_head = e_ptr->next;
    }
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::persist_graph_changes() noexcept
{
    const auto datablock_c = utils::atomic_load(&m_vertices_file.read_metadata()->data_block_c);

    auto gbl_vertex_ptr = m_vertices_file.read_entry(0);
    auto gbl_edge_ptr   = m_edges_file.read_entry(0);
    for (Id_t i = 0; i < datablock_c; i++)
    {
        auto src_vertex_ptr = gbl_vertex_ptr + i;
        if (unlikely(!(src_vertex_ptr->state & 1 << VERTEX_INITIALISED_STATE_BIT)))
            continue;

        uint32_t edge_ref      = src_vertex_ptr->payload.edge_idx;
        uint32_t prev_edge_ref = edge_ref;
        while (edge_ref != END_INDEX)
        {
            auto edge_ptr = gbl_edge_ptr + edge_ref;

            for (size_t j = 0; j < edge_ptr->state.size(); j++)
            {
                if (likely(edge_ptr->state.test(j)))
                {
                    auto dst_vertex_ptr = gbl_vertex_ptr + edge_ptr->payload[j].metadata.dst;

                    if (dst_vertex_ptr->state & 1 << VERTEX_MARKED_STATE_BIT)
                    {
                        edge_ptr->state[j].flip();
                        edge_ptr->payload_amt--;
                        utils::atomic_fetch_dec(&read_graph_metadata()->edges_c);
                        utils::atomic_fetch_dec(&dst_vertex_ptr->payload.metadata.indegree);
                        utils::atomic_fetch_dec(&src_vertex_ptr->payload.metadata.outdegree);

                        if (dst_vertex_ptr->payload.metadata.indegree == 0)
                            m_vertices_file.append_free_data_block(dst_vertex_ptr->idx);
                    }
                }
            }

            if (edge_ptr->payload_amt == 0)
            {
                auto prev_edge_ptr  = gbl_edge_ptr + prev_edge_ref;
                prev_edge_ptr->next = edge_ptr->next;
                m_edges_file.append_free_data_block(edge_ptr->idx);

                if (edge_ref == prev_edge_ref)
                    src_vertex_ptr->payload.edge_idx = END_INDEX;
            }

            prev_edge_ref = edge_ref;
            edge_ref      = edge_ptr->next;
        }
    }
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges(const Id_t src, const Id_t dst)
{
    auto src_vertex_ptr = get_vertex_by_id(src);
    auto dst_vertex_ptr = get_vertex_by_id(dst);

    if (!(src_vertex_ptr.has_value() && dst_vertex_ptr.has_value()))
        return {};

    return get_edges_by_offset(src_vertex_ptr->ref->idx, dst_vertex_ptr->ref->idx);
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges_by_label(const std::string_view label)
{
    const std::optional<uint16_t> exists = check_if_edge_label_exists(label);

    if (!exists.has_value())
        return {};

    const auto label_id = exists.value();

    return get_edges([&label_id](const SEdge_t & edge) -> bool { return edge.edge_label_id == label_id; });
}

std::vector<graphquery::database::storage::ILPGModel::SVertex_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertices_by_label(const std::string_view label)
{
    const std::optional<uint16_t> exists = check_if_vertex_label_exists(label);

    if (!exists.has_value())
        return {};

    const uint16_t label_id = exists.value();

    std::vector<SVertex_t> ret = {};
    ret.reserve(m_label_vertex[label_id].size());

    for (const uint32_t vertex_offset : m_label_vertex[label_id])
    {
        auto vertex_ptr = get_vertex_by_offset(vertex_offset);

        if (!vertex_ptr.has_value())
            continue;

        ret.emplace_back(vertex_ptr.value()->payload.metadata);
    }

    return ret;
}

std::vector<int64_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertices_offset_by_label(const std::string_view label)
{
    const std::optional<uint16_t> exists = check_if_vertex_label_exists(label);

    if (!exists.has_value())
        return {};

    const uint16_t label_id = exists.value();

    std::vector<int64_t> ret = {};
    ret.reserve(m_label_vertex[label_id].size());

    for (const uint32_t vertex_offset : m_label_vertex[label_id])
        ret.emplace_back(vertex_offset);

    return ret;
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges(const Id_t src, const std::string_view edge_label, const std::string_view vertex_label)
{
    const auto src_vertex_ptr      = get_vertex_by_id(src);
    const auto vertex_label_exists = check_if_vertex_label_exists(vertex_label);
    const auto edge_label_exists   = check_if_edge_label_exists(edge_label);

    if (!(src_vertex_ptr.has_value() && vertex_label_exists.has_value() && edge_label_exists.has_value()))
        return {};

    const int64_t src_vertex_idx   = utils::atomic_load(&src_vertex_ptr->ref->idx);
    const uint16_t vertex_label_id = vertex_label_exists.value();
    const uint16_t edge_label_id   = edge_label_exists.value();

    return get_edges_by_offset(src_vertex_idx, edge_label_id, [this, &vertex_label_id](const SEdge_t & edge) -> bool { return contains_vertex_label_id(edge.dst, vertex_label_id); });
}

std::unordered_set<graphquery::database::storage::Id_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edge_dst_vertices(const Id_t src, [[maybe_unused]] const std::function<bool(const SEdge_t &)> & pred)
{
    auto vertex_ptr = get_vertex_by_id(src);

    if (unlikely(!vertex_ptr.has_value()))
        return {};

    const auto neighbours = utils::atomic_load(&(*vertex_ptr)->payload.metadata.outdegree);

    std::unordered_set<Id_t> ret = {};
    ret.reserve(neighbours);

    auto gbl_edge_ptr = m_edges_file.read_entry(0);
    uint32_t curr     = utils::atomic_load(&(*vertex_ptr)->payload.edge_idx);

    while (curr != END_INDEX)
    {
        const auto lcl_edge_ptr = gbl_edge_ptr + curr;

        for (size_t i = 0; i < lcl_edge_ptr->state.size(); i++)
        {
            if (pred(lcl_edge_ptr->payload[i].metadata))
                ret.insert(lcl_edge_ptr->payload[i].metadata.dst);
        }
        curr = lcl_edge_ptr->next;
    }
    return ret;
}

std::unordered_set<graphquery::database::storage::Id_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edge_dst_vertices(const Id_t src, const std::string_view edge_label, const std::string_view vertex_label)
{
    const std::optional<uint16_t> vertex_label_exists = check_if_vertex_label_exists(vertex_label);
    const std::optional<uint16_t> edge_label_exists   = check_if_edge_label_exists(edge_label);

    if (!(vertex_label_exists.has_value() && edge_label_exists.has_value()))
        return {};

    auto vertex_label_id   = vertex_label_exists.value();
    auto edge_label_id     = edge_label_exists.value();
    auto gbl_v_ptr         = m_vertices_file.read_entry(0);
    auto gbl_label_ref_ptr = m_label_ref_file.read_entry(0);

    return get_edge_dst_vertices(src,
                                 [&gbl_label_ref_ptr, &gbl_v_ptr, &vertex_label_id, &edge_label_id](const SEdge_t & edge) -> bool
                                 {
                                     const auto dst_vertex_ptr = gbl_v_ptr + edge.dst;
                                     auto label_head           = dst_vertex_ptr->payload.metadata.label_id;

                                     while (label_head != END_INDEX)
                                     {
                                         const auto label_ptr = gbl_label_ref_ptr + label_head;

                                         for (size_t i = 0; i < label_ptr->state.size(); i++)
                                         {
                                             if (!label_ptr->state.test(i))
                                                 continue;

                                             if (label_ptr->payload[i] == vertex_label_id && edge.edge_label_id == edge_label_id)
                                                 return true;
                                         }
                                         label_head = label_ptr->next;
                                     }

                                     return false;
                                 });
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_recursive_edges(const Id_t src, const std::vector<SProperty_t> edge_vertex_label_pairs)
{
    const auto src_vertex_ptr = get_vertex_by_id(src);
    std::vector<SEdge_t> ret  = {};

    if (!src_vertex_ptr.has_value())
        return ret;

    std::unordered_set vertices_to_process = {src_vertex_ptr->ref->idx};

    for (const auto & curr_label_pair : edge_vertex_label_pairs)
    {
        ret.clear();
        for (const auto & i : vertices_to_process)
        {
            auto retrieved_edges = get_edges(i, curr_label_pair.key, curr_label_pair.value);
            ret.insert(ret.begin(), retrieved_edges.begin(), retrieved_edges.end());
        }
        vertices_to_process.clear();

        for (const auto & curr_result : ret)
            vertices_to_process.insert(curr_result.dst);
    }
    return ret;
}

std::vector<graphquery::database::storage::ILPGModel::SProperty_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_properties_by_vertex(const Id_t src)
{
    SRef_t<SVertexDataBlock> src_vertex = {};
    std::vector<SProperty_t> ret        = {};

    if (auto src_vertex_exists = get_vertex_by_id(src); unlikely(!src_vertex_exists.has_value()))
        return ret;
    else
        src_vertex = std::move(src_vertex_exists.value());

    const auto properties = utils::atomic_load(&src_vertex->payload.metadata.property_c);
    ret.reserve(properties);

    auto property_ref_cpy = utils::atomic_load(&src_vertex->payload.metadata.property_id);
    while (property_ref_cpy != END_INDEX)
    {
        auto property_ptr = m_properties_file.read_entry(property_ref_cpy);

        for (size_t i = 0; i < property_ptr->state.size(); i++)
        {
            if (likely(property_ptr->state.test(i)))
                ret.emplace_back(property_ptr->payload[i].key, property_ptr->payload[i].value);
        }

        property_ref_cpy = property_ptr->next;
    }

    ret.shrink_to_fit();
    return ret;
}

std::unordered_map<std::string, std::string>
graphquery::database::storage::CMemoryModelMMAPLPG::get_properties_by_vertex_map(Id_t src)
{
    SRef_t<SVertexDataBlock> src_vertex              = {};
    std::unordered_map<std::string, std::string> ret = {};

    if (auto src_vertex_exists = get_vertex_by_id(src); unlikely(!src_vertex_exists.has_value()))
        return ret;
    else
        src_vertex = std::move(src_vertex_exists.value());

    const auto properties = utils::atomic_load(&src_vertex->payload.metadata.property_c);
    ret.reserve(properties);

    auto property_ref_cpy = utils::atomic_load(&src_vertex->payload.metadata.property_id);
    while (property_ref_cpy != END_INDEX)
    {
        auto property_ptr = m_properties_file.read_entry(property_ref_cpy);

        for (size_t i = 0; i < property_ptr->state.size(); i++)
        {
            if (likely(property_ptr->state.test(i)))
                ret[property_ptr->payload[i].key] = property_ptr->payload[i].value;
        }

        property_ref_cpy = property_ptr->next;
    }

    return ret;
}

std::vector<graphquery::database::storage::ILPGModel::SProperty_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_properties_by_id(const int64_t id)
{
    SRef_t<SVertexDataBlock> vertex_ptr = m_vertices_file.read_entry(static_cast<int64_t>(id));
    std::vector<SProperty_t> ret        = {};

    ret.reserve(vertex_ptr->payload.metadata.property_c);

    auto property_ref_cpy = vertex_ptr->payload.metadata.property_id;
    while (property_ref_cpy != END_INDEX)
    {
        auto property_ptr = m_properties_file.read_entry(property_ref_cpy);

        for (size_t i = 0; i < property_ptr->state.size(); i++)
        {
            if (likely(property_ptr->state.test(i)))
                ret.emplace_back(property_ptr->payload[i].key, property_ptr->payload[i].value);
        }

        property_ref_cpy = property_ptr->next;
    }

    ret.shrink_to_fit();
    return ret;
}

std::unordered_map<std::string, std::string>
graphquery::database::storage::CMemoryModelMMAPLPG::get_properties_by_id_map(const int64_t id)
{
    SRef_t<SVertexDataBlock> vertex_ptr              = m_vertices_file.read_entry(id);
    std::unordered_map<std::string, std::string> ret = {};

    ret.reserve(vertex_ptr->payload.metadata.property_c);

    auto property_ref_cpy = vertex_ptr->payload.metadata.property_id;
    while (property_ref_cpy != END_INDEX)
    {
        auto property_ptr = m_properties_file.read_entry(property_ref_cpy);

        for (size_t i = 0; i < property_ptr->state.size(); i++)
        {
            if (likely(property_ptr->state.test(i)))
                ret.emplace(property_ptr->payload[i].key, property_ptr->payload[i].value);
        }

        property_ref_cpy = property_ptr->next;
    }

    return ret;
}

std::vector<graphquery::database::storage::ILPGModel::SProperty_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_properties_by_property_id(const uint32_t id)
{
    std::vector<SProperty_t> ret = {};

    auto property_ref_cpy = id;
    while (property_ref_cpy != END_INDEX)
    {
        auto property_ptr = m_properties_file.read_entry(property_ref_cpy);

        for (size_t i = 0; i < property_ptr->state.size(); i++)
        {
            if (likely(property_ptr->state.test(i)))
                ret.emplace_back(property_ptr->payload[i].key, property_ptr->payload[i].value);
        }

        property_ref_cpy = property_ptr->next;
    }

    ret.shrink_to_fit();
    return ret;
}

std::unordered_map<std::string, std::string>
graphquery::database::storage::CMemoryModelMMAPLPG::get_properties_by_property_id_map(const uint32_t id)
{
    std::unordered_map<std::string, std::string> ret = {};

    auto property_ref_cpy = id;
    while (property_ref_cpy != END_INDEX)
    {
        auto property_ptr = m_properties_file.read_entry(property_ref_cpy);

        for (size_t i = 0; i < property_ptr->state.size(); i++)
        {
            if (likely(property_ptr->state.test(i)))
                ret[property_ptr->payload[i].key] = property_ptr->payload[i].value;
        }

        property_ref_cpy = property_ptr->next;
    }

    return ret;
}

uint32_t
graphquery::database::storage::CMemoryModelMMAPLPG::out_degree(const Id_t id) noexcept
{
    assert(id < m_vertices_file.read_metadata()->data_block_c);
    return get_vertex_by_offset(id)->ref->payload.metadata.outdegree;
}

uint32_t
graphquery::database::storage::CMemoryModelMMAPLPG::in_degree(const Id_t id) noexcept
{
    assert(id < m_vertices_file.read_metadata()->data_block_c);
    return get_vertex_by_offset(id)->ref->payload.metadata.indegree;
}

uint32_t
graphquery::database::storage::CMemoryModelMMAPLPG::out_degree_by_id(const Id_t id) noexcept
{
    return get_vertex(id)->outdegree;
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::calc_outdegree(uint32_t outdeg[]) noexcept
{
    const auto datablock_c   = utils::atomic_load(&m_vertices_file.read_metadata()->data_block_c);
    auto gbl_curr_vertex_ptr = m_vertices_file.read_entry(0);

#pragma omp parallel for default(none) shared(gbl_curr_vertex_ptr, outdeg, datablock_c) schedule(auto)
    for (Id_t i = 0; i < datablock_c; i++)
    {
        const auto curr_vertex_ptr = gbl_curr_vertex_ptr + i;
        if (unlikely(!(curr_vertex_ptr->state & 1 << VERTEX_INITIALISED_STATE_BIT)))
            continue;

        outdeg[i] = utils::atomic_load(&curr_vertex_ptr->payload.metadata.outdegree);
    }
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::calc_indegree(uint32_t indeg[]) noexcept
{
    const auto datablock_c   = utils::atomic_load(&m_vertices_file.read_metadata()->data_block_c);
    auto gbl_curr_vertex_ptr = m_vertices_file.read_entry(0);

#pragma omp parallel for default(none) shared(gbl_curr_vertex_ptr, indeg, datablock_c) schedule(auto)
    for (Id_t i = 0; i < datablock_c; i++)
    {
        const auto curr_vertex_ptr = gbl_curr_vertex_ptr + i;
        if (unlikely(!(curr_vertex_ptr->state & 1 << VERTEX_INITIALISED_STATE_BIT)))
            continue;

        indeg[i] = utils::atomic_load(&curr_vertex_ptr->payload.metadata.indegree);
    }
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::calc_vertex_sparse_map(Id_t arr[]) noexcept
{
    SRef_t<SVertexDataBlock> gbl_vertex_ptr = m_vertices_file.read_entry(0);
    int64_t block_c                         = utils::atomic_load(&m_vertices_file.read_metadata()->data_block_c);
    int64_t vertex_i                        = 0;

    for (int64_t i = 0; i < block_c; i++)
    {
        auto local_ptr = gbl_vertex_ptr + i;
        if (local_ptr->state & 1 << VERTEX_VALID_STATE_BIT)
            arr[vertex_i++] = i;
    }
}

double
graphquery::database::storage::CMemoryModelMMAPLPG::get_avg_out_degree() noexcept
{
    return static_cast<double>(get_num_edges()) / static_cast<double>(get_num_vertices());
}

std::optional<graphquery::database::storage::SRef_t<graphquery::database::storage::CMemoryModelMMAPLPG::SVertexDataBlock>>
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertex_by_offset(const uint32_t offset) noexcept
{
    const uint32_t block_c = utils::atomic_load(&m_vertices_file.read_metadata()->data_block_c);

    if (offset >= block_c)
        return std::nullopt;

    auto v_ptr = m_vertices_file.read_entry(offset);
    if (v_ptr->state & 1 << VERTEX_MARKED_STATE_BIT)
        return std::nullopt;
    return v_ptr;
}

std::optional<graphquery::database::storage::SRef_t<graphquery::database::storage::CMemoryModelMMAPLPG::SVertexDataBlock>>
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertex_by_id(const Id_t id) noexcept
{
    auto index_ptr = m_index_file.read_entry(id);

    if (index_ptr->set == 0)
        return std::nullopt;

    auto v_ptr = m_vertices_file.read_entry(index_ptr->offset);
    if (v_ptr->state & 1 << VERTEX_MARKED_STATE_BIT)
        return std::nullopt;
    return v_ptr;
}

template<bool write>
graphquery::database::storage::SRef_t<graphquery::database::storage::CMemoryModelMMAPLPG::SGraphMetaData_t, write>
graphquery::database::storage::CMemoryModelMMAPLPG::read_graph_metadata() noexcept
{
    return m_master_file.ref<SGraphMetaData_t, write>(METADATA_START_ADDR);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::define_luts() noexcept
{
    auto label_c = utils::atomic_load(&read_graph_metadata()->vertex_label_c);

    m_label_vertex.resize(label_c);
    m_v_label_map.reserve(label_c);

    auto label_ptr = read_vertex_label_entry(0);

    for (uint16_t i = 0; i < label_c; i++, ++label_ptr)
    {
        m_v_label_map[label_ptr->label_s] = label_ptr->label_id;
        m_label_vertex[label_ptr->label_id].reserve(label_ptr->item_c);
    }

    label_c = utils::atomic_load(&read_graph_metadata()->edge_label_c);
    m_e_label_map.reserve(label_c);
    label_ptr = read_edge_label_entry(0);

    for (uint16_t i                       = 0; i < label_c; i++, ++label_ptr)
        m_e_label_map[label_ptr->label_s] = label_ptr->label_id;
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::read_index_list() noexcept
{
    define_luts();
    const uint32_t block_c = utils::atomic_load(&m_vertices_file.read_metadata()->data_block_c);

    auto vertex_ptr = m_vertices_file.read_entry(0);
    for (uint32_t vertex_i = 0; vertex_i < block_c; vertex_i++, ++vertex_ptr)
    {
        if (unlikely(!(vertex_ptr->state & 1 << VERTEX_INITIALISED_STATE_BIT)))
            continue;

        auto label_head = vertex_ptr->payload.metadata.label_id;

        while (label_head != END_INDEX)
        {
            auto label_ptr = m_label_ref_file.read_entry(label_head);

            if (!label_ptr->state.any())
                continue;

            for (size_t i = 0; i < label_ptr->payload_amt; i++)
                if (label_ptr->state.test(i))
                    m_label_vertex[label_ptr->payload[i]].emplace_back(vertex_ptr->idx);

            label_head = label_ptr->next;
        }
    }
}

template<bool write>
graphquery::database::storage::SRef_t<graphquery::database::storage::CMemoryModelMMAPLPG::SLabel_t, write>
graphquery::database::storage::CMemoryModelMMAPLPG::read_vertex_label_entry(const uint32_t offset) noexcept
{
    static const auto base_addr  = read_graph_metadata()->vertex_label_table_addr;
    static const auto label_size = read_graph_metadata()->label_size;
    const auto effective_addr    = base_addr + (label_size * offset);
    return m_master_file.ref<SLabel_t, write>(effective_addr);
}

template<bool write>
graphquery::database::storage::SRef_t<graphquery::database::storage::CMemoryModelMMAPLPG::SLabel_t, write>
graphquery::database::storage::CMemoryModelMMAPLPG::read_edge_label_entry(const uint32_t offset) noexcept
{
    static const auto base_addr  = read_graph_metadata()->edge_label_table_addr;
    static const auto label_size = read_graph_metadata()->label_size;
    const auto effective_addr    = base_addr + (label_size * offset);
    return m_master_file.ref<SLabel_t, write>(effective_addr);
}

graphquery::database::storage::CMemoryModelMMAPLPG::EActionState_t
graphquery::database::storage::CMemoryModelMMAPLPG::rm_vertex_entry(const Id_t src) noexcept
{
    SRef_t<SVertexDataBlock> vertex_ptr = {};

    if (auto vertex_opt = get_vertex_by_id(src); unlikely(!vertex_opt.has_value()))
        return EActionState_t::invalid;
    else
        vertex_ptr = std::move(vertex_opt.value());

    //~ Mark deletion for each edge block connected to the vertex
    const auto head_edge_idx = utils::atomic_load(&vertex_ptr->payload.edge_idx);
    uint32_t count           = m_edges_file.foreach_block(head_edge_idx, [this](SRef_t<SEdgeDataBlock> & edge_block_ptr) -> void { m_edges_file.append_free_data_block(edge_block_ptr->idx); });

    //~ Mark deletion for vertex
    utils::atomic_store(&vertex_ptr->state, 1 << VERTEX_MARKED_STATE_BIT);
    utils::atomic_fetch_dec(&read_graph_metadata()->vertices_c);
    utils::atomic_fetch_sub(&read_graph_metadata()->edges_c, static_cast<Id_t>(count));
    utils::atomic_fetch_sub(&vertex_ptr->payload.metadata.outdegree, count);

    if (vertex_ptr->payload.metadata.indegree == 0)
        m_vertices_file.append_free_data_block(vertex_ptr->idx);

    return EActionState_t::valid;
}

graphquery::database::storage::CMemoryModelMMAPLPG::EActionState_t
graphquery::database::storage::CMemoryModelMMAPLPG::rm_edge_entry(const Id_t src, const Id_t dst) noexcept
{
    std::optional<SRef_t<SVertexDataBlock>> src_vertex_ptr = get_vertex_by_id(src);
    std::optional<SRef_t<SVertexDataBlock>> dst_vertex_ptr = get_vertex_by_id(dst);

    if (unlikely(!(src_vertex_ptr.has_value() && dst_vertex_ptr.has_value())))
        return EActionState_t::invalid;

    const uint32_t head_edge_idx = utils::atomic_load(&src_vertex_ptr.value()->payload.edge_idx);
    const auto dst_idx           = utils::atomic_load(&dst_vertex_ptr.value()->idx);

    uint32_t count = m_edges_file.foreach_block(head_edge_idx,
                                                [dst_idx, &dst_vertex_ptr](SRef_t<SEdgeDataBlock> & edge_block_ptr) -> void
                                                {
                                                    for (size_t i = 0; i < edge_block_ptr->state.size(); i++)
                                                    {
                                                        if (edge_block_ptr->state.test(i))
                                                        {
                                                            if (edge_block_ptr->payload[i].metadata.dst == dst_idx)
                                                            {
                                                                edge_block_ptr->state[i].flip();
                                                                utils::atomic_fetch_dec(&dst_vertex_ptr->ref->payload.metadata.indegree);
                                                            }
                                                        }
                                                    }
                                                });

    utils::atomic_fetch_sub(&read_graph_metadata()->edges_c, static_cast<Id_t>(count));
    utils::atomic_fetch_sub(&src_vertex_ptr->ref->payload.metadata.outdegree, count);

    return EActionState_t::valid;
}

graphquery::database::storage::CMemoryModelMMAPLPG::EActionState_t
graphquery::database::storage::CMemoryModelMMAPLPG::rm_edge_entry(const Id_t src, const Id_t dst, const std::string_view edge_label) noexcept
{
    std::optional<SRef_t<SVertexDataBlock>> src_vertex_ptr = get_vertex_by_id(src);
    std::optional<SRef_t<SVertexDataBlock>> dst_vertex_ptr = get_vertex_by_id(dst);
    std::optional<uint16_t> edge_label_id                  = check_if_vertex_label_exists(edge_label);

    if (unlikely(!(src_vertex_ptr.has_value() && dst_vertex_ptr.has_value() && edge_label_id.value())))
        return EActionState_t::invalid;

    const uint32_t head_edge_idx = utils::atomic_load(&src_vertex_ptr.value()->payload.edge_idx);
    const auto dst_idx           = utils::atomic_load(&dst_vertex_ptr.value()->idx);

    uint32_t count = m_edges_file.foreach_block(head_edge_idx,
                                                [dst_idx, &edge_label_id, &dst_vertex_ptr](SRef_t<SEdgeDataBlock> & edge_block_ptr) -> void
                                                {
                                                    for (size_t i = 0; i < edge_block_ptr->state.size(); i++)
                                                    {
                                                        if (edge_block_ptr->state.test(i))
                                                        {
                                                            if (edge_block_ptr->payload[i].metadata.dst == dst_idx && edge_block_ptr->payload[i].metadata.edge_label_id == *edge_label_id)
                                                            {
                                                                edge_block_ptr->state[i].flip();
                                                                utils::atomic_fetch_dec(&dst_vertex_ptr->ref->payload.metadata.indegree);
                                                            }
                                                        }
                                                    }
                                                });

    utils::atomic_fetch_sub(&read_graph_metadata()->edges_c, static_cast<Id_t>(count));
    utils::atomic_fetch_sub(&src_vertex_ptr->ref->payload.metadata.outdegree, static_cast<uint32_t>(count));

    return EActionState_t::valid;
}

int64_t
graphquery::database::storage::CMemoryModelMMAPLPG::get_num_edges()
{
    return read_graph_metadata()->edges_c;
}

int64_t
graphquery::database::storage::CMemoryModelMMAPLPG::get_num_vertices()
{
    return read_graph_metadata()->vertices_c;
}

int64_t
graphquery::database::storage::CMemoryModelMMAPLPG::get_total_num_vertices() noexcept
{
    return m_vertices_file.read_metadata()->data_block_c;
}

uint16_t
graphquery::database::storage::CMemoryModelMMAPLPG::get_num_vertex_labels()
{
    return read_graph_metadata()->vertex_label_c;
}

uint16_t
graphquery::database::storage::CMemoryModelMMAPLPG::get_num_edge_labels()
{
    return read_graph_metadata()->edge_label_c;
}

extern "C"
{
LIB_EXPORT void
create_graph_model(graphquery::database::storage::ILPGModel ** graph_model, const std::shared_ptr<graphquery::logger::CLogSystem> & log_system, const bool & _sync_state_)
{
    assert(log_system != nullptr);
    *graph_model = new graphquery::database::storage::CMemoryModelMMAPLPG(log_system, _sync_state_);
}
}