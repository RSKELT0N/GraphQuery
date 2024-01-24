#include "lpg_mmap.h"

#include "transaction.h"

#include <string_view>
#include <optional>
#include <ranges>
#include <vector>

namespace
{
    std::shared_ptr<graphquery::database::storage::CTransaction> transactions;
}

graphquery::database::storage::CMemoryModelMMAPLPG::CMemoryModelMMAPLPG(): m_flush_needed(false), m_save_lock(1)
{
    m_log_system = logger::CLogSystem::get_instance();
    m_vertex_lut = std::unordered_map<uint16_t, std::unordered_map<uint64_t, int64_t>>();
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::close() noexcept
{
    transactions->close();
    (void) m_master_file.close();
    (void) m_vertices_file.close();
    (void) m_index_file.close();
}

std::string_view
graphquery::database::storage::CMemoryModelMMAPLPG::get_name() noexcept
{
    return read_graph_metadata()->graph_name;
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::save_graph() noexcept
{
    if (m_flush_needed)
    {
        m_save_lock.acquire();

        (void) m_master_file.sync();
        (void) m_vertices_file.sync();
        (void) m_index_file.sync();

        m_save_lock.release();
        transactions->reset();

        m_flush_needed = false;
        m_log_system->debug("Graph has been saved");
    }
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::create_graph(std::filesystem::path path, const std::string_view graph) noexcept
{
    this->m_graph_name = graph;
    this->m_master_file.set_path(path);
    this->m_vertices_file.set_path(path);
    this->m_index_file.set_path(path);
    this->m_edges_file.set_path(path);
    this->m_properties_file.set_path(path);
    transactions = std::make_shared<CTransaction>(path, this);

    //~ Create initial model files
    (void) CDiskDriver::create_file(path, MASTER_FILE_NAME, DEFAULT_FILE_SIZE);
    (void) CDiskDriver::create_file(path, VERTICES_FILE_NAME, DEFAULT_FILE_SIZE);
    (void) CDiskDriver::create_file(path, EDGES_FILE_NAME, DEFAULT_FILE_SIZE);
    (void) CDiskDriver::create_file(path, INDEX_FILE_NAME, DEFAULT_FILE_SIZE);
    (void) CDiskDriver::create_file(path, INDEX_FILE_NAME, DEFAULT_FILE_SIZE);
    (void) CDiskDriver::create_file(path, PROPERTIES_FILE_NAME, DEFAULT_FILE_SIZE);

    //~ Open mapping to model files
    (void) this->m_master_file.open(MASTER_FILE_NAME);
    (void) this->m_vertices_file.open(VERTICES_FILE_NAME);
    (void) this->m_edges_file.open(EDGES_FILE_NAME);
    (void) this->m_index_file.open(INDEX_FILE_NAME);
    (void) this->m_properties_file.open(PROPERTIES_FILE_NAME);

    //~ Initialise graph memory.
    transactions->init();

    store_graph_metadata();
    store_vertices_metadata();
    store_edges_metadata();
    store_index_metadata();
    store_properties_metadata();
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::load_graph(std::filesystem::path path, std::string_view graph) noexcept
{
    this->m_graph_name = graph;
    this->m_master_file.set_path(path);
    this->m_vertices_file.set_path(path);
    this->m_edges_file.set_path(path);
    this->m_index_file.set_path(path);
    this->m_properties_file.set_path(path);
    transactions = std::make_shared<CTransaction>(path, this);

    //~ Open mapping to model files
    (void) this->m_master_file.open(MASTER_FILE_NAME);
    (void) this->m_vertices_file.open(VERTICES_FILE_NAME);
    (void) this->m_edges_file.open(EDGES_FILE_NAME);
    (void) this->m_index_file.open(INDEX_FILE_NAME);
    (void) this->m_properties_file.open(PROPERTIES_FILE_NAME);

    //~ Load graph memory.
    transactions->init();
    read_index_list();
    transactions->handle_transactions();
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::store_graph_metadata() noexcept
{
    auto * metadata = read_graph_metadata();
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
graphquery::database::storage::CMemoryModelMMAPLPG::store_index_entry(uint64_t id, const uint16_t label_id, const uint32_t vertex_offset) noexcept
{
    const auto entry_offset = read_index_metadata()->index_c++;
    auto * index_ptr        = read_index_entry(entry_offset);

    index_ptr->id       = id;
    index_ptr->offset   = vertex_offset;
    index_ptr->label_id = label_id;

    m_vertex_lut[label_id].emplace(id, entry_offset);
    return entry_offset;
}

uint32_t
graphquery::database::storage::CMemoryModelMMAPLPG::store_vertex_entry(const uint64_t vertex_id, const uint16_t label_id, const std::vector<SProperty_t> & props) noexcept
{
    const auto entry_offset = read_vertices_metadata()->data_block_c++;
    auto * data_block_ptr   = read_vertex_entry(entry_offset);
    data_block_ptr->idx     = entry_offset;
    data_block_ptr->next    = EIndexValue_t::END_INDEX;
    data_block_ptr->version = EIndexValue_t::END_INDEX;

    data_block_ptr->payload.edge_idx              = EIndexValue_t::END_INDEX;
    data_block_ptr->payload.metadata.id           = vertex_id;
    data_block_ptr->payload.metadata.label_id     = label_id;
    data_block_ptr->payload.metadata.neighbour_c  = 0;
    data_block_ptr->payload.metadata.property_c   = props.size();
    data_block_ptr->payload.metadata.edge_label_c = 0;

    uint32_t next_props_ref = EIndexValue_t::END_INDEX;

    for (const SProperty_t & prop : props)
        next_props_ref = store_property_entry(prop, next_props_ref);

    data_block_ptr->payload.properties_idx = next_props_ref;
    return entry_offset;
}

uint32_t
graphquery::database::storage::CMemoryModelMMAPLPG::store_edge_entry(const uint32_t next_ref,
                                                                     const uint64_t src,
                                                                     const uint64_t dst,
                                                                     const uint16_t label_id,
                                                                     const std::vector<SProperty_t> & props) noexcept
{
    const auto entry_offset = read_edges_metadata()->data_block_c++;
    auto * data_block_ptr   = read_edge_entry(entry_offset);

    data_block_ptr->idx  = entry_offset;
    data_block_ptr->next = next_ref;
    data_block_ptr->version = EIndexValue_t::END_INDEX;

    data_block_ptr->payload.metadata.dst        = dst;
    data_block_ptr->payload.metadata.src        = src;
    data_block_ptr->payload.metadata.label_id   = label_id;
    data_block_ptr->payload.metadata.property_c = props.size();

    uint32_t next_props_ref = EIndexValue_t::END_INDEX;

    for (const SProperty_t & prop : props)
        next_props_ref = store_property_entry(prop, next_props_ref);

    data_block_ptr->payload.properties_idx = next_props_ref;
    return entry_offset;
}

uint32_t
graphquery::database::storage::CMemoryModelMMAPLPG::store_property_entry(const SProperty_t & prop, const uint32_t next_ref) noexcept
{
    const auto entry_offset = read_properties_metadata()->data_block_c++;
    auto * data_block_ptr   = read_property_entry(entry_offset);

    data_block_ptr->idx  = entry_offset;
    data_block_ptr->next = next_ref;
    data_block_ptr->version = EIndexValue_t::END_INDEX;

    strncpy(&data_block_ptr->payload.key[0], prop.key, CFG_LPG_PROPERTY_KEY_LENGTH);
    strncpy(&data_block_ptr->payload.value[0], prop.value, CFG_LPG_PROPERTY_VALUE_LENGTH);

    return entry_offset;
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::store_index_metadata() noexcept
{
    auto * metadata                 = read_index_metadata();
    metadata->index_c               = 0;
    metadata->index_size            = sizeof(SIndexEntry_t);
    metadata->index_list_start_addr = sizeof(SIndexMetadata_t);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::store_vertices_metadata() noexcept
{
    auto * metadata                  = read_vertices_metadata();
    metadata->data_block_c           = 0;
    metadata->data_block_size        = sizeof(SDataBlock_t<SVertexEntry_t>);
    metadata->data_blocks_start_addr = sizeof(SBlockFileMetadata_t);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::store_edges_metadata() noexcept
{
    auto * metadata                  = read_edges_metadata();
    metadata->data_block_c           = 0;
    metadata->data_block_size        = sizeof(SDataBlock_t<SEdgeEntry_t>);
    metadata->data_blocks_start_addr = sizeof(SBlockFileMetadata_t);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::store_properties_metadata() noexcept
{
    auto * metadata                  = read_properties_metadata();
    metadata->data_block_c           = 0;
    metadata->data_block_size        = sizeof(SDataBlock_t<SProperty_t>);
    metadata->data_blocks_start_addr = sizeof(SBlockFileMetadata_t);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::access_preamble() noexcept
{
    m_save_lock.acquire();
    m_save_lock.release();
}

uint16_t
graphquery::database::storage::CMemoryModelMMAPLPG::create_edge_label(std::string_view label_str) noexcept
{
    const uint64_t label_offset = EDGE_LABELS_START_ADDR + read_graph_metadata()->edge_label_c++ * sizeof(SLabel_t);
    const uint16_t label_id     = read_graph_metadata()->edge_label_c - 1;
    auto * label_ptr            = m_master_file.ref<SLabel_t>(label_offset);

    strncpy(&label_ptr->label_s[0], label_str.data(), 20);
    label_ptr->item_c   = 0;
    label_ptr->label_id = label_id;
    return label_id;
}

uint16_t
graphquery::database::storage::CMemoryModelMMAPLPG::create_vertex_label(const std::string_view label_str) noexcept
{
    const uint64_t label_offset = VERTEX_LABELS_START_ADDR + read_graph_metadata()->vertex_label_c++ * sizeof(SLabel_t);
    const uint16_t label_id     = read_graph_metadata()->vertex_label_c - 1;
    auto * label_ptr            = m_master_file.ref<SLabel_t>(label_offset);

    strncpy(&label_ptr->label_s[0], label_str.data(), 20);
    label_ptr->item_c      = 0;
    label_ptr->label_id    = label_id;
    m_vertex_lut[label_id] = std::unordered_map<uint64_t, int64_t>();
    return label_id;
}

std::optional<uint16_t>
graphquery::database::storage::CMemoryModelMMAPLPG::check_if_edge_label_exists(const std::string_view label_str) noexcept
{
    const auto edge_labels_amt = read_graph_metadata()->edge_label_c;
    auto * label_ptr           = m_master_file.ref<SLabel_t>(EDGE_LABELS_START_ADDR);

    for (int i = 0; i < edge_labels_amt; i++, label_ptr++)
    {
        if (strcmp(label_str.data(), label_ptr->label_s) == 0)
            return label_ptr->label_id;
    }
    return std::nullopt;
}

std::optional<uint16_t>
graphquery::database::storage::CMemoryModelMMAPLPG::check_if_vertex_label_exists(const std::string_view label_str) noexcept
{
    const auto vertex_labels_amt = read_graph_metadata()->vertex_label_c;
    auto * label_ptr             = m_master_file.ref<SLabel_t>(VERTEX_LABELS_START_ADDR);

    for (int i = 0; i < vertex_labels_amt; i++, label_ptr++)
    {
        if (strcmp(label_str.data(), label_ptr->label_s) == 0)
            return label_ptr->label_id;
    }
    return std::nullopt;
}

graphquery::database::storage::CMemoryModelMMAPLPG::EActionState_t
graphquery::database::storage::CMemoryModelMMAPLPG::add_vertex_entry(uint64_t id, const std::string_view label, const std::vector<SProperty_t> & props) noexcept
{
    const std::optional<uint16_t> vertex_label_exists = check_if_vertex_label_exists(label);
    const auto vertex_label_id                        = vertex_label_exists.has_value() ? vertex_label_exists.value() : create_vertex_label(label);

    if (get_vertex_by_id(id).has_value())
        return EActionState_t::invalid;

    const auto entry_offset = store_vertex_entry(id, vertex_label_id, props);
    store_index_entry(id, vertex_label_id, entry_offset);
    return EActionState_t::valid;
}

graphquery::database::storage::CMemoryModelMMAPLPG::EActionState_t
graphquery::database::storage::CMemoryModelMMAPLPG::add_vertex_entry(const std::string_view label, const std::vector<SProperty_t> & props) noexcept
{
    const std::optional<uint16_t> vertex_label_exists = check_if_vertex_label_exists(label);
    const auto vertex_label_id                        = vertex_label_exists.has_value() ? vertex_label_exists.value() : create_vertex_label(label);

    const auto vertex_id    = read_graph_metadata()->vertices_c++;
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
    const auto src_vertex_exists = get_vertex_by_id(src);

    if (!src_vertex_exists.has_value())
        return EActionState_t::invalid;

    SDataBlock_t<SVertexEntry_t> * vertex_src_ptr   = src_vertex_exists.value();
    const std::optional<uint16_t> edge_label_exists = check_if_edge_label_exists(label);
    const auto edge_label_id                        = edge_label_exists.has_value() ? edge_label_exists.value() : create_edge_label(label);

    const auto tail_edge   = vertex_src_ptr->payload.edge_idx;
    const auto edge_offset = store_edge_entry(tail_edge, src, dst, edge_label_id, props);

    // ~ Update vertex tail and connect edges
    vertex_src_ptr->next = edge_offset;
    vertex_src_ptr->payload.metadata.neighbour_c++;
    read_graph_metadata()->edges_c++;

    return EActionState_t::valid;
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::add_vertex(const uint64_t id,
                                                               const std::string_view label,
                                                               const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop)
{
    const auto transformed_properties = transform_properties(prop);
    if (add_vertex_entry(id, label, transformed_properties) == EActionState_t::invalid)
    {
        m_log_system->warning(fmt::format("Issue adding vertex"));
        return;
    }

    transactions->commit_vertex(label, transformed_properties, id);
    m_flush_needed = true;
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::add_vertex(const std::string_view label, const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop)
{
    const auto transformed_properties = transform_properties(prop);
    (void) add_vertex_entry(label, transformed_properties);
    transactions->commit_vertex(label, transformed_properties);
    m_flush_needed = true;
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::add_edge(const uint64_t src,
                                                             const uint64_t dst,
                                                             const std::string_view label,
                                                             const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop)
{
    const auto transformed_properties = transform_properties(prop);
    if (add_edge_entry(src, dst, label, transformed_properties) == EActionState_t::invalid)
    {
        m_log_system->warning(fmt::format("Issue adding edge({}) to vertex({})", dst, src));
        return;
    }
    transactions->commit_edge(src, dst, label, transformed_properties);
    m_flush_needed = true;
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::rm_vertex(const uint64_t vertex_id)
{
    if (rm_vertex_entry(vertex_id) == EActionState_t::invalid)
    {
        m_log_system->warning(fmt::format("Issue removing vertex({})", vertex_id));
        return;
    }
    transactions->commit_rm_vertex(vertex_id);
    m_flush_needed = true;
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::rm_edge(const uint64_t src, const uint64_t dst)
{
    if (rm_edge_entry(src, dst) == EActionState_t::invalid)
    {
        m_log_system->warning(fmt::format("Issue remvoing edge({}) to vertex({})", dst, src));
        return;
    }
    transactions->commit_rm_edge(src, dst);
    m_flush_needed = true;
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::rm_edge(uint64_t src, uint64_t dst, std::string_view label)
{
    if (rm_edge_entry(label, src, dst) == EActionState_t::invalid)
    {
        m_log_system->warning(fmt::format("Issue remvoing edge({}) to vertex({})", dst, src));
        return;
    }
    transactions->commit_rm_edge(src, dst, label);
    m_flush_needed = true;
}

std::optional<graphquery::database::storage::ILPGModel::SVertex_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertex(uint64_t vertex_id)
{
    const std::optional<SDataBlock_t<SVertexEntry_t> *> exists = get_vertex_by_id(vertex_id);

    if (!exists.has_value())
        return std::nullopt;

    return exists.value()->payload.metadata;
}

std::vector<graphquery::database::storage::ILPGModel::SVertex_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertices(const std::function<bool(const SVertex_t &)> pred)
{
    const uint64_t datablock_c = read_vertices_metadata()->data_block_c;

    std::vector<SVertex_t> ret = {};
    ret.reserve(datablock_c);

    for (uint64_t i = 0; i < datablock_c; i++)
    {
        const SDataBlock_t<SVertexEntry_t> * curr_vertex_ptr = read_vertex_entry(i);

        if (curr_vertex_ptr->idx == EIndexValue_t::MARKED_INDEX)
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
    const uint64_t datablock_c = read_edges_metadata()->data_block_c;

    std::vector<SEdge_t> ret = {};
    ret.reserve(datablock_c);

    for (uint64_t i = 0; i < datablock_c; i++)
    {
        const SDataBlock_t<SEdgeEntry_t> * curr_edge_ptr = read_edge_entry(i);

        if (curr_edge_ptr->idx == EIndexValue_t::MARKED_INDEX)
            continue;

        if (pred(curr_edge_ptr->payload.metadata))
            ret.emplace_back(curr_edge_ptr->payload.metadata);
    }

    ret.shrink_to_fit();
    return ret;
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges(const uint64_t src, std::function<bool(const SEdge_t &)> pred)
{
    const auto exists = get_vertex_by_id(src);

    if (!exists.has_value())
        return {};

    const SDataBlock_t<SVertexEntry_t> * vertex_ptr = exists.value();

    std::vector<SEdge_t> ret = {};
    ret.reserve(vertex_ptr->payload.metadata.neighbour_c);

    uint64_t curr = vertex_ptr->payload.edge_idx;

    while (curr != EIndexValue_t::END_INDEX)
    {
        const SDataBlock_t<SEdgeEntry_t> * curr_edge_ptr = read_edge_entry(curr);

        if (pred(curr_edge_ptr->payload.metadata))
            ret.emplace_back(curr_edge_ptr->payload.metadata);

        curr = curr_edge_ptr->next;
    }

    ret.shrink_to_fit();
    return ret;
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
    ret.resize(m_vertex_lut[label_id].size());

    std::for_each(m_vertex_lut[label_id].begin(),
                  m_vertex_lut[label_id].end(),
                  [this, &ret](const std::pair<uint64_t, uint64_t> & p)
                  {
                      const SDataBlock_t<SVertexEntry_t> * vertex_ptr = read_vertex_entry(p.second);
                      ret.emplace_back(vertex_ptr->payload.metadata);
                  });

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
                         const auto dst_vertex_ptr = get_vertex_by_id(edge.dst).value();
                         return edge.label_id == edge_label_id && dst_vertex_ptr->payload.metadata.label_id == vertex_label_id;
                     });
}

std::vector<graphquery::database::storage::ILPGModel::SProperty_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertex_properties(uint64_t id)
{
    const auto src_vertex_exists = get_vertex_by_id(id);
    std::vector<SProperty_t> ret = {};

    if (!src_vertex_exists.has_value())
        return ret;

    const auto src_vertex = src_vertex_exists.value();
    ret.reserve(src_vertex->payload.metadata.property_c);

    auto property_ref_cpy                    = src_vertex->payload.properties_idx;
    SDataBlock_t<SProperty_t> * property_ptr = nullptr;

    while (property_ref_cpy != EIndexValue_t::END_INDEX)
    {
        property_ptr = read_property_entry(src_vertex->payload.properties_idx);
        ret.emplace_back(property_ptr->payload.key, property_ptr->payload.value);
        property_ref_cpy = property_ptr->next;
    }

    return ret;
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::calc_outdegree(const std::shared_ptr<uint32_t[]> outdeg) noexcept
{
    int64_t vertex_c       = 0;
    const auto datablock_c = read_vertices_metadata()->data_block_c;

    for (uint32_t i = 0; i < datablock_c; i++)
    {
        const auto * curr_vertex_ptr = read_vertex_entry(i);

        if (curr_vertex_ptr->idx == EIndexValue_t::MARKED_INDEX) [[unlikely]]
            continue;

        outdeg[vertex_c++] = curr_vertex_ptr->payload.metadata.neighbour_c;
    }
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::edgemap(const std::unique_ptr<analytic::IRelax> & relax) noexcept
{
    const auto datablock_c = read_vertices_metadata()->data_block_c;

    for (uint32_t i = 0; i < datablock_c; i++)
    {
        const auto * curr_vertex_ptr = read_vertex_entry(i);

        if (curr_vertex_ptr->idx == EIndexValue_t::MARKED_INDEX) [[unlikely]]
            continue;

        SDataBlock_t<SEdgeEntry_t> * curr_edge_ptr = nullptr;
        uint32_t edge_ref                          = curr_vertex_ptr->payload.edge_idx;

        while (edge_ref != EIndexValue_t::END_INDEX)
        {
            curr_edge_ptr = read_edge_entry(edge_ref);

            if (curr_edge_ptr->idx != EIndexValue_t::MARKED_INDEX) [[unlikely]]
                relax->relax(curr_vertex_ptr->payload.metadata.id, curr_edge_ptr->payload.metadata.dst);

            edge_ref = curr_edge_ptr->next;
        }
    }
}

std::optional<graphquery::database::storage::CMemoryModelMMAPLPG::SDataBlock_t<graphquery::database::storage::CMemoryModelMMAPLPG::SVertexEntry_t> *>
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertex_by_id(const uint64_t id) noexcept
{
    int16_t vertex_label_id = -1;

    for (const auto & [label_id, vertex_table] : m_vertex_lut)
    {
        if (vertex_table.contains(id))
        {
            vertex_label_id = static_cast<int16_t>(label_id);
            break;
        }
    }

    if (vertex_label_id == -1)
        return std::nullopt;

    const auto index_ptr = read_index_entry(m_vertex_lut[vertex_label_id][id]);
    return read_vertex_entry(index_ptr->offset);
}

graphquery::database::storage::CMemoryModelMMAPLPG::SGraphMetaData_t *
graphquery::database::storage::CMemoryModelMMAPLPG::read_graph_metadata() noexcept
{
    return m_master_file.ref<SGraphMetaData_t>(GRAPH_METADATA_START_ADDR);
}

graphquery::database::storage::CMemoryModelMMAPLPG::SBlockFileMetadata_t *
graphquery::database::storage::CMemoryModelMMAPLPG::read_vertices_metadata() noexcept
{
    return m_vertices_file.ref<SBlockFileMetadata_t>(VERTICES_METADATA_START_ADDR);
}

graphquery::database::storage::CMemoryModelMMAPLPG::SBlockFileMetadata_t *
graphquery::database::storage::CMemoryModelMMAPLPG::read_edges_metadata() noexcept
{
    return m_edges_file.ref<SBlockFileMetadata_t>(EDGES_METADATA_START_ADDR);
}

graphquery::database::storage::CMemoryModelMMAPLPG::SIndexMetadata_t *
graphquery::database::storage::CMemoryModelMMAPLPG::read_index_metadata() noexcept
{
    return m_index_file.ref<SIndexMetadata_t>(INDEX_METADATA_START_ADDR);
}

graphquery::database::storage::CMemoryModelMMAPLPG::SBlockFileMetadata_t *
graphquery::database::storage::CMemoryModelMMAPLPG::read_properties_metadata() noexcept
{
    return m_properties_file.ref<SBlockFileMetadata_t>(PROPERTIES_METADATA_START_ADDR);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::define_vertex_lut() noexcept
{
    auto addr_offset      = read_graph_metadata()->vertex_label_table_addr;
    const auto label_size = read_graph_metadata()->label_size;
    const auto label_c    = read_graph_metadata()->vertex_label_c;

    for (uint16_t i = 0; i < label_c; i++)
    {
        const SLabel_t * label_ptr        = m_master_file.ref<SLabel_t>(addr_offset);
        m_vertex_lut[label_ptr->label_id] = std::unordered_map<uint64_t, int64_t>();
        m_vertex_lut[label_ptr->label_id].reserve(label_ptr->item_c);
        addr_offset += label_size;
    }
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::read_index_list() noexcept
{
    define_vertex_lut();
    const auto vertices_amt = read_graph_metadata()->vertices_c;
    auto * index_ptr        = read_index_entry(0);

    for (int i = 0; i < vertices_amt; i++, index_ptr++)
        m_vertex_lut[index_ptr->label_id].emplace(index_ptr->id, i);
}

graphquery::database::storage::CMemoryModelMMAPLPG::SIndexEntry_t *
graphquery::database::storage::CMemoryModelMMAPLPG::read_index_entry(const uint32_t offset) noexcept
{
    const auto addr_offset = read_index_metadata()->index_list_start_addr + read_index_metadata()->index_size * offset;
    return m_index_file.ref<SIndexEntry_t>(addr_offset);
}

graphquery::database::storage::CMemoryModelMMAPLPG::SDataBlock_t<graphquery::database::storage::CMemoryModelMMAPLPG::SVertexEntry_t> *
graphquery::database::storage::CMemoryModelMMAPLPG::read_vertex_entry(const uint32_t offset) noexcept
{
    static const auto base_addr      = read_vertices_metadata()->data_blocks_start_addr;
    static const auto datablock_size = read_vertices_metadata()->data_block_size;
    const auto effective_addr        = base_addr + (datablock_size * offset);
    return m_vertices_file.ref<SDataBlock_t<SVertexEntry_t>>(effective_addr);
}

graphquery::database::storage::CMemoryModelMMAPLPG::SDataBlock_t<graphquery::database::storage::ILPGModel::SProperty_t> *
graphquery::database::storage::CMemoryModelMMAPLPG::read_property_entry(const uint32_t offset) noexcept
{
    static const auto base_addr      = read_properties_metadata()->data_blocks_start_addr;
    static const auto datablock_size = read_properties_metadata()->data_block_size;
    const auto effective_addr        = base_addr + (datablock_size * offset);
    return m_properties_file.ref<SDataBlock_t<SProperty_t>>(effective_addr);
}

graphquery::database::storage::CMemoryModelMMAPLPG::SDataBlock_t<graphquery::database::storage::CMemoryModelMMAPLPG::SEdgeEntry_t> *
graphquery::database::storage::CMemoryModelMMAPLPG::read_edge_entry(const uint32_t offset) noexcept
{
    static const auto base_addr      = read_edges_metadata()->data_blocks_start_addr;
    static const auto datablock_size = read_edges_metadata()->data_block_size;
    const auto effective_addr        = base_addr + (datablock_size * offset);
    return m_edges_file.ref<SDataBlock_t<SEdgeEntry_t>>(effective_addr);
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges(uint64_t src, std::initializer_list<std::pair<std::string_view, std::string_view>> edge_vertex_label_pairs)
{
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::update_edge(uint64_t edge_id, const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop)
{
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::update_vertex(uint64_t vertex_id, const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop)
{
}

graphquery::database::storage::CMemoryModelMMAPLPG::EActionState_t
graphquery::database::storage::CMemoryModelMMAPLPG::rm_vertex_entry(uint64_t vertex_id) noexcept
{
}

graphquery::database::storage::CMemoryModelMMAPLPG::EActionState_t
graphquery::database::storage::CMemoryModelMMAPLPG::rm_edge_entry(uint64_t src_vertex_id, uint64_t dst_vertex_id) noexcept
{
}

graphquery::database::storage::CMemoryModelMMAPLPG::EActionState_t
graphquery::database::storage::CMemoryModelMMAPLPG::rm_edge_entry(std::string_view, uint64_t src_vertex_id, uint64_t dst_vertex_id) noexcept
{
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
