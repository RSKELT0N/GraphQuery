#include "lpg.h"

#include "transaction.h"

#include <atomic>
#include <fcntl.h>
#include <iostream>
#include <string_view>
#include <sys/mman.h>
#include <optional>
#include <ranges>
#include <vector>

namespace
{
    std::shared_ptr<graphquery::database::storage::CTransaction> transactions;
}

graphquery::database::storage::CMemoryModelLPG::CMemoryModelLPG():
    m_flush_needed(false), m_save_lock(1), m_master_file(O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED), m_connections_file(O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED),
    m_vertices_prop_file(O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED), m_edges_prop_file(O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED)
{
    m_log_system = logger::CLogSystem::get_instance();

    m_labelled_vertices     = LabelGroup<std::vector<SVertexContainer>>();
    m_all_vertex_properties = LabelGroup<SPropertyContainer>();
    m_all_edge_properties   = std::vector<SPropertyContainer>();

    m_marked_vertices  = std::set<std::vector<SVertexContainer>::iterator>();
    m_vertex_lut       = LabelGroup<std::unordered_map<uint64_t, uint64_t>>();
    m_vertex_label_lut = std::unordered_map<uint64_t, uint64_t>();
    m_edge_label_lut   = std::unordered_map<uint16_t, uint64_t>();
}

void
graphquery::database::storage::CMemoryModelLPG::close() noexcept
{
    transactions->close();
    (void) m_master_file.close();
    (void) m_connections_file.close();
    (void) m_vertices_prop_file.close();
    (void) m_edges_prop_file.close();
}

std::string_view
graphquery::database::storage::CMemoryModelLPG::get_name() const noexcept
{
    return this->m_graph_metadata.graph_name;
}

void
graphquery::database::storage::CMemoryModelLPG::save_graph() noexcept
{
    if (m_flush_needed)
    {
        m_save_lock.acquire();
        remove_marked_vertices();
        remove_unused_labels();

        store_graph_header();
        store_vertex_labels();
        store_edge_labels();
        store_labelled_vertices();

        m_save_lock.release();
        transactions->reset();
        m_flush_needed = false;
        m_log_system->debug("Graph has been saved");
    }
}

void
graphquery::database::storage::CMemoryModelLPG::create_graph(std::filesystem::path path, std::string_view graph) noexcept
{
    this->m_graph_name = graph;
    this->m_master_file.set_path(path);
    this->m_connections_file.set_path(path);
    this->m_vertices_prop_file.set_path(path);
    this->m_edges_prop_file.set_path(path);
    transactions = std::make_shared<CTransaction>(path, this);

    //~ Create initial model files
    (void) CDiskDriver::create_file(path, MASTER_FILE_NAME, DEFAULT_FILE_SIZE);
    (void) CDiskDriver::create_file(path, CONNECTIONS_FILE_NAME, DEFAULT_FILE_SIZE);
    (void) CDiskDriver::create_file(path, VERTEX_PROPERTIES_FILE_NAME, DEFAULT_FILE_SIZE);
    (void) CDiskDriver::create_file(path, EDGE_PROPERTIES_FILE_NAME, DEFAULT_FILE_SIZE);

    //~ Open mapping to model files
    (void) this->m_master_file.open(MASTER_FILE_NAME);
    (void) this->m_connections_file.open(CONNECTIONS_FILE_NAME);
    (void) this->m_vertices_prop_file.open(VERTEX_PROPERTIES_FILE_NAME);
    (void) this->m_edges_prop_file.open(EDGE_PROPERTIES_FILE_NAME);

    //~ Initialise graph memory.
    transactions->init();
    define_graph_header();
    store_graph_header();
}

void
graphquery::database::storage::CMemoryModelLPG::load_graph(std::filesystem::path path, std::string_view graph) noexcept
{
    this->m_master_file.set_path(path);
    this->m_connections_file.set_path(path);
    this->m_vertices_prop_file.set_path(path);
    this->m_edges_prop_file.set_path(path);
    transactions = std::make_shared<CTransaction>(path, this);

    //~ Open mapping to model files
    (void) this->m_master_file.open(MASTER_FILE_NAME);
    (void) this->m_connections_file.open(CONNECTIONS_FILE_NAME);
    (void) this->m_vertices_prop_file.open(VERTEX_PROPERTIES_FILE_NAME);
    (void) this->m_edges_prop_file.open(EDGE_PROPERTIES_FILE_NAME);

    //~ Load graph memory.
    transactions->init();

    read_graph_header();
    read_vertex_labels();
    read_edge_labels();

    define_vertex_label_map();
    define_edge_label_map();

    read_labelled_vertices();
    transactions->handle_transactions();
}

