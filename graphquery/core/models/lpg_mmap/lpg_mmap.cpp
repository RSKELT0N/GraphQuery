#include "lpg_mmap.h"

#include <cassert>
#include <string_view>
#include <optional>
#include <vector>
#include <set>

graphquery::database::storage::CMemoryModelMMAPLPG::
CMemoryModelMMAPLPG(const std::shared_ptr<logger::CLogSystem> & log_system): ILPGModel(log_system), m_sync_needed(false), m_syncing(0), m_transaction_ref_c(0)
{
    m_label_vertex = std::vector<std::vector<uint32_t>>();
    m_unq_lock     = std::unique_lock(m_sync_lock);
}

graphquery::database::storage::CMemoryModelMMAPLPG::~
CMemoryModelMMAPLPG()
{
    save_graph();
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
graphquery::database::storage::CMemoryModelMMAPLPG::save_graph() noexcept
{
    if (m_sync_needed)
    {
        m_cv_sync.wait(m_unq_lock, wait_on_transactions);
        utils::atomic_store(&m_syncing, 1);

        (void) m_master_file.async();
        (void) m_index_file.get_file().async();
        (void) m_vertices_file.get_file().async();
        (void) m_edges_file.get_file().async();
        (void) m_properties_file.get_file().async();

        m_transactions->reset();
        utils::atomic_store(&m_syncing, 0);
        m_cv_sync.notify_all();

        utils::atomic_store(&m_sync_needed, false);
        m_log_system->debug("Graph has been saved");
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
    m_transactions->handle_transactions();
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::setup_files(const std::filesystem::path & path, const bool initialise) noexcept
{
    //~ Set path for master file and initialise transactions
    m_master_file.set_path(path);
    m_master_file.open(MASTER_FILE_NAME);
    m_transactions = std::make_shared<CTransaction>(path, this);

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
    strncpy(&metadata->graph_name[0], m_graph_name.c_str(), CFG_GRAPH_NAME_LENGTH);
    strncpy(&metadata->graph_type[0], "lpg_mmap", CFG_GRAPH_MODEL_TYPE_LENGTH);
    metadata->vertices_c              = 0;
    metadata->edges_c                 = 0;
    metadata->vertex_label_c          = 0;
    metadata->edge_label_c            = 0;
    metadata->vertex_label_table_addr = VERTEX_LABELS_START_ADDR;
    metadata->edge_label_table_addr   = EDGE_LABELS_START_ADDR;
    metadata->label_size              = sizeof(SLabel_t);
}

uint32_t
graphquery::database::storage::CMemoryModelMMAPLPG::store_vertex_entry(const SNodeID id, const std::unordered_set<uint16_t> & label_ids, const std::vector<SProperty_t> & props) noexcept
{
    SRef_t<SVertexDataBlock> data_block_ptr = m_vertices_file.attain_data_block();
    const uint32_t entry_offset             = data_block_ptr->idx;

    data_block_ptr->state[0] = true;
    data_block_ptr->version  = END_INDEX;

    data_block_ptr->payload.edge_idx              = END_INDEX;
    data_block_ptr->payload.metadata.id           = id;
    data_block_ptr->payload.metadata.neighbour_c  = 0;
    data_block_ptr->payload.metadata.property_c   = props.size();
    data_block_ptr->payload.metadata.edge_label_c = 0;

    uint32_t next_label_ref = END_INDEX;

    for (const auto label_id : label_ids)
        next_label_ref = store_label_entry(label_id, next_label_ref);

    data_block_ptr->payload.metadata.label_id = next_label_ref;

    uint32_t next_props_ref = END_INDEX;

    for (const SProperty_t & prop : props)
        next_props_ref = store_property_entry(prop, next_props_ref);

    data_block_ptr->payload.metadata.property_id = next_props_ref;

    return entry_offset;
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::store_index_entry(const int64_t id, const std::unordered_set<uint16_t> & label_ids, const uint32_t vertex_offset) noexcept
{
    m_index_file.store_entry(id, vertex_offset);

    for (const auto label_id : label_ids)
        m_label_vertex[label_id].emplace_back(vertex_offset);
}

uint32_t
graphquery::database::storage::CMemoryModelMMAPLPG::store_edge_entry(const uint32_t next_ref,
                                                                     const int64_t src,
                                                                     const int64_t dst,
                                                                     const uint16_t edge_label_id,
                                                                     const std::vector<SProperty_t> & props) noexcept
{
    SRef_t<SEdgeDataBlock> data_block_ptr = m_edges_file.attain_data_block(next_ref);
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

    uint32_t next_props_ref = END_INDEX;

    for (const SProperty_t & prop : props)
        next_props_ref = store_property_entry(prop, next_props_ref);

    data_block_ptr->payload[payload_offset].metadata.property_id = next_props_ref;
    return entry_offset;
}

uint32_t
graphquery::database::storage::CMemoryModelMMAPLPG::store_label_entry(const uint16_t label_id, const uint32_t next_ref) noexcept
{
    SRef_t<SLabelRefDataBlock> data_block_ptr = m_label_ref_file.attain_data_block(next_ref);
    const uint32_t entry_offset               = data_block_ptr->idx;

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

uint32_t
graphquery::database::storage::CMemoryModelMMAPLPG::store_property_entry(const SProperty_t & prop, const uint32_t next_ref) noexcept
{
    SRef_t<SPropertyDataBlock> data_block_ptr = m_properties_file.attain_data_block(next_ref);
    const uint32_t entry_offset               = data_block_ptr->idx;

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

uint16_t
graphquery::database::storage::CMemoryModelMMAPLPG::create_edge_label(const std::string_view label_str) noexcept
{
    assert(!label_str.empty());

    auto graph_metadata         = read_graph_metadata();
    const uint16_t label_id     = utils::atomic_fetch_inc(&graph_metadata->edge_label_c);
    const uint32_t label_offset = EDGE_LABELS_START_ADDR + label_id * sizeof(SLabel_t);
    const auto label_ptr        = m_master_file.ref<SLabel_t>(label_offset);

    strncpy(&label_ptr.ref->label_s[0], label_str.data(), CFG_LPG_LABEL_LENGTH - 1);
    label_ptr.ref->item_c   = 0;
    label_ptr.ref->label_id = label_id;
    return label_id;
}

uint16_t
graphquery::database::storage::CMemoryModelMMAPLPG::create_vertex_label(const std::string_view label_str) noexcept
{
    auto graph_metadata     = read_graph_metadata();
    const uint16_t label_id = utils::atomic_fetch_inc(&graph_metadata->vertex_label_c);
    const auto label_ptr    = read_vertex_label_entry(label_id);

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
graphquery::database::storage::CMemoryModelMMAPLPG::add_vertex_entry(const SNodeID id, const std::vector<std::string_view> & labels, const std::vector<SProperty_t> & props) noexcept
{
    std::unordered_set<uint16_t> label_ids = get_vertex_labels(labels, true);

    if (get_vertex_by_id(id).has_value())
        return EActionState_t::invalid;

    utils::atomic_fetch_inc(&read_graph_metadata()->vertices_c);
    for (const auto label_id : label_ids)
        utils::atomic_fetch_inc(&read_vertex_label_entry(label_id)->item_c);

    const auto entry_offset = store_vertex_entry(id, label_ids, props);
    store_index_entry(id, label_ids, entry_offset);
    return EActionState_t::valid;
}

graphquery::database::storage::CMemoryModelMMAPLPG::EActionState_t
graphquery::database::storage::CMemoryModelMMAPLPG::add_vertex_entry(const std::vector<std::string_view> & labels, const std::vector<SProperty_t> & props) noexcept
{
    std::unordered_set<uint16_t> label_ids = get_vertex_labels(labels, true);

    const auto vertex_id = utils::atomic_fetch_inc(&read_graph_metadata()->vertices_c);

    for (const auto label_id : label_ids)
        utils::atomic_fetch_inc(&read_vertex_label_entry(label_id)->item_c);

    const auto entry_offset = store_vertex_entry(vertex_id, label_ids, props);
    store_index_entry(vertex_id, label_ids, entry_offset);

    return EActionState_t::valid;
}

graphquery::database::storage::CMemoryModelMMAPLPG::EActionState_t
graphquery::database::storage::CMemoryModelMMAPLPG::add_edge_entry(SNodeID src, SNodeID dst, const std::string_view edge_label, const std::vector<SProperty_t> & props, bool undirected) noexcept
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
    utils::atomic_fetch_inc(&src_vertex_exists.value()->payload.metadata.neighbour_c);
    utils::atomic_fetch_inc(&read_graph_metadata()->edges_c);

    if (undirected)
    {
        tail_edge   = utils::atomic_load(&dst_vertex_exists.value()->payload.edge_idx);
        edge_offset = store_edge_entry(tail_edge, dst_vertex_exists.value()->idx, src_vertex_exists.value()->idx, edge_label_id, props);

        // ~ Update vertex tail and connect edges
        utils::atomic_store(&dst_vertex_exists.value()->payload.edge_idx, edge_offset);
        utils::atomic_fetch_inc(&dst_vertex_exists.value()->payload.metadata.neighbour_c);
        utils::atomic_fetch_inc(&read_graph_metadata()->edges_c);
    }

    return EActionState_t::valid;
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::add_vertex(SNodeID src, const std::vector<std::string_view> & labels, const std::vector<SProperty_t> & prop)
{
    transaction_preamble();
    if (add_vertex_entry(src, labels, prop) == EActionState_t::invalid)
    {
        transaction_epilogue();
        m_log_system->warning(fmt::format("Issue adding vertex"));
        return;
    }

    m_transactions->commit_vertex(labels, prop, src);
    transaction_epilogue();
    utils::atomic_store(&m_sync_needed, true);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::add_edge(SNodeID src, SNodeID dst, const std::string_view edge_label, const std::vector<SProperty_t> & prop, bool undirected)
{
    transaction_preamble();
    if (add_edge_entry(src, dst, edge_label, prop, undirected) == EActionState_t::invalid)
    {
        transaction_epilogue();
        m_log_system->warning(fmt::format("Issue adding edge({}) to vertex({})", dst, src));
        return;
    }

    m_transactions->commit_edge(src, dst, edge_label, prop, undirected);
    transaction_epilogue();
    utils::atomic_store(&m_sync_needed, true);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::add_vertex(const std::vector<std::string_view> & labels, const std::vector<SProperty_t> & prop)
{
    transaction_preamble();
    (void) add_vertex_entry(labels, prop);
    m_transactions->commit_vertex(labels, prop);

    transaction_epilogue();
    utils::atomic_store(&m_sync_needed, true);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::rm_vertex(SNodeID src)
{
    transaction_preamble();
    if (rm_vertex_entry(src) == EActionState_t::invalid)
    {
        transaction_epilogue();
        m_log_system->warning(fmt::format("Issue removing vertex({})", src));
        return;
    }

    m_transactions->commit_rm_vertex(src);
    transaction_epilogue();
    utils::atomic_store(&m_sync_needed, true);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::rm_edge(SNodeID src, SNodeID dst)
{
    transaction_preamble();
    if (rm_edge_entry(src, dst) == EActionState_t::invalid)
    {
        transaction_epilogue();
        m_log_system->warning(fmt::format("Issue remvoing edge({}) to vertex({})", dst, src));
        return;
    }

    m_transactions->commit_rm_edge(src, dst);
    transaction_epilogue();
    utils::atomic_store(&m_sync_needed, true);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::rm_edge(SNodeID src, SNodeID dst, const std::string_view edge_label)
{
    transaction_preamble();
    if (rm_edge_entry(src, dst, edge_label) == EActionState_t::invalid)
    {
        transaction_epilogue();
        m_log_system->warning(fmt::format("Issue remvoing edge({}) to vertex({})", dst, src));
        return;
    }

    m_transactions->commit_rm_edge(src, dst, edge_label);
    transaction_epilogue();
    utils::atomic_store(&m_sync_needed, true);
}

std::optional<graphquery::database::storage::ILPGModel::SVertex_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertex(SNodeID src)
{
    auto ptr = get_vertex_by_id(src);

    if (!ptr.has_value())
        return std::nullopt;

    return ptr->ref->payload.metadata;
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

        if (unlikely(curr_vertex_ptr->state == 0))
            continue;

        if (pred(curr_vertex_ptr->payload.metadata))
            ret.emplace_back(curr_vertex_ptr->payload.metadata);
    }

    ret.shrink_to_fit();
    return ret;
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges(const std::function<bool(const SEdge_t &)> & pred)
{
    const uint32_t datablock_c = utils::atomic_load(&m_edges_file.read_metadata()->data_block_c);

    std::vector<SEdge_t> ret = {};
    ret.reserve(datablock_c);

    for (uint32_t i = 0; i < datablock_c; i++)
    {
        auto curr_edge_ptr = m_edges_file.read_entry(i);

        if (unlikely(!curr_edge_ptr->state.any()))
            continue;

        for (size_t j = 0; j < curr_edge_ptr->state.size(); j++)
        {
            if (likely(curr_edge_ptr->state.test(j)))
            {
                if (pred(curr_edge_ptr->payload[j].metadata))
                    ret.emplace_back(curr_edge_ptr->payload[j].metadata);
            }
        }
    }

    ret.shrink_to_fit();
    return ret;
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges(const std::string_view vertex_label, const std::string_view edge_label, SNodeID dst)
{
    const auto dst_vertex_exists = get_vertex_by_id(dst);

    if (!dst_vertex_exists.has_value())
        return {};

    auto dst_vertex_ptr = dst_vertex_exists.value();

    return get_edges(vertex_label, edge_label, [&dst_vertex_ptr](const SEdge_t & edge) -> bool { return edge.dst == dst_vertex_ptr->idx; });
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges(const uint32_t vertex_id, std::string_view edge_label, std::string_view vertex_label)
{
    const auto edge_label_exists   = check_if_edge_label_exists(edge_label);
    const auto vertex_label_exists = check_if_vertex_label_exists(vertex_label);

    if (!(edge_label_exists.has_value() && vertex_label_exists.has_value()))
        return {};

    const auto edge_label_id   = edge_label_exists.value();
    const auto vertex_label_id = vertex_label_exists.value();

    return get_edges_by_offset(vertex_id,
                               [this, &edge_label_id, &vertex_label_id](const SEdge_t & edge) -> bool
                               {
                                   auto dst_vertex_ptr = get_vertex_by_offset(edge.dst);

                                   if (dst_vertex_ptr.has_value())
                                       return false;

                                   return dst_vertex_ptr.value()->payload.metadata.label_id == vertex_label_id && edge.edge_label_id == edge_label_id;
                               });
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges(const std::string_view vertex_label, const std::function<bool(const SEdge_t &)> & pred)
{
    std::vector<int64_t> label_vertices = get_vertices_offset_by_label(vertex_label);
    std::vector<SEdge_t> ret;

    for (const auto vertex_offset : label_vertices)
    {
        auto edges = get_edges_by_offset(vertex_offset, pred);
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

    std::vector<int64_t> label_vertices = get_vertices_offset_by_label(vertex_label);
    std::vector<SEdge_t> ret;

    auto label_pred = [&label_id, &pred](const SEdge_t & edge) -> bool { return edge.edge_label_id == label_id && pred(edge); };

    for (const auto vertex_offset : label_vertices)
    {
        auto edges = get_edges_by_offset(vertex_offset, label_pred);
        ret.insert(ret.begin(), edges.begin(), edges.end());
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

    const auto neighbours = utils::atomic_load(&vertex_ptr.value()->payload.metadata.neighbour_c);
    ret.reserve(neighbours);

    uint32_t curr = utils::atomic_load(&vertex_ptr.value()->payload.edge_idx);

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

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges_by_offset(const uint32_t src_vertex_id, const uint32_t dst_vertex_id)
{
    auto src_vertex_ptr      = get_vertex_by_offset(src_vertex_id);
    auto dst_vertex_ptr      = get_vertex_by_offset(dst_vertex_id);
    std::vector<SEdge_t> ret = {};

    if (!(src_vertex_ptr.has_value() && dst_vertex_ptr.has_value()))
        return ret;

    ret.reserve(utils::atomic_load(&src_vertex_ptr.value()->payload.metadata.neighbour_c));
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

    ret.reserve(utils::atomic_load(&src_vertex_ptr.value()->payload.metadata.neighbour_c));
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
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges_by_id(const int64_t src, const std::function<bool(const SEdge_t &)> & pred)
{
    auto vertex_ptr          = get_vertex_by_id(src);
    std::vector<SEdge_t> ret = {};

    if (unlikely(!vertex_ptr.has_value()))
        return ret;

    ret.reserve(utils::atomic_load(&vertex_ptr->ref->payload.metadata.neighbour_c));
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

void
graphquery::database::storage::CMemoryModelMMAPLPG::edgemap(const std::unique_ptr<analytic::IRelax> & relax) noexcept
{
    const auto datablock_c = utils::atomic_load(&m_vertices_file.read_metadata()->data_block_c);
    auto curr_vertex_ptr   = m_vertices_file.read_entry(0);

    for (uint32_t i = 0; i < datablock_c; i++, ++curr_vertex_ptr)
    {
        if (unlikely(!curr_vertex_ptr->state.any()))
            continue;

        uint32_t edge_ref = utils::atomic_load(&curr_vertex_ptr->payload.edge_idx);

        while (edge_ref != END_INDEX)
        {
            auto curr_edge_ptr = m_edges_file.read_entry(edge_ref);

            for (size_t j = 0; j < curr_edge_ptr->state.size(); j++)
            {
                if (likely(curr_edge_ptr->state.test(j)))
                    relax->relax(curr_vertex_ptr->payload.metadata.id, curr_edge_ptr->payload[j].metadata.dst);
            }
            edge_ref = curr_edge_ptr->next;
        }
    }
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges(SNodeID src, SNodeID dst)
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
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges(SNodeID src, const std::string_view edge_label, const std::string_view vertex_label)
{
    const auto src_vertex_ptr      = get_vertex_by_id(src);
    const auto vertex_label_exists = check_if_vertex_label_exists(vertex_label);
    const auto edge_label_exists   = check_if_edge_label_exists(edge_label);

    if (!(src_vertex_ptr.has_value() && vertex_label_exists.has_value() && edge_label_exists.has_value()))
        return {};

    const uint32_t src_vertex_idx  = utils::atomic_load(&src_vertex_ptr->ref->idx);
    const uint16_t vertex_label_id = vertex_label_exists.value();
    const uint16_t edge_label_id   = edge_label_exists.value();

    return get_edges_by_offset(src_vertex_idx,
                               edge_label_id,
                               [this, &vertex_label_id](const SEdge_t & edge) -> bool
                               {
                                   auto dst_vertex_ptr = get_vertex_by_offset(edge.dst).value();

                                   if (!dst_vertex_ptr->state.any())
                                       return false;

                                   return dst_vertex_ptr->payload.metadata.label_id == vertex_label_id;
                               });
}

std::unordered_set<int64_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edge_dst_vertices(SNodeID src, [[maybe_unused]] const std::function<bool(const SEdge_t &)> & pred)
{
    SRef_t<SVertexDataBlock> vertex_ptr = {};

    if (const auto exists = get_vertex_by_id(src); unlikely(!exists.has_value()))
        return {};
    else
        vertex_ptr = exists.value();

    std::unordered_set<int64_t> ret = {};
    const auto neighbours           = utils::atomic_load(&vertex_ptr->payload.metadata.neighbour_c);
    ret.reserve(neighbours);

    uint32_t curr = utils::atomic_load(&vertex_ptr->payload.edge_idx);

    while (curr != END_INDEX)
    {
        auto curr_edge_ptr = m_edges_file.read_entry(curr);

        for (size_t i = 0; i < curr_edge_ptr->state.size(); i++)
        {
            if (pred(curr_edge_ptr->payload[i].metadata))
                ret.insert(curr_edge_ptr->payload[i].metadata.dst);
        }
        curr = curr_edge_ptr->next;
    }
    return ret;
}

std::unordered_set<int64_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edge_dst_vertices(SNodeID src, const std::string_view edge_label, const std::string_view vertex_label)
{
    std::optional<uint16_t> vertex_label_exists = check_if_vertex_label_exists(vertex_label);
    std::optional<uint16_t> edge_label_exists   = check_if_edge_label_exists(edge_label);

    if (!(vertex_label_exists.has_value() && edge_label_exists.has_value()))
        return {};

    auto vertex_label_id = vertex_label_exists.value();
    auto edge_label_id   = edge_label_exists.value();

    return get_edge_dst_vertices(src,
                                 [this, &vertex_label_id, &edge_label_id](const SEdge_t & edge) -> bool
                                 {
                                     std::optional<SRef_t<SVertexDataBlock>> dst_vertex_ptr = get_vertex_by_offset(edge.dst);

                                     if (unlikely(!dst_vertex_ptr.has_value()))
                                         return false;

                                     if (!dst_vertex_ptr.value()->state.any())
                                         return false;

                                     auto label_head = dst_vertex_ptr->ref->payload.metadata.label_id;
                                     while (label_head != END_INDEX)
                                     {
                                         auto label_ptr = m_label_ref_file.read_entry(label_head);

                                         for (size_t i = 0; i < label_ptr->state.size(); i++)
                                         {
                                             if (label_ptr->state.test(i) && label_ptr->payload[i] == vertex_label_id && edge.edge_label_id == edge_label_id)
                                                 return true;
                                         }
                                         label_head = label_ptr->next;
                                     }

                                     return false;
                                 });
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_recursive_edges(SNodeID src, const std::vector<SProperty_t> edge_vertex_label_pairs)
{
    const auto src_vertex_ptr = get_vertex_by_id(src);
    std::vector<SEdge_t> ret  = {};

    if (!src_vertex_ptr.has_value())
        return ret;

    std::unordered_set<int64_t> vertices_to_process = {src_vertex_ptr->ref->idx};

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
graphquery::database::storage::CMemoryModelMMAPLPG::get_properties_by_vertex(SNodeID src)
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
graphquery::database::storage::CMemoryModelMMAPLPG::get_properties_by_vertex_map(SNodeID src)
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
    SRef_t<SVertexDataBlock> vertex_ptr              = m_vertices_file.read_entry(static_cast<int64_t>(id));
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

void
graphquery::database::storage::CMemoryModelMMAPLPG::calc_outdegree(const std::shared_ptr<uint32_t[]> outdeg) noexcept
{
    int64_t vertex_c       = 0;
    const auto datablock_c = utils::atomic_load(&m_vertices_file.read_metadata()->data_block_c);
    auto curr_vertex_ptr   = m_vertices_file.read_entry(0);

    for (uint32_t i = 0; i < datablock_c; i++, ++curr_vertex_ptr)
    {
        if (unlikely(!curr_vertex_ptr->state.any()))
            continue;

        outdeg[vertex_c++] = utils::atomic_load(&curr_vertex_ptr->payload.metadata.neighbour_c);
    }
}

std::optional<graphquery::database::storage::SRef_t<graphquery::database::storage::CMemoryModelMMAPLPG::SVertexDataBlock>>
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertex_by_offset(const uint32_t offset) noexcept
{
    const uint32_t block_c = utils::atomic_load(&m_vertices_file.read_metadata()->data_block_c);

    if (offset >= block_c)
        return std::nullopt;

    return m_vertices_file.read_entry(offset);
}

std::optional<graphquery::database::storage::SRef_t<graphquery::database::storage::CMemoryModelMMAPLPG::SVertexDataBlock>>
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertex_by_id(const SNodeID id) noexcept
{
    auto index_ptr = m_index_file.read_entry(id);

    if (index_ptr->id != id)
        return std::nullopt;

    return m_vertices_file.read_entry(index_ptr->offset);
}

graphquery::database::storage::SRef_t<graphquery::database::storage::CMemoryModelMMAPLPG::SGraphMetaData_t>
graphquery::database::storage::CMemoryModelMMAPLPG::read_graph_metadata() noexcept
{
    return m_master_file.ref<SGraphMetaData_t>(METADATA_START_ADDR);
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

    for (uint16_t i = 0; i < label_c; i++, ++label_ptr)
        m_e_label_map[label_ptr->label_s] = label_ptr->label_id;
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::read_index_list() noexcept
{
    define_luts();
    const uint32_t block_c = utils::atomic_load(&m_vertices_file.read_metadata()->data_block_c);
    auto vertex_ptr        = m_vertices_file.read_entry(0);

    for (uint32_t vertex_i = 0; vertex_i < block_c; vertex_i++, ++vertex_ptr)
    {
        if (unlikely(!vertex_ptr->state.any()))
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

graphquery::database::storage::SRef_t<graphquery::database::storage::CMemoryModelMMAPLPG::SLabel_t>
graphquery::database::storage::CMemoryModelMMAPLPG::read_vertex_label_entry(const uint32_t offset) noexcept
{
    static const auto base_addr  = read_graph_metadata()->vertex_label_table_addr;
    static const auto label_size = read_graph_metadata()->label_size;
    const auto effective_addr    = base_addr + (label_size * offset);
    return m_master_file.ref<SLabel_t>(effective_addr);
}

graphquery::database::storage::SRef_t<graphquery::database::storage::CMemoryModelMMAPLPG::SLabel_t>
graphquery::database::storage::CMemoryModelMMAPLPG::read_edge_label_entry(const uint32_t offset) noexcept
{
    static const auto base_addr  = read_graph_metadata()->edge_label_table_addr;
    static const auto label_size = read_graph_metadata()->label_size;
    const auto effective_addr    = base_addr + (label_size * offset);
    return m_master_file.ref<SLabel_t>(effective_addr);
}

graphquery::database::storage::CMemoryModelMMAPLPG::EActionState_t
graphquery::database::storage::CMemoryModelMMAPLPG::rm_vertex_entry(SNodeID src) noexcept
{
    SRef_t<SVertexDataBlock> vertex_ptr = {};

    if (auto vertex_opt = get_vertex_by_id(src); unlikely(!vertex_opt.has_value()))
        return EActionState_t::invalid;
    else
        vertex_ptr = std::move(vertex_opt.value());

    //~ Mark deletion for each edge block connected to the vertex
    const auto head_edge_idx = utils::atomic_load(&vertex_ptr->payload.edge_idx);
    m_edges_file.foreach_block(head_edge_idx, [this](SRef_t<SEdgeDataBlock> & edge_block_ptr) -> void { m_edges_file.append_free_data_block(edge_block_ptr->idx); });

    //~ Mark deletion for vertex
    const auto vertex_idx = utils::atomic_load(&vertex_ptr->idx);
    m_vertices_file.append_free_data_block(vertex_idx);
    return EActionState_t::valid;
}

graphquery::database::storage::CMemoryModelMMAPLPG::EActionState_t
graphquery::database::storage::CMemoryModelMMAPLPG::rm_edge_entry(SNodeID src, SNodeID dst) noexcept
{
    std::optional<SRef_t<SVertexDataBlock>> src_vertex_ptr = get_vertex_by_id(src);
    std::optional<SRef_t<SVertexDataBlock>> dst_vertex_ptr = get_vertex_by_id(dst);

    if (unlikely(!(src_vertex_ptr.has_value() && dst_vertex_ptr.has_value())))
        return EActionState_t::invalid;

    const uint32_t head_edge_idx = utils::atomic_load(&src_vertex_ptr.value()->payload.edge_idx);
    const auto dst_idx           = utils::atomic_load(&dst_vertex_ptr.value()->idx);

    m_edges_file.foreach_block(head_edge_idx,
                               [dst_idx](SRef_t<SEdgeDataBlock> & edge_block_ptr) -> void
                               {
                                   for (size_t i = 0; i < edge_block_ptr->state.size(); i++)
                                   {
                                       if (edge_block_ptr->state.test(i))
                                       {
                                           if (edge_block_ptr->payload[i].metadata.dst == dst_idx)
                                               edge_block_ptr->state[i].flip();
                                       }
                                   }
                               });

    return EActionState_t::valid;
}

graphquery::database::storage::CMemoryModelMMAPLPG::EActionState_t
graphquery::database::storage::CMemoryModelMMAPLPG::rm_edge_entry(SNodeID src, SNodeID dst, const std::string_view edge_label) noexcept
{
    std::optional<SRef_t<SVertexDataBlock>> src_vertex_ptr = get_vertex_by_id(src);
    std::optional<SRef_t<SVertexDataBlock>> dst_vertex_ptr = get_vertex_by_id(dst);
    std::optional<uint16_t> edge_label_id                  = check_if_vertex_label_exists(edge_label);

    if (unlikely(!(src_vertex_ptr.has_value() && dst_vertex_ptr.has_value() && edge_label_id.value())))
        return EActionState_t::invalid;

    const uint32_t head_edge_idx = utils::atomic_load(&src_vertex_ptr.value()->payload.edge_idx);
    const auto dst_idx           = utils::atomic_load(&dst_vertex_ptr.value()->idx);

    m_edges_file.foreach_block(head_edge_idx,
                               [dst_idx, &edge_label_id](SRef_t<SEdgeDataBlock> & edge_block_ptr) -> void
                               {
                                   for (size_t i = 0; i < edge_block_ptr->state.size(); i++)
                                   {
                                       if (edge_block_ptr->state.test(i))
                                       {
                                           if (edge_block_ptr->payload[i].metadata.dst == dst_idx && edge_block_ptr->payload[i].metadata.edge_label_id == edge_label_id.value())
                                               edge_block_ptr->state[i].flip();
                                       }
                                   }
                               });

    return EActionState_t::valid;
}

int64_t
graphquery::database::storage::CMemoryModelMMAPLPG::get_unassigned_vertex_id(const size_t label_idx) const noexcept
{
    return label_idx;
}

int64_t
graphquery::database::storage::CMemoryModelMMAPLPG::get_num_edges()
{
    return utils::atomic_load(&read_graph_metadata()->edges_c);
}

int64_t
graphquery::database::storage::CMemoryModelMMAPLPG::get_num_vertices()
{
    return utils::atomic_load(&read_graph_metadata()->vertices_c);
}

extern "C"
{
    LIB_EXPORT void create_graph_model(graphquery::database::storage::ILPGModel ** graph_model, const std::shared_ptr<graphquery::logger::CLogSystem> & log_system)
    {
        assert(log_system != nullptr);
        *graph_model = new graphquery::database::storage::CMemoryModelMMAPLPG(log_system);
    }
}