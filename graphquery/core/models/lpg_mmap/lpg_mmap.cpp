#include "lpg_mmap.h"

#include "transaction.h"

#include <fcntl.h>
#include <iostream>
#include <string_view>
#include <sys/mman.h>
#include <optional>
#include <ranges>
#include <vector>
#include <dispatch/io.h>

namespace
{
    std::shared_ptr<graphquery::database::storage::CTransaction> transactions;
}

graphquery::database::storage::CMemoryModelMMAPLPG::CMemoryModelMMAPLPG():
    m_flush_needed(false), m_save_lock(1), m_master_file(O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED), m_vertices_file(O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED),
    m_edges_file(O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED), m_index_file(O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED)
{
    m_log_system = logger::CLogSystem::get_instance();
    m_vertex_lut = LabelGroup<std::unordered_map<uint64_t, int64_t>>();
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
    transactions = std::make_shared<CTransaction>(path, this);

    //~ Create initial model files
    (void) CDiskDriver::create_file(path, MASTER_FILE_NAME, DEFAULT_FILE_SIZE);
    (void) CDiskDriver::create_file(path, VERTICES_FILE_NAME, DEFAULT_FILE_SIZE);
    (void) CDiskDriver::create_file(path, EDGES_FILE_NAME, DEFAULT_FILE_SIZE);
    (void) CDiskDriver::create_file(path, INDEX_FILE_NAME, DEFAULT_FILE_SIZE);

    //~ Open mapping to model files
    (void) this->m_master_file.open(MASTER_FILE_NAME);
    (void) this->m_vertices_file.open(VERTICES_FILE_NAME);
    (void) this->m_edges_file.open(EDGES_FILE_NAME);
    (void) this->m_index_file.open(INDEX_FILE_NAME);

    //~ Initialise graph memory.
    transactions->init();
    store_graph_metadata();
    store_vertices_metadata();
    store_edges_metadata();
    store_index_metadata();
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::load_graph(std::filesystem::path path, std::string_view graph) noexcept
{
    this->m_graph_name = graph;
    this->m_master_file.set_path(path);
    this->m_vertices_file.set_path(path);
    this->m_edges_file.set_path(path);
    this->m_index_file.set_path(path);
    transactions = std::make_shared<CTransaction>(path, this);

    //~ Open mapping to model files
    (void) this->m_master_file.open(MASTER_FILE_NAME);
    (void) this->m_vertices_file.open(VERTICES_FILE_NAME);
    (void) this->m_edges_file.open(EDGES_FILE_NAME);
    (void) this->m_index_file.open(INDEX_FILE_NAME);

    //~ Load graph memory.
    transactions->init();
    read_index_list();
    transactions->handle_transactions();
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::store_graph_metadata() noexcept
{
    auto * metadata = m_master_file.ref<SGraphMetaData_t>(GRAPH_METADATA_START_ADDR);
    strncpy(&metadata->graph_name[0], m_graph_name.c_str(), CFG_GRAPH_NAME_LENGTH);
    strncpy(&metadata->graph_type[0], "lpg_mmap", CFG_GRAPH_MODEL_TYPE_LENGTH);
    metadata->vertices_c     = 0;
    metadata->edges_c        = 0;
    metadata->vertex_label_c = 0;
    metadata->edge_label_c   = 0;
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::store_index_entry(uint64_t id, const uint16_t label_id, const uint64_t vertex_offset) noexcept
{
    const auto entry_idx    = read_index_metadata()->index_c++;
    const auto entry_offset = INDEX_LIST_START_ADDR + entry_idx * sizeof(SIndexEntry_t);
    auto * index_ptr        = m_index_file.ref<SIndexEntry_t>(entry_offset);

    index_ptr->id       = id;
    index_ptr->offset   = vertex_offset;
    index_ptr->label_id = label_id;

    m_vertex_lut[label_id].emplace(id, entry_idx);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::store_index_metadata() noexcept
{
    auto * metadata                 = m_index_file.ref<SIndexMetadata_t>(INDEX_METADATA_START_ADDR);
    metadata->index_c               = 0;
    metadata->index_size            = sizeof(SIndexEntry_t);
    metadata->index_list_start_addr = sizeof(SIndexMetadata_t);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::store_vertices_metadata() noexcept
{
    auto * metadata              = m_vertices_file.ref<SBlockFileMetadata_t>(VERTICES_METADATA_START_ADDR);
    metadata->data_block_c       = 0;
    metadata->data_block_size    = sizeof(SDataBlock_t<SVertexEntry_t>);
    metadata->data_blocks_offset = sizeof(SBlockFileMetadata_t);
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::store_edges_metadata() noexcept
{
    auto * metadata              = m_edges_file.ref<SBlockFileMetadata_t>(EDGES_METADATA_START_ADDR);
    metadata->data_block_c       = 0;
    metadata->data_block_size    = sizeof(SDataBlock_t<SEdgeEntry_t>);
    metadata->data_blocks_offset = sizeof(SBlockFileMetadata_t);
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
    label_ptr->item_c   = 0;
    label_ptr->label_id = label_id;
    m_vertex_lut.emplace_back();
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
graphquery::database::storage::CMemoryModelMMAPLPG::add_vertex_entry(const std::string_view label, const std::vector<SProperty_t> & prop) noexcept
{
    const std::optional<uint16_t> vertex_label_exists = check_if_vertex_label_exists(label);
    const auto vertex_label_id                        = vertex_label_exists.has_value() ? vertex_label_exists.value() : create_vertex_label(label);

    const auto vertex_id          = read_graph_metadata()->vertices_c++;
    const auto vertex_offset      = read_vertices_metadata()->data_block_c++;
    const auto data_blocks_offset = read_vertices_metadata()->data_blocks_offset;

    auto * data_block_ptr = m_vertices_file.ref<SDataBlock_t<SVertexEntry_t>>(data_blocks_offset + vertex_offset * sizeof(SDataBlock_t<SVertexEntry_t>));
    data_block_ptr->idx   = vertex_offset;
    data_block_ptr->next  = EIndexValue_t::END_INDEX;

    data_block_ptr->payload.edge_idx              = EIndexValue_t::UNALLOCATED_INDEX;
    data_block_ptr->payload.properties_idx        = EIndexValue_t::UNALLOCATED_INDEX;
    data_block_ptr->payload.metadata.id           = vertex_id;
    data_block_ptr->payload.metadata.label_id     = vertex_label_id;
    data_block_ptr->payload.metadata.neighbour_c  = 0;
    data_block_ptr->payload.metadata.edge_label_c = 0;

    store_index_entry(vertex_id, vertex_label_id, vertex_offset);
    return EActionState_t::valid;
}

graphquery::database::storage::CMemoryModelMMAPLPG::EActionState_t
graphquery::database::storage::CMemoryModelMMAPLPG::add_edge_entry(const uint64_t src,
                                                                   const uint64_t dst,
                                                                   const std::string_view label,
                                                                   const std::vector<SProperty_t> & prop) noexcept
{
    const auto src_vertex_exists = get_vertex_by_id(src);

    if (!src_vertex_exists.has_value())
        return EActionState_t::invalid;

    SDataBlock_t<SVertexEntry_t> * vertex_src_ptr   = src_vertex_exists.value();
    const std::optional<uint16_t> edge_label_exists = check_if_edge_label_exists(label);
    const auto edge_label_id                        = edge_label_exists.has_value() ? edge_label_exists.value() : create_edge_label(label);

    const auto tail_edge          = vertex_src_ptr->payload.edge_idx;
    const auto edge_offset        = read_edges_metadata()->data_block_c++;
    const auto data_blocks_offset = read_edges_metadata()->data_blocks_offset;

    auto * data_block_ptr = m_edges_file.ref<SDataBlock_t<SEdgeEntry_t>>(data_blocks_offset + edge_offset * sizeof(SDataBlock_t<SEdgeEntry_t>));
    data_block_ptr->idx   = edge_offset;
    data_block_ptr->next  = tail_edge;

    data_block_ptr->payload.properties_idx    = EIndexValue_t::UNALLOCATED_INDEX;
    data_block_ptr->payload.metadata.dst      = dst;
    data_block_ptr->payload.metadata.label_id = edge_label_id;

    // ~ Update vertex tail and connect edges
    vertex_src_ptr->next = edge_offset;

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

void
graphquery::database::storage::CMemoryModelMMAPLPG::calc_outdegree(std::shared_ptr<uint32_t[]>) noexcept
{
}

void
graphquery::database::storage::CMemoryModelMMAPLPG::edgemap(const std::unique_ptr<analytic::IRelax> & relax) noexcept
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

std::optional<graphquery::database::storage::ILPGModel::SVertex_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertex(uint64_t vertex_id)
{
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges(uint64_t src, uint64_t dst)
{
}

std::vector<graphquery::database::storage::ILPGModel::SVertex_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges_by_label(std::string_view label_id)
{
}

std::vector<graphquery::database::storage::ILPGModel::SVertex_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertices_by_label(std::string_view label_id)
{
}

std::optional<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edge(uint64_t src, uint64_t dst, std::string_view edge_label)
{
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges(uint64_t src, std::string_view edge_label, std::string_view vertex_label)
{
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_edges(uint64_t src, std::initializer_list<std::pair<std::string_view, std::string_view>> edge_vertex_label_pairs)
{
}

std::optional<graphquery::database::storage::ILPGModel::SPropertyContainer_t>
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertex_properties(uint64_t id)
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

graphquery::database::storage::CMemoryModelMMAPLPG::EActionState_t
graphquery::database::storage::CMemoryModelMMAPLPG::add_vertex_entry(uint64_t id, std::string_view label, const std::vector<SProperty_t> & prop) noexcept
{
}

const graphquery::database::storage::CMemoryModelMMAPLPG::SLabel_t &
graphquery::database::storage::CMemoryModelMMAPLPG::get_edge_label(std::string_view) noexcept
{
}

const graphquery::database::storage::CMemoryModelMMAPLPG::SLabel_t &
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertex_label(std::string_view) noexcept
{
}

uint64_t
graphquery::database::storage::CMemoryModelMMAPLPG::get_unassigned_vertex_id(size_t label_idx) const noexcept
{
}

uint64_t
graphquery::database::storage::CMemoryModelMMAPLPG::get_unassigned_vertex_label_id() const noexcept
{
}

uint64_t
graphquery::database::storage::CMemoryModelMMAPLPG::get_unassigned_edge_label_id() const noexcept
{
}

bool
graphquery::database::storage::CMemoryModelMMAPLPG::check_if_vertex_exists(uint64_t id) noexcept
{
}

std::optional<graphquery::database::storage::CMemoryModelMMAPLPG::SDataBlock_t<graphquery::database::storage::CMemoryModelMMAPLPG::SVertexEntry_t> *>
graphquery::database::storage::CMemoryModelMMAPLPG::get_vertex_by_id(const uint64_t id) noexcept
{
    int16_t vertex_label_offset = -1;
    for (uint16_t i = 0; i < read_graph_metadata()->vertex_label_c; i++)
    {
        if (m_vertex_lut[i].contains(id))
        {
            vertex_label_offset = static_cast<int16_t>(i);
            break;
        }
    }

    if (vertex_label_offset == -1)
        return std::nullopt;

    const auto index_ptr = read_index_entry(m_vertex_lut[vertex_label_offset][id]);
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

void
graphquery::database::storage::CMemoryModelMMAPLPG::read_index_list() noexcept
{
    m_vertex_lut.resize(read_graph_metadata()->vertex_label_c);
    const auto vertices_amt = read_graph_metadata()->vertices_c;
    auto * index_ptr        = m_index_file.ref<SIndexEntry_t>(read_index_metadata()->index_list_start_addr);

    for (int i = 0; i < vertices_amt; i++, index_ptr++)
        m_vertex_lut[index_ptr->label_id].emplace(index_ptr->id, i);
}

graphquery::database::storage::CMemoryModelMMAPLPG::SIndexEntry_t *
graphquery::database::storage::CMemoryModelMMAPLPG::read_index_entry(const uint64_t offset) noexcept
{
    const auto addr_offset = read_index_metadata()->index_list_start_addr + read_index_metadata()->index_size * offset;
    return m_index_file.ref<SIndexEntry_t>(addr_offset);
}

graphquery::database::storage::CMemoryModelMMAPLPG::SDataBlock_t<graphquery::database::storage::CMemoryModelMMAPLPG::SVertexEntry_t> *
graphquery::database::storage::CMemoryModelMMAPLPG::read_vertex_entry(const uint64_t offset) noexcept
{
    const auto addr_offset = read_vertices_metadata()->data_blocks_offset + read_vertices_metadata()->data_block_size * offset;
    return m_vertices_file.ref<SDataBlock_t<SVertexEntry_t>>(addr_offset);
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