void
graphquery::database::storage::CMemoryModelLPG::access_preamble() noexcept
{
    m_save_lock.acquire();
    m_save_lock.release();
}

void
graphquery::database::storage::CMemoryModelLPG::remove_marked_vertices() noexcept
{
    for (const auto vertex : m_marked_vertices)
    {
        const auto vertex_label_offset = m_vertex_label_lut[vertex->metadata.label_id];
        m_labelled_vertices[vertex_label_offset].erase(vertex);
        m_vertex_lut[vertex_label_offset].erase(vertex->metadata.id);
    }
}

void
graphquery::database::storage::CMemoryModelLPG::remove_unused_labels() noexcept
{
    for (auto it = m_vertex_labels.begin(); it != m_vertex_labels.end(); ++it)
    {
        if (it->item_c == 0)
        {
            m_vertex_labels.erase(it);
            m_vertex_label_lut.erase(it->label_id);
            m_graph_metadata.vertex_label_c--;
        }
    }

    for (auto it = m_edge_labels.begin(); it != m_edge_labels.end(); ++it)
    {
        if (it->item_c == 0)
        {
            m_edge_labels.erase(it);
            m_edge_label_lut.erase(it->label_id);
            m_graph_metadata.edge_label_c--;
        }
    }
}

void
graphquery::database::storage::CMemoryModelLPG::define_graph_header() noexcept
{
    m_graph_metadata.vertices_c     = 0;
    m_graph_metadata.edges_c        = 0;
    m_graph_metadata.vertex_label_c = 0;
    m_graph_metadata.edge_label_c   = 0;
    (void) strcpy(m_graph_metadata.graph_name, m_graph_name.c_str());
    (void) strncpy(m_graph_metadata.graph_type, "lpg", CFG_GRAPH_MODEL_TYPE_LENGTH);
}

void
graphquery::database::storage::CMemoryModelLPG::define_vertex_label_map() noexcept
{
    m_vertex_label_lut.reserve(m_graph_metadata.vertex_label_c);

    for (size_t offset = 0; offset < m_vertex_labels.size(); offset++)
    {
        m_vertex_label_lut[m_vertex_labels[offset].label_id] = offset;
    }
}

void
graphquery::database::storage::CMemoryModelLPG::define_edge_label_map() noexcept
{
    m_edge_label_lut.reserve(m_graph_metadata.edge_label_c);

    for (size_t offset                                   = 0; offset < m_edge_labels.size(); offset++)
        m_edge_label_lut[m_edge_labels[offset].label_id] = offset;
}

void
graphquery::database::storage::CMemoryModelLPG::store_graph_header() noexcept
{
    m_master_file.seek(DEFAULT_FILE_START_ADDR);
    m_master_file.write(&m_graph_metadata, sizeof(SGraphMetaData), 1);
}

void
graphquery::database::storage::CMemoryModelLPG::store_vertex_labels() noexcept
{
    m_master_file.seek(MASTER_VERTEX_LABELS_START_ADDR);
    m_master_file.write(&m_vertex_labels[0], sizeof(SLabel), m_graph_metadata.vertex_label_c);
}

void
graphquery::database::storage::CMemoryModelLPG::store_edge_labels() noexcept
{
    m_master_file.seek(MASTER_VERTEX_LABELS_START_ADDR + static_cast<int64_t>(m_graph_metadata.vertex_label_c * sizeof(SLabel)));
    m_master_file.write(&m_edge_labels[0], sizeof(SLabel), m_graph_metadata.edge_label_c);
}

void
graphquery::database::storage::CMemoryModelLPG::store_labelled_vertices() noexcept
{
    m_connections_file.seek(DEFAULT_FILE_START_ADDR);
    m_vertices_prop_file.seek(DEFAULT_FILE_START_ADDR);

    for (int label_id = 0; label_id < m_graph_metadata.vertex_label_c.load(); label_id++)
    {
        for (auto & [metadata, edge_labels, edges] : m_labelled_vertices[label_id])
        {
            m_connections_file.write(&metadata, sizeof(SVertex), 1);
            m_connections_file.write(&edge_labels[0], sizeof(SVertexEdgeLabelEntry), metadata.edge_label_c);

            for (const auto & labelled_edges : edges)
                for (const auto & [metadata] : labelled_edges)
                    m_connections_file.write(&metadata, sizeof(SEdge), 1);

            auto & properties = m_all_vertex_properties[m_vertex_lut[metadata.label_id][metadata.id]];
            m_vertices_prop_file.write(&properties.ref_id, sizeof(uint64_t), 1);
            m_vertices_prop_file.write(&properties.property_c, sizeof(uint16_t), 1);
            m_vertices_prop_file.write(&properties.properties[0], sizeof(SProperty), properties.property_c);
        }
    }
}

