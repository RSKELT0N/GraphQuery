#include "lpg_mmap.h"

#include "db/utils/lib.h"

#include <string_view>
#include <optional>
#include <ranges>
#include <vector>
#include <set>

graphquery::database::storage::CMemoryModelMMAPLPG::
CMemoryModelMMAPLPG(): m_sync_needed(false), m_syncing(0), m_transaction_ref_c(0)
{
    m_log_system   = logger::CLogSystem::get_instance();
    m_label_vertex = std::vector<std::vector<uint64_t>>();
    m_unq_lock     = std::unique_lock(m_sync_lock);
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
        (void) m_global_index_file.get_file().async();
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

    this->m_master_file.set_path(path);

    m_transactions = std::make_shared<CTransaction>(path, this);

    //~ Create initial model files
    (void) CDiskDriver::create_file(path, MASTER_FILE_NAME);

    //~ Open mapping to model files
    (void) this->m_master_file.open(MASTER_FILE_NAME);
    this->m_global_index_file.open(path, INDEX_FILE_NAME, true);
    this->m_vertices_file.open(path, VERTICES_FILE_NAME, true);
    this->m_edges_file.open(path, EDGES_FILE_NAME, true);
    this->m_properties_file.open(path, PROPERTIES_FILE_NAME, true);

    //~ Initialise graph memory.
    m_transactions->init();

    store_graph_metadata();
    m_global_index_file.store_metadata();
    m_vertices_file.store_metadata();
    m_edges_file.store_metadata();
    m_properties_file.store_metadata();
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::load_graph(std::filesystem::path path, std::string_view graph) noexcept
{
    this->m_graph_name = graph;
    this->m_graph_path = path;

    this->m_master_file.set_path(path);

    m_transactions = std::make_shared<CTransaction>(path, this);

    //~ Open mapping to model files
    (void) this->m_master_file.open(MASTER_FILE_NAME);
    this->m_global_index_file.open(path, INDEX_FILE_NAME, false);
    this->m_vertices_file.open(path, VERTICES_FILE_NAME, false);
    this->m_edges_file.open(path, EDGES_FILE_NAME, false);
    this->m_properties_file.open(path, PROPERTIES_FILE_NAME, false);

    //~ Load graph memory.
    m_transactions->init();
    read_index_list();
    m_transactions->handle_transactions();
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
graphquery::database::storage::CMemoryModelMMAPLPG::store_vertex_entry(const uint64_t id, const uint16_t label_id, const std::vector<SProperty_t> & props, const uint32_t next_ref) noexcept
{
    SRef_t<SVertexDataBlock> data_block_ptr = m_vertices_file.attain_data_block(next_ref);
    const uint32_t entry_offset             = data_block_ptr->idx;

    data_block_ptr->state.set(0) = true;
    data_block_ptr->version      = END_INDEX;

    data_block_ptr->payload.edge_idx              = END_INDEX;
    data_block_ptr->payload.metadata.id           = id;
    data_block_ptr->payload.metadata.label_id     = label_id;
    data_block_ptr->payload.metadata.neighbour_c  = 0;
    data_block_ptr->payload.metadata.property_c   = props.size();
    data_block_ptr->payload.metadata.edge_label_c = 0;

    uint32_t next_props_ref = END_INDEX;

    for (const SProperty_t & prop : props)
        next_props_ref = store_property_entry(prop, next_props_ref);

    data_block_ptr->payload.metadata.property_id = next_props_ref;

    return entry_offset;
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::store_index_entry(uint64_t id, const uint16_t label_id, const uint32_t vertex_offset) noexcept
{
    m_global_index_file.store_entry(id, vertex_offset);
    m_label_vertex[label_id].emplace_back(id);
}

uint32_t
graphquery::database::storage::CMemoryModelMMAPLPG::store_edge_entry(const uint32_t next_ref,
                                                                     const uint64_t src,
                                                                     const uint64_t dst,
                                                                     const uint16_t dst_label_id,
                                                                     const uint16_t edge_label_id,
                                                                     const std::vector<SProperty_t> & props) noexcept
{
    SRef_t<SEdgeDataBlock> data_block_ptr = m_edges_file.attain_data_block(next_ref);
    const auto entry_offset               = data_block_ptr->idx;

    size_t payload_offset = 0;
    for (int i = 0; i < data_block_ptr->state.size(); i++)
    {
        if (data_block_ptr->state.test(i) == 0)
        {
            payload_offset = i;
            data_block_ptr->state.set(i);
            break;
        }
    }

    data_block_ptr->state[payload_offset]                          = true;
    data_block_ptr->payload[payload_offset].metadata.dst           = dst;
    data_block_ptr->payload[payload_offset].metadata.dst_label_id  = dst_label_id;
    data_block_ptr->payload[payload_offset].metadata.src           = src;
    data_block_ptr->payload[payload_offset].metadata.edge_label_id = edge_label_id;
    data_block_ptr->payload[payload_offset].metadata.property_c    = props.size();

    uint32_t next_props_ref = EIndexValue_t::END_INDEX;

    for (const SProperty_t & prop : props)
        next_props_ref = store_property_entry(prop, next_props_ref);

    data_block_ptr->payload[payload_offset].metadata.property_id = next_props_ref;
    return entry_offset;
}

uint32_t
graphquery::database::storage::CMemoryModelMMAPLPG::store_property_entry(const SProperty_t & prop, const uint32_t next_ref) noexcept
{
    SRef_t<SPropertyDataBlock> data_block_ptr = m_properties_file.attain_data_block(next_ref);
    const uint32_t entry_offset               = data_block_ptr->idx;

    size_t payload_offset = 0;
    for (size_t i = 0; i < data_block_ptr->state.size(); i++)
    {
        if (data_block_ptr->state.test(i) == 0)
        {
            payload_offset = i;
            data_block_ptr->state.set(i);
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
    auto graph_metadata         = read_graph_metadata();
    const uint16_t label_id     = utils::atomic_fetch_inc(&graph_metadata->edge_label_c);
    const uint64_t label_offset = EDGE_LABELS_START_ADDR + label_id * sizeof(SLabel_t);
    const auto label_ptr        = m_master_file.ref<SLabel_t>(label_offset);

    strncpy(&label_ptr.ref->label_s[0], label_str.data(), 20);
    label_ptr.ref->item_c   = 0;
    label_ptr.ref->label_id = label_id;
    return label_id;
}

uint16_t
graphquery::database::storage::CMemoryModelMMAPLPG::create_vertex_label(const std::string_view label_str) noexcept
{
    auto graph_metadata         = read_graph_metadata();
    const uint16_t label_id     = utils::atomic_fetch_inc(&graph_metadata->vertex_label_c);
    const uint64_t label_offset = VERTEX_LABELS_START_ADDR + label_id * sizeof(SLabel_t);
    const auto label_ptr        = m_master_file.ref<SLabel_t>(label_offset);

    strncpy(&label_ptr.ref->label_s[0], label_str.data(), 20);
    label_ptr.ref->item_c   = 0;
    label_ptr.ref->label_id = label_id;
    m_label_vertex.emplace_back();
    m_label_map[label_str.data()] = label_id;

    return label_id;
}

std::optional<uint16_t>
graphquery::database::storage::CMemoryModelMMAPLPG::check_if_edge_label_exists(const std::string_view label_str) noexcept
{
    const auto edge_labels_amt = utils::atomic_load(&read_graph_metadata()->edge_label_c);
    auto label_ptr             = m_master_file.ref<SLabel_t>(EDGE_LABELS_START_ADDR);

    for (int i = 0; i < edge_labels_amt; i++, label_ptr.ref++)
    {
        if (strcmp(label_str.data(), label_ptr.ref->label_s) == 0)
            return label_ptr.ref->label_id;
    }
    return std::nullopt;
}

std::optional<uint16_t>
graphquery::database::storage::CMemoryModelMMAPLPG::check_if_vertex_label_exists(const std::string_view label_str) noexcept
{
    if (!m_label_map.contains(label_str.data()))
        return std::nullopt;

    return m_label_map.at(label_str.data());
}

graphquery::database::storage::CMemoryModelMMAPLPG::EActionState_t
graphquery::database::storage::CMemoryModelMMAPLPG::add_vertex_entry(const uint64_t id, const std::string_view label, const std::vector<SProperty_t> & props) noexcept
{
    const std::optional<uint16_t> vertex_label_exists = check_if_vertex_label_exists(label);
    const auto vertex_label_id                        = vertex_label_exists.has_value() ? vertex_label_exists.value() : create_vertex_label(label);

    if (get_vertex_by_id(id, vertex_label_id).has_value())
        return EActionState_t::invalid;

    utils::atomic_fetch_inc(&read_graph_metadata()->vertices_c);
    utils::atomic_fetch_inc(&read_vertex_label_entry(vertex_label_id)->item_c);

    const auto vertex_head_offset = get_vertex_head_offset(id);
    const auto entry_offset       = store_vertex_entry(id, vertex_label_id, props, vertex_head_offset);
    store_index_entry(id, vertex_label_id, entry_offset);
    return EActionState_t::valid;
}

graphquery::database::storage::CMemoryModelMMAPLPG::EActionState_t
graphquery::database::storage::CMemoryModelMMAPLPG::add_vertex_entry(const std::string_view label, const std::vector<SProperty_t> & props) noexcept
{
    const std::optional<uint16_t> vertex_label_exists = check_if_vertex_label_exists(label);
    const auto vertex_label_id                        = vertex_label_exists.has_value() ? vertex_label_exists.value() : create_vertex_label(label);

    const auto vertex_id = utils::atomic_fetch_inc(&read_graph_metadata()->vertices_c);
    utils::atomic_fetch_inc(&read_vertex_label_entry(vertex_label_id)->item_c);

    const auto entry_offset = store_vertex_entry(vertex_id, vertex_label_id, props, END_INDEX);
    store_index_entry(vertex_id, vertex_label_id, entry_offset);

    return EActionState_t::valid;
}

graphquery::database::storage::CMemoryModelMMAPLPG::EActionState_t
graphquery::database::storage::CMemoryModelMMAPLPG::add_edge_entry(const SNodeID & src, const SNodeID & dst, const std::string_view edge_label, const std::vector<SProperty_t> & props) noexcept
{
    const auto src_label_id = check_if_vertex_label_exists(src.label);
    const auto dst_label_id = check_if_vertex_label_exists(dst.label);

    if (!(src_label_id.has_value() && dst_label_id.has_value()))
        return EActionState_t::invalid;

    SRef_t<SVertexDataBlock> vertex_src_ptr = {};
    SRef_t<SVertexDataBlock> vertex_dst_ptr = {};

    auto src_vertex_exists = get_vertex_by_id(src.id, src_label_id.value());
    auto dst_vertex_exists = get_vertex_by_id(dst.id, dst_label_id.value());

    if (!(src_vertex_exists.has_value() && dst_vertex_exists.has_value()))
        return EActionState_t::invalid;

    vertex_src_ptr = std::move(src_vertex_exists.value());
    vertex_dst_ptr = std::move(dst_vertex_exists.value());

    const std::optional<uint16_t> edge_label_exists = check_if_edge_label_exists(edge_label);
    const auto edge_label_id                        = edge_label_exists.has_value() ? edge_label_exists.value() : create_edge_label(edge_label);

    const auto tail_edge   = utils::atomic_load(&vertex_src_ptr->payload.edge_idx);
    const auto edge_offset = store_edge_entry(tail_edge, vertex_src_ptr->idx, vertex_dst_ptr->idx, dst_label_id.value(), edge_label_id, props);

    // ~ Update vertex tail and connect edges
    utils::atomic_store(&vertex_src_ptr->payload.edge_idx, edge_offset);
    utils::atomic_fetch_inc(&vertex_src_ptr->payload.metadata.neighbour_c);
    utils::atomic_fetch_inc(&read_graph_metadata()->edges_c);

    return EActionState_t::valid;
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::add_vertex(const SNodeID & src, const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop)
{
    transaction_preamble();
    const auto transformed_properties = transform_properties(prop);
    if (add_vertex_entry(src.id, src.label, transformed_properties) == EActionState_t::invalid)
    {
        transaction_epilogue();
        m_log_system->warning(fmt::format("Issue adding vertex"));
        return;
    }

    m_transactions->commit_vertex(src.label, transformed_properties, src.id);
    transaction_epilogue();
    utils::atomic_store(&m_sync_needed, true);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::add_edge(const SNodeID & src,
                                                             const SNodeID & dst,
                                                             const std::string_view edge_label,
                                                             const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop)
{
    transaction_preamble();
    const auto transformed_properties = transform_properties(prop);
    if (add_edge_entry(src, dst, edge_label, transformed_properties) == EActionState_t::invalid)
    {
        transaction_epilogue();
        m_log_system->warning(fmt::format("Issue adding edge({}) to vertex({})", dst.id, src.id));
        return;
    }

    m_transactions->commit_edge(src, dst, edge_label, transformed_properties);
    transaction_epilogue();
    utils::atomic_store(&m_sync_needed, true);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::add_vertex(const std::string_view label, const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop)
{
    transaction_preamble();
    const auto transformed_properties = transform_properties(prop);
    (void) add_vertex_entry(label, transformed_properties);
    m_transactions->commit_vertex(label, transformed_properties);

    transaction_epilogue();
    utils::atomic_store(&m_sync_needed, true);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::rm_vertex(const SNodeID & src)
{
    transaction_preamble();
    if (rm_vertex_entry(src) == EActionState_t::invalid)
    {
        transaction_epilogue();
        m_log_system->warning(fmt::format("Issue removing vertex({})", src.id));
        return;
    }

    m_transactions->commit_rm_vertex(src);
    transaction_epilogue();
    utils::atomic_store(&m_sync_needed, true);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::rm_edge(const SNodeID & src, const SNodeID & dst)
{
    transaction_preamble();
    if (rm_edge_entry(src, dst) == EActionState_t::invalid)
    {
        transaction_epilogue();
        m_log_system->warning(fmt::format("Issue remvoing edge({}) to vertex({})", dst.id, src.id));
        return;
    }

    m_transactions->commit_rm_edge(src, dst);
    transaction_epilogue();
    utils::atomic_store(&m_sync_needed, true);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::rm_edge(const SNodeID & src, const SNodeID & dst, const std::string_view edge_label)
{
    transaction_preamble();
    if (rm_edge_entry(src, dst, edge_label) == EActionState_t::invalid)
    {
        transaction_epilogue();
        m_log_system->warning(fmt::format("Issue remvoing edge({}) to vertex({})", dst.id, src.id));
        return;
    }

    m_transactions->commit_rm_edge(src, dst, edge_label);
    transaction_epilogue();
    utils::atomic_store(&m_sync_needed, true);
}

std::optional<graphquery::database::storage::ILPGModel::SVertex_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertex(const SNodeID & src)
{
    const auto src_label_id = check_if_vertex_label_exists(src.label);

    if (!src_label_id.has_value())
        return std::nullopt;

    std::optional<SRef_t<SVertexDataBlock>> exists = get_vertex_by_id(src.id, src_label_id.value());

    if (!exists.has_value())
        return std::nullopt;

    return exists.value()->payload.metadata;
}

std::vector<graphquery::database::storage::ILPGModel::SVertex_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertices(const std::function<bool(const SVertex_t &)> pred)
{
    const uint64_t datablock_c = utils::atomic_load(&m_vertices_file.read_metadata()->data_block_c);

    std::vector<SVertex_t> ret = {};
    ret.reserve(datablock_c);

    for (uint64_t i = 0; i < datablock_c; i++)
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
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges(const std::function<bool(const SEdge_t &)> pred)
{
    const uint64_t datablock_c = utils::atomic_load(&m_edges_file.read_metadata()->data_block_c);

    std::vector<SEdge_t> ret = {};
    ret.reserve(datablock_c);

    for (uint64_t i = 0; i < datablock_c; i++)
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
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges(const std::string_view vertex_label, std::function<bool(const SEdge_t &)> pred)
{
    std::vector<SVertex_t> label_vertices = get_vertices_by_label(vertex_label);
    std::vector<SEdge_t> ret;

    for (const auto & vertex : label_vertices)
    {
        auto edges = get_edges(vertex.id, vertex.label_id, pred);
        ret.insert(ret.begin(), edges.begin(), edges.end());
    }

    return ret;
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges(const std::string_view vertex_label, const std::string_view edge_label, const std::function<bool(const SEdge_t &)> pred)
{
    uint16_t label_id = {};

    if (const std::optional<uint16_t> exists = check_if_edge_label_exists(edge_label); !exists.has_value())
        return {};
    else
        label_id = exists.value();

    std::vector<SVertex_t> label_vertices = get_vertices_by_label(vertex_label);
    std::vector<SEdge_t> ret;

    auto label_pred = [&label_id, &pred](const SEdge_t & edge) -> bool { return edge.edge_label_id == label_id && pred(edge); };

    for (const auto & vertex : label_vertices)
    {
        auto edges = get_edges(vertex.id, vertex.label_id, label_pred);
        ret.insert(ret.begin(), edges.begin(), edges.end());
    }

    return ret;
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges(const uint64_t src, const uint16_t src_label_id, const std::function<bool(const SEdge_t &)> pred)
{
    const auto exists = get_vertex_by_id(src, src_label_id);

    if (unlikely(!exists.has_value()))
        return {};

    auto vertex_ptr = exists.value();

    std::vector<SEdge_t> ret = {};
    const auto neighbours    = utils::atomic_load(&vertex_ptr->payload.metadata.neighbour_c);
    ret.reserve(neighbours);

    uint64_t curr = utils::atomic_load(&vertex_ptr->payload.edge_idx);

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
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges(const SNodeID & src, const SNodeID & dst)
{
    const auto src_label_exists = check_if_vertex_label_exists(src.label);
    const auto dst_label_exists = check_if_vertex_label_exists(dst.label);

    if (!(src_label_exists.has_value() && dst_label_exists.has_value()))
        return {};

    const auto dst_label_id = dst_label_exists.value();

    return get_edges(src.id, src_label_exists.value(), [&dst, &dst_label_id](const SEdge_t & edge) -> bool { return edge.dst == dst.id && edge.dst_label_id == dst_label_id; });
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

    const uint16_t label_id = exists.has_value();

    std::vector<SVertex_t> ret = {};
    ret.resize(m_label_vertex[label_id].size());

    for (const uint64_t id : m_label_vertex[label_id])
    {
        auto vertex_ptr = get_vertex_by_id(id, label_id).value();
        ret.emplace_back(vertex_ptr->payload.metadata);
    }

    return ret;
}

std::optional<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edge(const SNodeID & src, const SNodeID & dst, const std::string_view edge_label)
{
    const auto src_label_exists  = check_if_vertex_label_exists(src.label);
    const auto dst_label_exists  = check_if_vertex_label_exists(dst.label);
    const auto edge_label_exists = check_if_edge_label_exists(edge_label);

    if (!(dst_label_exists.has_value() && src_label_exists.has_value() && edge_label_exists.has_value()))
        return {};

    const auto edge_label_id = edge_label_exists.value();
    const auto dst_label_id  = dst_label_exists.value();

    auto edges =
        get_edges(src.id,
                  src_label_exists.value(),
                  [&dst, edge_label_id, dst_label_id](const SEdge_t & edge) -> bool { return edge.dst == dst.id && edge.dst_label_id == dst_label_id && edge.edge_label_id == edge_label_id; });

    if (edges.empty())
        return std::nullopt;

    return edges[0];
}

std::optional<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edge(const SNodeID & src, const std::string_view edge_label, const std::string_view vertex_label)
{
    const auto src_label_exists    = check_if_edge_label_exists(src.label);
    const auto edge_label_exists   = check_if_edge_label_exists(edge_label);
    const auto vertex_label_exists = check_if_vertex_label_exists(vertex_label);

    if (!(src_label_exists.has_value() && edge_label_exists.has_value() && vertex_label_exists.has_value()))
        return {};

    const auto src_label_id    = src_label_exists.value();
    const auto edge_label_id   = edge_label_exists.value();
    const auto vertex_label_id = vertex_label_exists.value();

    auto edges = get_edges(src.id,
                           src_label_id,
                           [this, &vertex_label_id, &edge_label_id](const SEdge_t & edge) -> bool { return edge.dst_label_id == vertex_label_id && edge.edge_label_id == edge_label_id; });

    if (edges.empty())
        return std::nullopt;

    return edges[0];
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges(const SNodeID & src, const std::string_view edge_label, const std::string_view vertex_label)
{
    const std::optional<uint16_t> src_label_exists    = check_if_vertex_label_exists(src.label);
    const std::optional<uint16_t> vertex_label_exists = check_if_vertex_label_exists(vertex_label);
    const std::optional<uint16_t> edge_label_exists   = check_if_edge_label_exists(edge_label);

    if (!(vertex_label_exists.has_value() && edge_label_exists.has_value()))
        return {};

    const auto src_label_id    = src_label_exists.value();
    const auto vertex_label_id = vertex_label_exists.value();
    const auto edge_label_id   = edge_label_exists.value();

    return get_edges(src.id,
                     src_label_id,
                     [this, &vertex_label_id, &edge_label_id](const SEdge_t & edge) -> bool
                     {
                         auto dst_vertex_ptr = get_vertex_by_id(edge.dst, edge.dst_label_id).value();

                         if (!dst_vertex_ptr->state.any())
                             return false;

                         return edge.edge_label_id == edge_label_id && dst_vertex_ptr->payload.metadata.label_id == vertex_label_id;
                     });
}

std::unordered_set<uint64_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edge_dst_vertices(const SNodeID & src, const std::function<bool(const SEdge_t &)> pred)
{
    uint16_t src_label_id;

    if (const auto src_label_exists = check_if_vertex_label_exists(src.label); !src_label_exists.has_value())
        return {};
    else
        src_label_id = src_label_exists.value();

    const auto exists = get_vertex_by_id(src.id, src_label_id);

    if (unlikely(!exists.has_value()))
        return {};

    auto vertex_ptr = exists.value();

    std::unordered_set<uint64_t> ret = {};
    const auto neighbours            = utils::atomic_load(&vertex_ptr->payload.metadata.neighbour_c);
    ret.reserve(neighbours);

    uint64_t curr = utils::atomic_load(&vertex_ptr->payload.edge_idx);

    while (curr != END_INDEX)
    {
        auto curr_edge_ptr = m_edges_file.read_entry(curr);

        for (size_t i = 0; i < curr_edge_ptr->state.size(); i++)
        {
            if (likely(curr_edge_ptr->state.test(i)))
            {
                if (pred(curr_edge_ptr->payload[i].metadata))
                    ret.emplace(curr_edge_ptr->payload[i].metadata.dst);
            }
        }

        curr = curr_edge_ptr->next;
    }

    return ret;
}

std::unordered_set<uint64_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edge_dst_vertices(const SNodeID & src, const std::string_view edge_label, const std::string_view vertex_label)
{
    const std::optional<uint16_t> vertex_label_exists = check_if_vertex_label_exists(vertex_label);
    const std::optional<uint16_t> edge_label_exists   = check_if_edge_label_exists(edge_label);

    if (!(vertex_label_exists.has_value() && edge_label_exists.has_value()))
        return {};

    const auto vertex_label_id = vertex_label_exists.value();
    const auto edge_label_id   = edge_label_exists.value();

    return get_edge_dst_vertices(src,
                                 [this, &vertex_label_id, &edge_label_id](const SEdge_t & edge) -> bool
                                 {
                                     auto dst_vertex_ptr = get_vertex_by_id(edge.dst, edge.dst_label_id).value();

                                     if (!dst_vertex_ptr->state.any())
                                         return false;

                                     return edge.edge_label_id == edge_label_id && dst_vertex_ptr->payload.metadata.label_id == vertex_label_id;
                                 });
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_recursive_edges(const SNodeID & src, const std::initializer_list<std::pair<std::string_view, std::string_view>> edge_vertex_label_pairs)
{
    // std::set vertices_to_process = {src.id};
    std::vector<SEdge_t> ret = {};

    // for (const auto & curr_label_pair : edge_vertex_label_pairs)
    // {
    //     ret.clear();
    //     for (const auto & i : vertices_to_process)
    //     {
    //         auto retrieved_edges = get_edges(i, curr_label_pair.first, curr_label_pair.second);
    //         ret.insert(ret.begin(), retrieved_edges.begin(), retrieved_edges.end());
    //     }
    //     vertices_to_process.clear();
    //
    //     // for (const auto & curr_result : ret)
    //     // vertices_to_process.emplace(curr_result.dst, curr_result.dst_label_id);
    // }
    return ret;
}

std::vector<graphquery::database::storage::ILPGModel::SProperty_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_properties_by_vertex(const SNodeID & src)
{
    SRef_t<SVertexDataBlock> src_vertex = {};
    std::vector<SProperty_t> ret        = {};

    uint16_t src_label_id;

    if (const auto src_label_exists = check_if_vertex_label_exists(src.label); !src_label_exists.has_value())
        return {};
    else
        src_label_id = src_label_exists.value();

    if (auto src_vertex_exists = get_vertex_by_id(src.id, src_label_id); unlikely(!src_vertex_exists.has_value()))
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
graphquery::database::storage::CMemoryModelMMAPLPG::get_properties_by_vertex_map(const SNodeID & src)
{
    SRef_t<SVertexDataBlock> src_vertex              = {};
    std::unordered_map<std::string, std::string> ret = {};

    uint16_t src_label_id;

    if (const auto src_label_exists = check_if_vertex_label_exists(src.label); !src_label_exists.has_value())
        return {};
    else
        src_label_id = src_label_exists.value();

    if (auto src_vertex_exists = get_vertex_by_id(src.id, src_label_id); unlikely(!src_vertex_exists.has_value()))
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

graphquery::database::storage::SRef_t<graphquery::database::storage::CMemoryModelMMAPLPG::SVertexDataBlock>
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertex_by_offset(const uint32_t offset) noexcept
{
    return m_vertices_file.read_entry(offset);
}

uint32_t
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertex_head_offset(const uint64_t id)
{
    auto index_ptr          = m_global_index_file.read_entry(id);
    const auto data_block_c = utils::atomic_load(&m_vertices_file.read_metadata()->data_block_c);

    if (id >= data_block_c)
        return END_INDEX;

    return m_vertices_file.read_entry(index_ptr->offset)->idx;
}

std::optional<graphquery::database::storage::SRef_t<graphquery::database::storage::CMemoryModelMMAPLPG::SVertexDataBlock>>
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertex_by_id(const uint64_t id, const uint16_t label_id) noexcept
{
    auto index_ptr = m_global_index_file.read_entry(id);
    auto head      = index_ptr->offset;

    while (head != END_INDEX)
    {
        auto vertex_ptr = m_vertices_file.read_entry(head);

        if (vertex_ptr->payload.metadata.id == id && vertex_ptr->payload.metadata.label_id == label_id)
            return vertex_ptr;

        head = vertex_ptr->next;
    }
    return std::nullopt;
}

graphquery::database::storage::SRef_t<graphquery::database::storage::CMemoryModelMMAPLPG::SGraphMetaData_t>
graphquery::database::storage::CMemoryModelMMAPLPG::read_graph_metadata() noexcept
{
    return m_master_file.ref<SGraphMetaData_t>(METADATA_START_ADDR);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::define_vertex_lut() noexcept
{
    const auto label_c = utils::atomic_load(&read_graph_metadata()->vertex_label_c);
    m_label_vertex.resize(label_c);
    m_label_map.reserve(label_c);

    auto label_ptr = read_vertex_label_entry(0);

    for (uint16_t i = 0; i < label_c; i++, ++label_ptr)
    {
        m_label_map[label_ptr->label_s] = label_ptr->label_id;
        m_label_vertex[label_ptr->label_id].resize(label_ptr->item_c);
    }
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::read_index_list() noexcept
{
    define_vertex_lut();
    const uint64_t block_c = utils::atomic_load(&m_vertices_file.read_metadata()->data_block_c);

    auto vertex_ptr = m_vertices_file.read_entry(0);

    for (uint64_t vertex_i = 0; vertex_i < block_c; vertex_i++, ++vertex_ptr)
    {
        if (!vertex_ptr->state.any())
            continue;

        m_label_vertex[vertex_ptr->payload.metadata.label_id][vertex_ptr->payload.metadata.id] = vertex_i;
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
    static const auto base_addr  = read_graph_metadata()->vertex_label_table_addr;
    static const auto label_size = read_graph_metadata()->label_size;
    const auto effective_addr    = base_addr + (label_size * offset);
    return m_master_file.ref<SLabel_t>(effective_addr);
}

graphquery::database::storage::CMemoryModelMMAPLPG::EActionState_t
graphquery::database::storage::CMemoryModelMMAPLPG::rm_vertex_entry(const SNodeID & src) noexcept
{
    SRef_t<SVertexDataBlock> vertex_ptr = {};

    uint16_t src_label_id;

    if (const auto src_label_exists = check_if_vertex_label_exists(src.label); !src_label_exists.has_value())
        return EActionState_t::invalid;
    else
        src_label_id = src_label_exists.value();

    if (auto vertex_opt = get_vertex_by_id(src.id, src_label_id); unlikely(!vertex_opt.has_value()))
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
graphquery::database::storage::CMemoryModelMMAPLPG::rm_edge_entry(const SNodeID & src, const SNodeID & dst) noexcept
{
    SRef_t<SVertexDataBlock> vertex_ptr = {};

    const auto src_label_exists = check_if_vertex_label_exists(src.label);
    const auto dst_label_exists = check_if_vertex_label_exists(dst.label);

    if (!(src_label_exists.has_value() && dst_label_exists.has_value()))
        return EActionState_t::invalid;

    if (auto vertex_opt = get_vertex_by_id(src.id, src_label_exists.value()); unlikely(!vertex_opt.has_value()))
        return EActionState_t::invalid;
    else
        vertex_ptr = std::move(vertex_opt.value());

    const auto head_edge_idx = utils::atomic_load(&vertex_ptr->payload.edge_idx);
    const auto dst_label_id  = dst_label_exists.value();

    m_edges_file.foreach_block(head_edge_idx,
                               [this, dst, dst_label_id](SRef_t<SEdgeDataBlock> & edge_block_ptr) -> void
                               {
                                   for (int i = 0; i < edge_block_ptr->state.size(); i++)
                                   {
                                       if (edge_block_ptr->state.test(i))
                                       {
                                           if (edge_block_ptr->payload[i].metadata.dst == dst.id && edge_block_ptr->payload[i].metadata.dst_label_id == dst_label_id)
                                               edge_block_ptr->state[i].flip();
                                       }
                                   }
                               });

    return EActionState_t::valid;
}

graphquery::database::storage::CMemoryModelMMAPLPG::EActionState_t
graphquery::database::storage::CMemoryModelMMAPLPG::rm_edge_entry(const SNodeID & src, const SNodeID & dst, const std::string_view edge_label) noexcept
{
    auto edge_label_exists = check_if_edge_label_exists(edge_label);
    auto src_label_exists  = check_if_vertex_label_exists(src.label);
    auto dst_label_exists  = check_if_vertex_label_exists(dst.label);

    if (!(edge_label_exists.has_value() && src_label_exists.has_value() && dst_label_exists.value()))
        return EActionState_t::invalid;

    SRef_t<SVertexDataBlock> vertex_ptr = {};

    if (auto vertex_opt = get_vertex_by_id(src.id, src_label_exists.value()); unlikely(!vertex_opt.has_value()))
        return EActionState_t::invalid;
    else
        vertex_ptr = std::move(vertex_opt.value());

    const auto head_edge_idx = utils::atomic_load(&vertex_ptr->payload.edge_idx);
    m_edges_file.foreach_block(head_edge_idx,
                               [this, &dst, &dst_label_exists, &edge_label_exists](SRef_t<SEdgeDataBlock> & edge_block_ptr) -> void
                               {
                                   for (int i = 0; i < edge_block_ptr->state.size(); i++)
                                   {
                                       if (edge_block_ptr->state.test(i))
                                       {
                                           if (edge_block_ptr->payload[i].metadata.dst == dst.id && edge_block_ptr->payload[i].metadata.dst_label_id == dst_label_exists.value() &&
                                               edge_block_ptr->payload[i].metadata.edge_label_id == edge_label_exists.value())
                                               edge_block_ptr->state[i].flip();
                                       }
                                   }
                               });

    return EActionState_t::valid;
}

uint64_t
graphquery::database::storage::CMemoryModelMMAPLPG::get_unassigned_vertex_id(size_t label_idx) const noexcept
{
}

uint64_t
graphquery::database::storage::CMemoryModelMMAPLPG::get_num_edges()
{
    return utils::atomic_load(&read_graph_metadata()->edges_c);
}

uint64_t
graphquery::database::storage::CMemoryModelMMAPLPG::get_num_vertices()
{
    return utils::atomic_load(&read_graph_metadata()->vertices_c);
}

std::vector<graphquery::database::storage::ILPGModel::SProperty_t>
graphquery::database::storage::CMemoryModelMMAPLPG::transform_properties(const std::vector<std::pair<std::string_view, std::string_view>> & properties) noexcept
{
    std::vector<SProperty_t> ret = {};
    ret.resize(properties.size());

    for (int i = 0; i < properties.size(); i++)
        ret[i] = {properties[i].first, properties[i].second};

    return ret;
}

extern "C"
{
    LIB_EXPORT void create_graph_model(graphquery::database::storage::ILPGModel ** graph_model)
    {
        *graph_model = new graphquery::database::storage::CMemoryModelMMAPLPG();
    }
}