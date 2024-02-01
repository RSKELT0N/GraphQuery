#include "lpg_mmap.h"

#include <string_view>
#include <optional>
#include <ranges>
#include <vector>
#include <cstdarg>
#include <set>

graphquery::database::storage::CMemoryModelMMAPLPG::CMemoryModelMMAPLPG(): m_sync_needed(false), m_syncing(0), m_transaction_ref_c(0)
{
    m_log_system = logger::CLogSystem::get_instance();
    m_label_map  = std::vector<std::vector<uint64_t>>();
    m_unq_lock   = std::unique_lock(m_sync_lock);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::close() noexcept
{
    m_transactions->close();
    m_master_file.close();
    m_index_file.close();
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
        m_syncing = 1;

        (void) m_master_file.sync();
        (void) m_index_file.sync();
        (void) m_vertices_file.get_file().sync();
        (void) m_edges_file.get_file().sync();
        (void) m_properties_file.get_file().sync();

        m_transactions->reset();
        m_syncing = 0;
        m_cv_sync.notify_all();

        m_sync_needed = false;
        m_log_system->debug("Graph has been saved");
    }
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::create_graph(std::filesystem::path path, const std::string_view graph) noexcept
{
    this->m_graph_name = graph;
    this->m_master_file.set_path(path);
    this->m_index_file.set_path(path);

    m_transactions = std::make_shared<CTransaction>(path, this);

    //~ Create initial model files
    (void) CDiskDriver::create_file(path, MASTER_FILE_NAME);
    (void) CDiskDriver::create_file(path, INDEX_FILE_NAME);
    (void) CDiskDriver::create_file(path, VERTICES_FILE_NAME);
    (void) CDiskDriver::create_file(path, EDGES_FILE_NAME);
    (void) CDiskDriver::create_file(path, PROPERTIES_FILE_NAME);

    //~ Open mapping to model files
    (void) this->m_master_file.open(MASTER_FILE_NAME);
    (void) this->m_index_file.open(INDEX_FILE_NAME);
    this->m_vertices_file.open(path, VERTICES_FILE_NAME, true);
    this->m_edges_file.open(path, EDGES_FILE_NAME, true);
    this->m_properties_file.open(path, PROPERTIES_FILE_NAME, true);

    //~ Initialise graph memory.
    m_transactions->init();

    store_graph_metadata();
    store_index_metadata();
    m_vertices_file.store_metadata();
    m_edges_file.store_metadata();
    m_properties_file.store_metadata();
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::load_graph(std::filesystem::path path, std::string_view graph) noexcept
{
    this->m_graph_name = graph;
    this->m_master_file.set_path(path);
    this->m_index_file.set_path(path);

    m_transactions = std::make_shared<CTransaction>(path, this);

    //~ Open mapping to model files
    (void) this->m_master_file.open(MASTER_FILE_NAME);
    (void) this->m_index_file.open(INDEX_FILE_NAME);
    this->m_vertices_file.open(path, VERTICES_FILE_NAME, false);
    this->m_edges_file.open(path, EDGES_FILE_NAME, false);
    this->m_properties_file.open(path, PROPERTIES_FILE_NAME, false);

    //~ Load graph memory.
    m_transactions->init();
    m_transactions->handle_transactions();
    read_index_list();
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
graphquery::database::storage::CMemoryModelMMAPLPG::store_vertex_entry(const uint64_t id, const uint16_t label_id, const std::vector<SProperty_t> & props) noexcept
{
    SRef_t<SVertexDataBlock> data_block_ptr = m_vertices_file.attain_data_block();
    const uint32_t entry_offset             = data_block_ptr->idx;

    data_block_ptr->state.set(0) = true;
    data_block_ptr->next         = END_INDEX;
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

    data_block_ptr->payload.properties_idx = next_props_ref;

    return entry_offset;
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::store_index_entry(uint64_t id, const uint16_t label_id, const uint32_t vertex_offset) noexcept
{
    auto index_ptr = read_index_entry(id);

    index_ptr->id     = id;
    index_ptr->offset = vertex_offset;

    m_label_map[label_id].emplace_back(id);
}

uint32_t
graphquery::database::storage::CMemoryModelMMAPLPG::store_edge_entry(const uint32_t next_ref,
                                                                     const uint64_t src,
                                                                     const uint64_t dst,
                                                                     const uint16_t label_id,
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

    data_block_ptr->state[payload_offset]                       = true;
    data_block_ptr->payload[payload_offset].metadata.dst        = dst;
    data_block_ptr->payload[payload_offset].metadata.src        = src;
    data_block_ptr->payload[payload_offset].metadata.label_id   = label_id;
    data_block_ptr->payload[payload_offset].metadata.property_c = props.size();

    uint32_t next_props_ref = EIndexValue_t::END_INDEX;

    for (const SProperty_t & prop : props)
        next_props_ref = store_property_entry(prop, next_props_ref);

    data_block_ptr->payload[payload_offset].properties_idx = next_props_ref;
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
graphquery::database::storage::CMemoryModelMMAPLPG::store_index_metadata() noexcept
{
    auto metadata                   = read_index_metadata();
    metadata->index_c               = 0;
    metadata->index_size            = sizeof(SIndexEntry_t);
    metadata->index_list_start_addr = sizeof(SIndexMetadata_t);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::transaction_preamble() noexcept
{
    m_cv_sync.wait(m_unq_lock, wait_on_syncing);
    m_transaction_ref_c++;
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::transaction_epilogue() noexcept
{
    m_transaction_ref_c--;
}

uint16_t
graphquery::database::storage::CMemoryModelMMAPLPG::create_edge_label(const std::string_view label_str) noexcept
{
    const uint64_t label_offset = EDGE_LABELS_START_ADDR + read_graph_metadata()->edge_label_c++ * sizeof(SLabel_t);
    const uint16_t label_id     = read_graph_metadata()->edge_label_c - 1;
    const auto label_ptr        = m_master_file.ref<SLabel_t>(label_offset);

    strncpy(&label_ptr.ref->label_s[0], label_str.data(), 20);
    label_ptr.ref->item_c   = 0;
    label_ptr.ref->label_id = label_id;
    return label_id;
}

uint16_t
graphquery::database::storage::CMemoryModelMMAPLPG::create_vertex_label(const std::string_view label_str) noexcept
{
    const uint64_t label_offset = VERTEX_LABELS_START_ADDR + read_graph_metadata()->vertex_label_c++ * sizeof(SLabel_t);
    const uint16_t label_id     = read_graph_metadata()->vertex_label_c - 1;
    const auto label_ptr        = m_master_file.ref<SLabel_t>(label_offset);

    strncpy(&label_ptr.ref->label_s[0], label_str.data(), 20);
    label_ptr.ref->item_c   = 0;
    label_ptr.ref->label_id = label_id;
    m_label_map.emplace_back();
    return label_id;
}

std::optional<uint16_t>
graphquery::database::storage::CMemoryModelMMAPLPG::check_if_edge_label_exists(const std::string_view label_str) noexcept
{
    const auto edge_labels_amt = read_graph_metadata()->edge_label_c.load();
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
    const auto vertex_labels_amt = read_graph_metadata()->vertex_label_c.load();
    auto label_ptr               = read_vertex_label_entry(0);

    for (int i = 0; i < vertex_labels_amt; i++, label_ptr.ref++)
    {
        if (strcmp(label_str.data(), label_ptr.ref->label_s) == 0)
            return label_ptr.ref->label_id;
    }
    return std::nullopt;
}

graphquery::database::storage::CMemoryModelMMAPLPG::EActionState_t
graphquery::database::storage::CMemoryModelMMAPLPG::add_vertex_entry(const uint64_t id, const std::string_view label, const std::vector<SProperty_t> & props) noexcept
{
    const std::optional<uint16_t> vertex_label_exists = check_if_vertex_label_exists(label);
    const auto vertex_label_id                        = vertex_label_exists.has_value() ? vertex_label_exists.value() : create_vertex_label(label);

    if (!get_vertex_by_id(id).has_value())
        return EActionState_t::invalid;

    ++read_graph_metadata()->vertices_c;
    ++read_vertex_label_entry(vertex_label_id)->item_c;

    const auto entry_offset = store_vertex_entry(id, vertex_label_id, props);
    store_index_entry(id, vertex_label_id, entry_offset);
    return EActionState_t::valid;
}

graphquery::database::storage::CMemoryModelMMAPLPG::EActionState_t
graphquery::database::storage::CMemoryModelMMAPLPG::add_vertex_entry(const std::string_view label, const std::vector<SProperty_t> & props) noexcept
{
    const std::optional<uint16_t> vertex_label_exists = check_if_vertex_label_exists(label);
    const auto vertex_label_id                        = vertex_label_exists.has_value() ? vertex_label_exists.value() : create_vertex_label(label);

    const auto vertex_id = read_graph_metadata()->vertices_c++;
    ++read_vertex_label_entry(vertex_label_id)->item_c;

    const auto entry_offset = store_vertex_entry(vertex_id, vertex_label_id, props);
    store_index_entry(vertex_id, vertex_label_id, entry_offset);

    return EActionState_t::valid;
}

graphquery::database::storage::CMemoryModelMMAPLPG::EActionState_t
graphquery::database::storage::CMemoryModelMMAPLPG::add_edge_entry(const uint64_t src,
                                                                   const uint64_t dst,
                                                                   const std::string_view label,
                                                                   const std::vector<SProperty_t> & props) noexcept
{
    auto src_vertex_exists = get_vertex_by_id(src);

    if (!src_vertex_exists.has_value())
        return EActionState_t::invalid;

    auto vertex_src_ptr                             = std::move(src_vertex_exists.value());
    const std::optional<uint16_t> edge_label_exists = check_if_edge_label_exists(label);
    const auto edge_label_id                        = edge_label_exists.has_value() ? edge_label_exists.value() : create_edge_label(label);

    const auto tail_edge   = vertex_src_ptr->payload.edge_idx.load();
    const auto edge_offset = store_edge_entry(tail_edge, src, dst, edge_label_id, props);

    // ~ Update vertex tail and connect edges
    vertex_src_ptr->next = edge_offset;
    ++vertex_src_ptr->payload.metadata.neighbour_c;
    ++read_graph_metadata()->edges_c;

    return EActionState_t::valid;
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::add_vertex(const uint64_t id,
                                                               const std::string_view label,
                                                               const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop)
{
    transaction_preamble();
    const auto transformed_properties = transform_properties(prop);
    if (add_vertex_entry(id, label, transformed_properties) == EActionState_t::invalid)
    {
        transaction_epilogue();
        m_log_system->warning(fmt::format("Issue adding vertex"));
        return;
    }

    m_transactions->commit_vertex(label, transformed_properties, id);
    m_sync_needed = true;
    transaction_epilogue();
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::add_edge(const uint64_t src,
                                                             const uint64_t dst,
                                                             const std::string_view label,
                                                             const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop)
{
    transaction_preamble();
    const auto transformed_properties = transform_properties(prop);
    if (add_edge_entry(src, dst, label, transformed_properties) == EActionState_t::invalid)
    {
        transaction_epilogue();
        m_log_system->warning(fmt::format("Issue adding edge({}) to vertex({})", dst, src));
        return;
    }

    m_transactions->commit_edge(src, dst, label, transformed_properties);
    m_sync_needed = true;
    transaction_epilogue();
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::add_vertex(const std::string_view label, const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop)
{
    transaction_preamble();
    const auto transformed_properties = transform_properties(prop);
    (void) add_vertex_entry(label, transformed_properties);
    m_transactions->commit_vertex(label, transformed_properties);

    m_sync_needed = true;
    transaction_epilogue();
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::rm_vertex(const uint64_t vertex_id)
{
    transaction_preamble();
    if (rm_vertex_entry(vertex_id) == EActionState_t::invalid)
    {
        transaction_epilogue();
        m_log_system->warning(fmt::format("Issue removing vertex({})", vertex_id));
        return;
    }

    m_transactions->commit_rm_vertex(vertex_id);
    m_sync_needed = true;
    transaction_epilogue();
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::rm_edge(const uint64_t src, const uint64_t dst)
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
    m_sync_needed = true;
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::rm_edge(uint64_t src, uint64_t dst, const std::string_view label)
{
    transaction_preamble();
    if (rm_edge_entry(label, src, dst) == EActionState_t::invalid)
    {
        transaction_epilogue();
        m_log_system->warning(fmt::format("Issue remvoing edge({}) to vertex({})", dst, src));
        return;
    }

    m_transactions->commit_rm_edge(src, dst, label);
    m_sync_needed = true;
    transaction_epilogue();
}

std::optional<graphquery::database::storage::ILPGModel::SVertex_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertex(const uint64_t vertex_id)
{
    std::optional<SRef_t<SVertexDataBlock>> exists = get_vertex_by_id(vertex_id);

    if (!exists.has_value())
        return std::nullopt;

    return exists.value()->payload.metadata;
}

std::vector<graphquery::database::storage::ILPGModel::SVertex_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertices(const std::function<bool(const SVertex_t &)> pred)
{
    const uint64_t datablock_c = m_vertices_file.read_metadata()->data_block_c;

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
    const uint64_t datablock_c = m_edges_file.read_metadata()->data_block_c;

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
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges(const uint64_t src, const std::function<bool(const SEdge_t &)> pred)
{
    const auto exists = get_vertex_by_id(src);

    if (unlikely(!exists.has_value()))
        return {};

    auto vertex_ptr = exists.value();

    std::vector<SEdge_t> ret = {};
    ret.reserve(vertex_ptr->payload.metadata.neighbour_c);

    uint64_t curr = vertex_ptr->payload.edge_idx;

    while (curr != END_INDEX)
    {
        auto curr_edge_ptr = m_edges_file.read_entry(curr);

        for (size_t i = 0; i < curr_edge_ptr->state.size();)
        {
            if (likely(curr_edge_ptr->state.test(i)))
            {
                if (pred(curr_edge_ptr->payload[i].metadata))
                    ret.emplace_back(curr_edge_ptr->payload[i].metadata);
                i++;
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
    const auto datablock_c = m_vertices_file.read_metadata()->data_block_c.load();
    auto curr_vertex_ptr   = m_vertices_file.read_entry(0);

    for (uint32_t i = 0; i < datablock_c; i++, ++curr_vertex_ptr)
    {
        if (unlikely(!curr_vertex_ptr->state.any()))
            continue;

        uint32_t edge_ref = curr_vertex_ptr->payload.edge_idx;

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
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges(const uint64_t src, const uint64_t dst)
{
    return get_edges(src, [&dst](const SEdge_t & edge) -> bool { return edge.dst == dst; });
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges_by_label(const std::string_view label)
{
    const std::optional<uint16_t> exists = check_if_edge_label_exists(label);

    if (!exists.has_value())
        return {};

    const auto label_id = exists.value();

    return get_edges([&label_id](const SEdge_t & edge) -> bool { return edge.label_id == label_id; });
}

std::vector<graphquery::database::storage::ILPGModel::SVertex_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertices_by_label(const std::string_view label)
{
    const std::optional<uint16_t> exists = check_if_vertex_label_exists(label);

    if (!exists.has_value())
        return {};

    const uint16_t label_id = exists.has_value();

    std::vector<SVertex_t> ret = {};
    ret.resize(m_label_map[label_id].size());

    for (const uint64_t id : m_label_map[label_id])
    {
        auto vertex_ptr = get_vertex_by_id(id).value();
        ret.emplace_back(vertex_ptr->payload.metadata);
    }

    return ret;
}

std::optional<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edge(const uint64_t src, const uint64_t dst, const std::string_view edge_label)
{
    const auto exists = check_if_edge_label_exists(edge_label);

    if (!exists.has_value())
        return {};

    const auto edge_label_id = exists.value();

    return get_edges(src, [&dst, &edge_label_id](const SEdge_t & edge) -> bool { return edge.dst == dst && edge.label_id == edge_label_id; })[0];
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges(const uint64_t src, const std::string_view edge_label, const std::string_view vertex_label)
{
    const std::optional<uint16_t> vertex_label_exists = check_if_vertex_label_exists(vertex_label);
    const std::optional<uint16_t> edge_label_exists   = check_if_edge_label_exists(edge_label);

    if (!(vertex_label_exists.has_value() && edge_label_exists.has_value()))
        return {};

    const auto vertex_label_id = vertex_label_exists.value();
    const auto edge_label_id   = edge_label_exists.value();

    return get_edges(src,
                     [this, &vertex_label_id, &edge_label_id](const SEdge_t & edge) -> bool
                     {
                         auto dst_vertex_ptr = get_vertex_by_id(edge.dst).value();

                         if (!dst_vertex_ptr->state.any())
                             return false;

                         return edge.label_id == edge_label_id && dst_vertex_ptr->payload.metadata.label_id == vertex_label_id;
                     });
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_recursive_edges(const uint64_t src,
                                                                        const std::initializer_list<std::pair<std::string_view, std::string_view>> edge_vertex_label_pairs)
{
    std::set vertices_to_process = {src};
    std::vector<SEdge_t> ret     = {};

    for (const auto & curr_label_pair : edge_vertex_label_pairs)
    {
        ret.clear();
        for (const auto & i : vertices_to_process)
        {
            auto retrieved_edges = get_edges(i, curr_label_pair.first, curr_label_pair.second);
            ret.insert(ret.begin(), retrieved_edges.begin(), retrieved_edges.end());
        }
        vertices_to_process.clear();

        for (const auto & curr_result : ret)
            vertices_to_process.emplace(curr_result.dst);
    }
    return ret;
}

std::vector<graphquery::database::storage::ILPGModel::SProperty_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertex_properties(const uint64_t id)
{
    SRef_t<SVertexDataBlock> src_vertex = {};
    std::vector<SProperty_t> ret        = {};

    if (auto src_vertex_exists = get_vertex_by_id(id); unlikely(!src_vertex_exists.has_value()))
        return ret;
    else
        src_vertex = std::move(src_vertex_exists.value());

    ret.reserve(src_vertex->payload.metadata.property_c);
    auto property_ref_cpy = src_vertex->payload.properties_idx.load();

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

void
graphquery::database::storage::CMemoryModelMMAPLPG::calc_outdegree(const std::shared_ptr<uint32_t[]> outdeg) noexcept
{
    int64_t vertex_c       = 0;
    const auto datablock_c = m_vertices_file.read_metadata()->data_block_c.load();
    auto curr_vertex_ptr   = m_vertices_file.read_entry(0);

    for (uint32_t i = 0; i < datablock_c; i++, ++curr_vertex_ptr)
    {
        if (unlikely(!curr_vertex_ptr->state.any()))
            continue;

        outdeg[vertex_c++] = curr_vertex_ptr->payload.metadata.neighbour_c;
    }
}

std::optional<graphquery::database::storage::SRef_t<graphquery::database::storage::CMemoryModelMMAPLPG::SVertexDataBlock>>
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertex_by_id(const uint64_t id) noexcept
{
    auto index_ptr = read_index_entry(id);
    return m_vertices_file.read_entry(index_ptr->offset);
}

graphquery::database::storage::SRef_t<graphquery::database::storage::CMemoryModelMMAPLPG::SGraphMetaData_t>
graphquery::database::storage::CMemoryModelMMAPLPG::read_graph_metadata() noexcept
{
    return m_master_file.ref<SGraphMetaData_t>(METADATA_START_ADDR);
}

graphquery::database::storage::SRef_t<graphquery::database::storage::CMemoryModelMMAPLPG::SIndexMetadata_t>
graphquery::database::storage::CMemoryModelMMAPLPG::read_index_metadata() noexcept
{
    return m_index_file.ref<SIndexMetadata_t>(METADATA_START_ADDR);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::define_vertex_lut() noexcept
{
    const auto label_c = read_graph_metadata()->vertex_label_c.load();
    m_label_map.resize(label_c);

    auto label_ptr = read_vertex_label_entry(0);

    for (uint16_t i = 0; i < label_c; i++, ++label_ptr)
        m_label_map[label_ptr->label_id].resize(label_ptr->item_c);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::read_index_list() noexcept
{
    define_vertex_lut();
    const uint64_t block_c = m_vertices_file.read_metadata()->data_block_c;

    auto vertex_ptr = m_vertices_file.read_entry(0);
    for (uint64_t vertex_i = 0; vertex_i < block_c; vertex_i++, ++vertex_ptr)
    {
        if (!vertex_ptr->state.any())
            continue;

        m_label_map[vertex_ptr->payload.metadata.label_id][vertex_ptr->payload.metadata.id] = vertex_i;
    }
}

graphquery::database::storage::SRef_t<graphquery::database::storage::CMemoryModelMMAPLPG::SIndexEntry_t>
graphquery::database::storage::CMemoryModelMMAPLPG::read_index_entry(const uint32_t offset) noexcept
{
    static const auto base_addr  = read_index_metadata()->index_list_start_addr.load();
    static const auto index_size = read_index_metadata()->index_size.load();
    const auto effective_addr    = base_addr + index_size * offset;
    return m_index_file.ref<SIndexEntry_t>(effective_addr);
}

graphquery::database::storage::SRef_t<graphquery::database::storage::CMemoryModelMMAPLPG::SLabel_t>
graphquery::database::storage::CMemoryModelMMAPLPG::read_vertex_label_entry(const uint32_t offset) noexcept
{
    static const auto base_addr  = read_graph_metadata()->vertex_label_table_addr.load();
    static const auto label_size = read_graph_metadata()->label_size.load();
    const auto effective_addr    = base_addr + (label_size * offset);
    return m_master_file.ref<SLabel_t>(effective_addr);
}

graphquery::database::storage::SRef_t<graphquery::database::storage::CMemoryModelMMAPLPG::SLabel_t>
graphquery::database::storage::CMemoryModelMMAPLPG::read_edge_label_entry(const uint32_t offset) noexcept
{
    static const auto base_addr  = read_graph_metadata()->vertex_label_table_addr.load();
    static const auto label_size = read_graph_metadata()->label_size.load();
    const auto effective_addr    = base_addr + (label_size * offset);
    return m_master_file.ref<SLabel_t>(effective_addr);
}

graphquery::database::storage::CMemoryModelMMAPLPG::EActionState_t
graphquery::database::storage::CMemoryModelMMAPLPG::rm_vertex_entry(const uint64_t vertex_id) noexcept
{
    SRef_t<SVertexDataBlock> vertex_ptr = {};

    if (auto vertex_opt = get_vertex_by_id(vertex_id); unlikely(!vertex_opt.has_value()))
        return EActionState_t::invalid;
    else
        vertex_ptr = std::move(vertex_opt.value());

    //~ Mark deletion for each edge block connected to the vertex
    m_edges_file.foreach_block(vertex_ptr->payload.edge_idx,
                               [this](SRef_t<SEdgeDataBlock> & edge_block_ptr) -> void { m_edges_file.append_free_data_block(edge_block_ptr->idx); });

    //~ Mark deletion for vertex
    m_vertices_file.append_free_data_block(vertex_ptr->idx);
    return EActionState_t::valid;
}

graphquery::database::storage::CMemoryModelMMAPLPG::EActionState_t
graphquery::database::storage::CMemoryModelMMAPLPG::rm_edge_entry(const uint64_t src_vertex_id, const uint64_t dst_vertex_id) noexcept
{
    SRef_t<SVertexDataBlock> vertex_ptr = {};

    if (auto vertex_opt = get_vertex_by_id(src_vertex_id); unlikely(!vertex_opt.has_value()))
        return EActionState_t::invalid;
    else
        vertex_ptr = std::move(vertex_opt.value());

    m_edges_file.foreach_block(vertex_ptr->payload.edge_idx,
                               [this, &dst_vertex_id](SRef_t<SEdgeDataBlock> & edge_block_ptr) -> void
                               {
                                   for (int i = 0; i < edge_block_ptr->state.size(); i++)
                                   {
                                       if (edge_block_ptr->state.test(i))
                                       {
                                           if (edge_block_ptr->payload[i].metadata.dst == dst_vertex_id)
                                               edge_block_ptr->state[i].flip();
                                       }
                                   }
                               });

    return EActionState_t::valid;
}

graphquery::database::storage::CMemoryModelMMAPLPG::EActionState_t
graphquery::database::storage::CMemoryModelMMAPLPG::rm_edge_entry(const std::string_view label, const uint64_t src_vertex_id, const uint64_t dst_vertex_id) noexcept
{
    uint16_t label_id                   = 0;
    SRef_t<SVertexDataBlock> vertex_ptr = {};

    if (const auto label_opt = check_if_edge_label_exists(label); unlikely(!label_opt.has_value()))
        return EActionState_t::invalid;
    else
        label_id = label_opt.value();

    if (auto vertex_opt = get_vertex_by_id(src_vertex_id); unlikely(!vertex_opt.has_value()))
        return EActionState_t::invalid;
    else
        vertex_ptr = std::move(vertex_opt.value());

    m_edges_file.foreach_block(vertex_ptr->payload.edge_idx,
                               [this, &dst_vertex_id, &label_id](SRef_t<SEdgeDataBlock> & edge_block_ptr) -> void
                               {
                                   for (int i = 0; i < edge_block_ptr->state.size(); i++)
                                   {
                                       if (edge_block_ptr->state.test(i))
                                       {
                                           if (edge_block_ptr->payload[i].metadata.dst == dst_vertex_id && edge_block_ptr->payload[i].metadata.label_id == label_id)
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
    return read_graph_metadata()->edges_c;
}

uint64_t
graphquery::database::storage::CMemoryModelMMAPLPG::get_num_vertices()
{
    return read_graph_metadata()->vertices_c;
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