void
graphquery::database::storage::CMemoryModelLPG::store_all_edge_properties() noexcept
{
}

void
graphquery::database::storage::CMemoryModelLPG::read_graph_header() noexcept
{
    m_master_file.seek(DEFAULT_FILE_START_ADDR);
    m_master_file.read(&m_graph_metadata, sizeof(SGraphMetaData), 1);
}

void
graphquery::database::storage::CMemoryModelLPG::read_vertex_labels() noexcept
{
    m_master_file.seek(MASTER_VERTEX_LABELS_START_ADDR);
    m_vertex_labels.resize(m_graph_metadata.vertex_label_c);

    m_master_file.read(&m_vertex_labels[0], sizeof(SLabel), m_graph_metadata.vertex_label_c);
}

void
graphquery::database::storage::CMemoryModelLPG::read_edge_labels() noexcept
{
    m_master_file.seek(MASTER_VERTEX_LABELS_START_ADDR + static_cast<int64_t>(m_graph_metadata.vertex_label_c * sizeof(SLabel)));
    m_edge_labels.resize(m_graph_metadata.edge_label_c);

    m_master_file.read(&m_edge_labels[0], sizeof(SLabel), m_graph_metadata.edge_label_c);
}

void
graphquery::database::storage::CMemoryModelLPG::read_labelled_vertices() noexcept
{
    m_connections_file.seek(DEFAULT_FILE_START_ADDR);
    m_vertices_prop_file.seek(DEFAULT_FILE_START_ADDR);

    //~ resize vector for n labels.
    m_vertex_lut.resize(m_graph_metadata.vertex_label_c);
    m_labelled_vertices.resize(m_graph_metadata.vertex_label_c);
    m_all_vertex_properties.resize(m_graph_metadata.vertices_c);

    int64_t prop_idx = {};
    int64_t vertex_c = {};

    for (const auto & [lv_str, lv_count, lv_id] : m_vertex_labels)
    {
        vertex_c = {};
        m_labelled_vertices[lv_id].resize(lv_count);
        for (auto & [metadata, edge_labels, edges] : m_labelled_vertices[lv_id])
        {
            m_connections_file.read(&metadata, sizeof(SVertex), 1);
            edge_labels.resize(metadata.edge_label_c);

            m_connections_file.read(&edge_labels[0], sizeof(SVertexEdgeLabelEntry), metadata.edge_label_c);
            edges.resize(metadata.edge_label_c);

            auto edge_label_i = 0;
            for (auto & [le_id, le_count, pos] : edge_labels)
            {
                edges[le_id].resize(le_count);
                for (auto & [metadata] : edges[le_id])
                    m_connections_file.read(&metadata, sizeof(SEdge), 1);
                pos = edge_label_i++;
            }

            m_vertex_lut[metadata.label_id][metadata.id] = vertex_c++;
            m_vertices_prop_file.read(&m_all_vertex_properties[prop_idx].ref_id, sizeof(uint64_t), 1);
            m_vertices_prop_file.read(&m_all_vertex_properties[prop_idx].property_c, sizeof(uint16_t), 1);
            m_all_vertex_properties[prop_idx].properties.resize(m_all_vertex_properties[prop_idx].property_c);
            m_vertices_prop_file.read(&m_all_vertex_properties[prop_idx].properties[0], sizeof(SProperty), m_all_vertex_properties[prop_idx].property_c);
            prop_idx++;
        }
    }
}

void
graphquery::database::storage::CMemoryModelLPG::read_all_edge_properties() noexcept
{
}

uint64_t
graphquery::database::storage::CMemoryModelLPG::get_unassigned_vertex_id(const size_t label_idx) const noexcept
{
    auto base_id  = m_graph_metadata.vertices_c.load();
    auto assigned = m_vertex_lut[label_idx].contains(base_id);

    while (assigned)
        assigned = m_vertex_lut[label_idx].contains(++base_id);

    return base_id;
}

uint64_t
graphquery::database::storage::CMemoryModelLPG::get_unassigned_vertex_label_id() const noexcept
{
    auto base_id  = m_graph_metadata.vertex_label_c.load();
    auto assigned = m_vertex_label_lut.contains(base_id);

    while (assigned)
        assigned = m_vertex_label_lut.contains(++base_id);

    return base_id;
}

uint64_t
graphquery::database::storage::CMemoryModelLPG::get_unassigned_edge_label_id() const noexcept
{
    auto base_id  = m_graph_metadata.edge_label_c.load();
    auto assigned = m_edge_label_lut.contains(base_id);

    while (assigned)
        assigned = m_edge_label_lut.contains(++base_id);

    return base_id;
}

graphquery::database::storage::CMemoryModelLPG::SLabel
graphquery::database::storage::CMemoryModelLPG::create_label(const std::string_view label, const uint16_t label_id, const uint64_t item_c) const noexcept
{
    SLabel ret = {};
    strcpy(ret.label_s, label.data());
    ret.label_id = label_id;
    ret.item_c   = item_c;

    return ret;
}

const graphquery::database::storage::ILPGModel::SLabel &
graphquery::database::storage::CMemoryModelLPG::get_vertex_label(const std::string_view label) noexcept
{
    const std::optional<uint16_t> exists = check_if_vertex_label_exists(label);
    uint16_t label_id;

    if (!exists.has_value())
        label_id = create_vertex_label(label);
    else
        label_id = exists.value();

    return m_vertex_labels[m_vertex_label_lut[label_id]];
}

const graphquery::database::storage::ILPGModel::SLabel &
graphquery::database::storage::CMemoryModelLPG::get_edge_label(const std::string_view label) noexcept
{
    const std::optional<uint16_t> exists = check_if_edge_label_exists(label);
    uint16_t label_id;

    if (!exists.has_value())
        label_id = create_edge_label(label);
    else
        label_id = exists.value();

    return m_edge_labels[m_edge_label_lut[label_id]];
}

const graphquery::database::storage::CMemoryModelLPG::SVertexEdgeLabelEntry &
graphquery::database::storage::CMemoryModelLPG::get_vertex_edge_label(const SVertexContainer & vertex, uint16_t edge_label_id) noexcept
{
    const auto exists = check_if_vertex_edge_label_exists(vertex, edge_label_id);
    uint16_t label_pos;

    if (!exists.has_value())
        label_pos = create_vertex_edge_label(const_cast<SVertexContainer &>(vertex), edge_label_id);
    else
        label_pos = exists.value();

    return vertex.edge_labels[label_pos];
}

uint16_t
graphquery::database::storage::CMemoryModelLPG::create_vertex_label(const std::string_view label_str) noexcept
{
    const auto label_id     = get_unassigned_vertex_label_id();
    auto vertex_label_entry = create_label(label_str, label_id, 0);

    m_vertex_label_lut[vertex_label_entry.label_id] = static_cast<int64_t>(m_vertex_labels.size());
    m_vertex_labels.emplace_back(vertex_label_entry);

    m_labelled_vertices.emplace_back();
    m_vertex_lut.emplace_back();
    m_graph_metadata.vertex_label_c++;

    return label_id;
}

uint16_t
graphquery::database::storage::CMemoryModelLPG::create_edge_label(const std::string_view label_str) noexcept
{
    const auto label_id   = get_unassigned_edge_label_id();
    auto edge_label_entry = create_label(label_str, label_id, 0);

    m_edge_label_lut[edge_label_entry.label_id] = static_cast<int64_t>(m_edge_labels.size());
    m_edge_labels.emplace_back(edge_label_entry);
    m_graph_metadata.edge_label_c++;

    return label_id;
}

uint16_t
graphquery::database::storage::CMemoryModelLPG::create_vertex_edge_label(SVertexContainer & vertex, const uint16_t edge_label_id) noexcept
{
    SVertexEdgeLabelEntry vertex_edge_label_entry = {};
    vertex_edge_label_entry.label_id_ref          = edge_label_id;
    vertex_edge_label_entry.item_c                = 0;
    vertex_edge_label_entry.pos                   = vertex.edge_labels.size();

    vertex.edge_labels.emplace_back(vertex_edge_label_entry);
    vertex.labelled_edges.emplace_back();
    vertex.metadata.edge_label_c++;

    return vertex_edge_label_entry.pos;
}

std::optional<uint16_t>
graphquery::database::storage::CMemoryModelLPG::check_if_vertex_label_exists(std::string_view label_str) const noexcept
{
    const auto label =
        std::find_if(m_vertex_labels.begin(), m_vertex_labels.end(), [&label_str](const SLabel & label) { return strcmp(label_str.data(), label.label_s) == 0; });

    if (label == m_vertex_labels.end())
        return std::nullopt;

    return label->label_id;
}

std::optional<uint16_t>
graphquery::database::storage::CMemoryModelLPG::check_if_edge_label_exists(std::string_view label_str) const noexcept
{
    const auto label = std::find_if(m_edge_labels.begin(), m_edge_labels.end(), [&label_str](const SLabel & label) { return strcmp(label_str.data(), label.label_s) == 0; });

    if (label == m_edge_labels.end())
        return std::nullopt;

    return label->label_id;
}

std::optional<uint16_t>
graphquery::database::storage::CMemoryModelLPG::check_if_vertex_edge_label_exists(const SVertexContainer & vertex, uint16_t edge_label_id) const noexcept
{
    const auto label = std::find_if(vertex.edge_labels.begin(), vertex.edge_labels.end(), [&edge_label_id](const auto & _) { return edge_label_id == _.label_id_ref; });

    if (label == vertex.edge_labels.end())
        return std::nullopt;

    return label->pos;
}

std::optional<std::vector<graphquery::database::storage::CMemoryModelLPG::SVertexContainer>::iterator>
graphquery::database::storage::CMemoryModelLPG::get_vertex_by_id(const uint64_t id) noexcept
{
    const auto label = std::find_if(m_vertex_lut.begin(), m_vertex_lut.end(), [&id](auto & bucket) { return bucket.contains(id); });

    if (label == m_vertex_lut.end())
        return std::nullopt;

    const uint64_t offset = (*label)[id];

    if (offset == ULONG_LONG_MAX)
        return std::nullopt;

    return m_labelled_vertices[std::distance(m_vertex_lut.begin(), label)].begin() + static_cast<int64_t>(offset);
}

graphquery::database::storage::CMemoryModelLPG::EActionState
graphquery::database::storage::CMemoryModelLPG::add_vertex_entry(const uint64_t id, const std::string_view label, const std::vector<SProperty> & prop) noexcept
{
    access_preamble();
    //~ Assign vertex label
    SVertexContainer vertex  = {};
    const SLabel & label_ref = get_vertex_label(label);

    const auto & vertex_label_offset = m_vertex_label_lut[label_ref.label_id];
    vertex.metadata.label_id         = m_vertex_labels[vertex_label_offset].label_id;

    //~ Check if vertex ID is present
    if (get_vertex_by_id(id).has_value())
        return EActionState::invalid;

    vertex.metadata.id = id;

    //~ Add vertex to DB
    ++m_graph_metadata.vertices_c;
    m_all_vertex_properties.emplace_back(id, prop.size(), prop);

    SPropertyContainer tmp = {};
    tmp.ref_id             = id;
    tmp.property_c         = prop.size();
    tmp.properties         = prop;

    m_all_vertex_properties.emplace_back(tmp);

    m_vertex_labels[vertex_label_offset].item_c++;
    m_labelled_vertices[vertex_label_offset].push_back(vertex);
    m_vertex_lut[vertex_label_offset][vertex.metadata.id] = static_cast<int64_t>(m_labelled_vertices.size() - 1);

    m_flush_needed = true;
    return EActionState::valid;
}

graphquery::database::storage::CMemoryModelLPG::EActionState
graphquery::database::storage::CMemoryModelLPG::add_vertex_entry(const std::string_view label, const std::vector<SProperty> & prop) noexcept
{
    access_preamble();
    SVertexContainer vertex = {};

    //~ Assign vertex info
    const SLabel & label_ref = get_vertex_label(label);

    const auto & vertex_label_offset = m_vertex_label_lut[label_ref.label_id];
    vertex.metadata.label_id         = m_vertex_labels[vertex_label_offset].label_id;
    vertex.metadata.id               = get_unassigned_vertex_id(vertex_label_offset);

    //~ Add vertex to DB
    ++m_graph_metadata.vertices_c;
    m_all_vertex_properties.emplace_back(vertex.metadata.id, prop.size(), prop);

    SPropertyContainer tmp = {};
    tmp.ref_id             = vertex.metadata.id;
    tmp.property_c         = prop.size();
    tmp.properties         = prop;

    m_all_vertex_properties.emplace_back(tmp);

    m_vertex_labels[vertex_label_offset].item_c++;
    m_labelled_vertices[vertex_label_offset].emplace_back(vertex);
    m_vertex_lut[vertex_label_offset][vertex.metadata.id] = static_cast<int64_t>(m_labelled_vertices.size());

    m_flush_needed = true;
    return EActionState::valid;
}

graphquery::database::storage::CMemoryModelLPG::EActionState
graphquery::database::storage::CMemoryModelLPG::add_edge_entry(const uint64_t src,
                                                               const uint64_t dst,
                                                               const std::string_view label,
                                                               const std::vector<SProperty> & prop) noexcept
{
    access_preamble();
    if (src == dst)
        return EActionState::invalid;

    SEdgeContainer edge = {};
    //~ src vertex reference
    const auto src_vertex_opt = get_vertex_by_id(src);

    if (!src_vertex_opt.has_value())
        [[unlikely]]
            return EActionState::invalid;

    const auto & src_vertex = src_vertex_opt.value();

    //~ Assign edge info
    const SLabel & label_ref         = get_edge_label(label);
    const auto & edge_label_offset   = m_edge_label_lut[label_ref.label_id];
    const auto vertex_edge_label_ref = get_vertex_edge_label(*src_vertex, label_ref.label_id);

    edge.metadata.label_id = label_ref.label_id;
    edge.metadata.dst      = dst;

    if (std::find_if(src_vertex->labelled_edges[edge_label_offset].begin(),
                     src_vertex->labelled_edges[edge_label_offset].end(),
                     [&dst](const auto & edge) { return edge.metadata.dst == dst; }) != src_vertex->labelled_edges[edge_label_offset].end()) [[unlikely]]
        return EActionState::invalid;

    //~ Update graph header
    m_graph_metadata.edges_c++;

    //~ Update vertex with new edge
    src_vertex->labelled_edges[vertex_edge_label_ref.pos].emplace_back(edge);
    src_vertex->edge_labels[vertex_edge_label_ref.pos].item_c++;
    src_vertex->metadata.neighbour_c++;

    m_flush_needed = true;
    return EActionState::valid;
}

graphquery::database::storage::CMemoryModelLPG::EActionState
graphquery::database::storage::CMemoryModelLPG::rm_vertex_entry(const uint64_t vertex_id) noexcept
{
    access_preamble();
    const auto src_vertex_opt = get_vertex_by_id(vertex_id);

    if (!src_vertex_opt.has_value())
        [[unlikely]]
            return EActionState::invalid;

    auto src_vertex = src_vertex_opt.value();

    if (!m_vertex_label_lut.contains(src_vertex->metadata.label_id))
        return EActionState::invalid;

    //~ Update vertex metadata
    const uint16_t & v_idx         = m_vertex_label_lut[src_vertex->metadata.label_id];
    m_vertex_lut[v_idx][vertex_id] = ULONG_LONG_MAX;
    m_graph_metadata.vertices_c--;
    m_vertex_labels[v_idx].item_c--;

    //~ Update edge metadata
    m_graph_metadata.edges_c -= m_graph_metadata.edges_c >= src_vertex->metadata.neighbour_c ? src_vertex->metadata.neighbour_c : 0;

    for (const auto [count, id, pos] : src_vertex->edge_labels)
        m_edge_labels[m_edge_label_lut[id]].item_c -= m_edge_labels[m_edge_label_lut[id]].item_c >= count ? count : 0;

    m_marked_vertices.emplace(src_vertex);

    m_flush_needed = true;
    return EActionState::valid;
}

graphquery::database::storage::CMemoryModelLPG::EActionState
graphquery::database::storage::CMemoryModelLPG::rm_edge_entry(const uint64_t src_vertex_id, uint64_t dst_vertex_id) noexcept
{
    access_preamble();
    const auto src_vertex_opt = get_vertex_by_id(src_vertex_id);

    if (!src_vertex_opt.has_value())
        [[unlikely]]
            return EActionState::invalid;

    const auto & src_vertex = src_vertex_opt.value();
    size_t erased           = {}, total = {};

    for (auto & [count, id, pos] : src_vertex->edge_labels)
    {
        erased += std::erase_if(src_vertex->labelled_edges[pos], [&dst_vertex_id](const auto & edge) { return edge.metadata.dst == dst_vertex_id; });

        count -= erased;
        m_edge_labels[m_edge_label_lut[id]].item_c -= erased;

        if (count == 0)
        {
            src_vertex->edge_labels.erase(src_vertex->edge_labels.begin() + pos);
            src_vertex->labelled_edges.erase(src_vertex->labelled_edges.begin() + pos);
            src_vertex->metadata.edge_label_c--;
        }

        total += erased;
        erased = {};
    }

    m_graph_metadata.edges_c -= m_graph_metadata.edges_c >= total ? total : 0;
    src_vertex->metadata.neighbour_c -= src_vertex->metadata.neighbour_c >= erased ? erased : 0;

    m_flush_needed = true;
    return EActionState::valid;
}

graphquery::database::storage::CMemoryModelLPG::EActionState
graphquery::database::storage::CMemoryModelLPG::rm_edge_entry(const std::string_view label, const uint64_t src_vertex_id, uint64_t dst_vertex_id) noexcept
{
    access_preamble();
    const auto src_vertex_opt = get_vertex_by_id(src_vertex_id);

    if (!src_vertex_opt.has_value())
        [[unlikely]]
            return EActionState::invalid;

    const auto & src_vertex     = src_vertex_opt.value();
    const auto & edge_label_ref = check_if_edge_label_exists(label);

    if (!edge_label_ref.has_value())
        return EActionState::invalid;

    if (const auto & vertex_edge_label = check_if_vertex_edge_label_exists(*src_vertex, edge_label_ref.value()); !vertex_edge_label.has_value())
        return EActionState::invalid;

    const auto & vertex_edge_label_ref = src_vertex->edge_labels[m_edge_label_lut[edge_label_ref.value()]];

    const size_t erased = std::erase_if(src_vertex->labelled_edges[m_edge_label_lut[edge_label_ref.value()]],
                                        [&dst_vertex_id](const auto & edge) { return edge.metadata.dst == dst_vertex_id; });

    if (vertex_edge_label_ref.item_c == 0)
    {
        src_vertex->edge_labels.erase(src_vertex->edge_labels.begin() + vertex_edge_label_ref.pos);
        src_vertex->labelled_edges.erase(src_vertex->labelled_edges.begin() + vertex_edge_label_ref.pos);
        src_vertex->metadata.edge_label_c--;
    }

    m_graph_metadata.edges_c -= erased;
    src_vertex->metadata.neighbour_c -= erased;
    m_edge_labels[m_edge_label_lut[edge_label_ref.value()]].item_c -= erased;

    m_flush_needed = true;
    return EActionState::valid;
}

void
graphquery::database::storage::CMemoryModelLPG::add_vertex(const uint64_t id,
                                                           const std::string_view label,
                                                           const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop)
{
    const auto transformed_properties = transform_properties(prop);
    if (add_vertex_entry(id, label, transformed_properties) == EActionState::invalid)
    {
        m_log_system->warning(fmt::format("Issue adding vertex"));
        return;
    }

    m_log_system->debug("Vertex has been added");
    transactions->commit_vertex(label, transformed_properties, id);
}

void
graphquery::database::storage::CMemoryModelLPG::add_vertex(const std::string_view label, const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop)
{
    const auto transformed_properties = transform_properties(prop);
    (void) add_vertex_entry(label, transformed_properties);
    transactions->commit_vertex(label, transformed_properties);
}

void
graphquery::database::storage::CMemoryModelLPG::add_edge(const uint64_t src,
                                                         const uint64_t dst,
                                                         const std::string_view label,
                                                         const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop)
{
    const auto transformed_properties = transform_properties(prop);
    if (add_edge_entry(src, dst, label, transformed_properties) == EActionState::invalid)
    {
        m_log_system->warning(fmt::format("Issue adding edge({}) to vertex({})", dst, src));
        return;
    }
    transactions->commit_edge(src, dst, label, transformed_properties);
}

void
graphquery::database::storage::CMemoryModelLPG::rm_vertex(const uint64_t vertex_id)
{
    if (rm_vertex_entry(vertex_id) == EActionState::invalid)
    {
        m_log_system->warning(fmt::format("Issue removing vertex({})", vertex_id));
        return;
    }
    transactions->commit_rm_vertex(vertex_id);
}

void
graphquery::database::storage::CMemoryModelLPG::rm_edge(const uint64_t src, const uint64_t dst)
{
    if (rm_edge_entry(src, dst) == EActionState::invalid)
    {
        m_log_system->warning(fmt::format("Issue remvoing edge({}) to vertex({})", dst, src));
        return;
    }
    transactions->commit_rm_edge(src, dst);
}

void
graphquery::database::storage::CMemoryModelLPG::rm_edge(uint64_t src, uint64_t dst, std::string_view label)
{
    if (rm_edge_entry(label, src, dst) == EActionState::invalid)
    {
        m_log_system->warning(fmt::format("Issue remvoing edge({}) to vertex({})", dst, src));
        return;
    }
    transactions->commit_rm_edge(src, dst, label);
}

void
graphquery::database::storage::CMemoryModelLPG::update_vertex(uint64_t vertex_id, const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop)
{
}

void
graphquery::database::storage::CMemoryModelLPG::update_edge(uint64_t edge_id, const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop)
{
}

uint64_t
graphquery::database::storage::CMemoryModelLPG::get_num_vertices() const
{
    return this->m_graph_metadata.vertices_c;
}

uint64_t
graphquery::database::storage::CMemoryModelLPG::get_num_edges() const
{
    return this->m_graph_metadata.edges_c;
}

std::optional<graphquery::database::storage::ILPGModel::SVertex>
graphquery::database::storage::CMemoryModelLPG::get_vertex(const uint64_t vertex_id)
{
    const auto src_vertex_opt = get_vertex_by_id(vertex_id);

    if (!src_vertex_opt.has_value())
    {
        m_log_system->warning(fmt::format("Issue retrieving vertex({})", vertex_id));
        return std::nullopt;
    }

    return src_vertex_opt.value()->metadata;
}

std::vector<graphquery::database::storage::ILPGModel::SEdge>
graphquery::database::storage::CMemoryModelLPG::get_edge(uint64_t src, uint64_t dst)
{
    std::vector<SEdge> ret    = {};
    const auto src_vertex_opt = get_vertex_by_id(src);

    if (!src_vertex_opt.has_value())
    {
        m_log_system->warning(fmt::format("Issue retrieving edge({}) from vertex({})", dst, src));
        return ret;
    }

    const auto & src_vertex = src_vertex_opt.value();

    for (const auto & label_edges : src_vertex->labelled_edges)
    {
        const auto edge = std::find_if(label_edges.begin(), label_edges.end(), [&dst](const auto & edge) { return edge.metadata.dst == dst; });

        if (edge != label_edges.end())
            ret.emplace_back(edge->metadata);
    }

    return ret;
}

std::optional<graphquery::database::storage::ILPGModel::SEdge>
graphquery::database::storage::CMemoryModelLPG::get_edge(uint64_t src, uint64_t dst, std::string_view label)
{
    const auto src_vertex_opt = get_vertex_by_id(src);

    if (!src_vertex_opt.has_value())
    {
        m_log_system->warning(fmt::format("Issue retrieving edge({}) from vertex({})", dst, src));
        return std::nullopt;
    }

    const auto & src_vertex     = src_vertex_opt.value();
    const auto & edge_label_ref = check_if_edge_label_exists(label);

    if (!edge_label_ref.has_value())
        return std::nullopt;

    const auto & vertex_edge_label_ref = check_if_vertex_edge_label_exists(*src_vertex, edge_label_ref.value());

    if (!vertex_edge_label_ref.has_value())
        return std::nullopt;

    const auto edge = std::find_if(src_vertex->labelled_edges[vertex_edge_label_ref.value()].begin(),
                                   src_vertex->labelled_edges[vertex_edge_label_ref.value()].end(),
                                   [&dst](const auto & edge) { return edge.metadata.dst == dst; });

    if (edge == src_vertex->labelled_edges[vertex_edge_label_ref.value()].end())
        return std::nullopt;

    return edge->metadata;
}

void
graphquery::database::storage::CMemoryModelLPG::calc_outdegree(std::shared_ptr<uint32_t[]> arr) noexcept
{
    for (uint16_t label = 0; label < m_graph_metadata.vertex_label_c; label++)
    {
        for (int64_t vertex = 0; vertex < m_vertex_labels[0].item_c; vertex++)
        {
            arr[vertex] = m_labelled_vertices[label][vertex].metadata.neighbour_c;
        }
    }
}

void
graphquery::database::storage::CMemoryModelLPG::edgemap(const std::unique_ptr<analytic::IRelax> & relax) noexcept
{
    //~ Iterating over all vertices and edges once. Multiple for-loops
    //~ due to label separation between vertices and edges.
    for (uint16_t label = 0; label < m_graph_metadata.vertex_label_c; label++)
        for (uint64_t vertex = 0; vertex < m_vertex_labels[0].item_c; vertex++)
        {
            const auto & ref = m_labelled_vertices[label][vertex];

            for (const auto & label_edges : ref.labelled_edges)
                for (const auto & edge : label_edges)
                    relax->relax(ref.metadata.id, edge.metadata.dst);
        }
}

std::vector<graphquery::database::storage::ILPGModel::SVertex>
graphquery::database::storage::CMemoryModelLPG::get_vertices_by_label(std::string_view label_id)
{
    return {};
}

std::vector<graphquery::database::storage::ILPGModel::SVertex>
graphquery::database::storage::CMemoryModelLPG::get_edges_by_label(std::string_view label_id)
{
    return {};
}

std::vector<graphquery::database::storage::ILPGModel::SProperty>
graphquery::database::storage::CMemoryModelLPG::transform_properties(const std::vector<std::pair<std::string_view, std::string_view>> & properties) noexcept
{
    std::vector<SProperty> ret = {};
    ret.resize(properties.size());

    for (int i = 0; i < properties.size(); i++)
        ret[i] = {properties[i].first, properties[i].second};

    return ret;
}

extern "C"
{
LIB_EXPORT void
create_graph_model(graphquery::database::storage::ILPGModel ** graph_model)
{
    *graph_model = new graphquery::database::storage::CMemoryModelLPG();
}
}
