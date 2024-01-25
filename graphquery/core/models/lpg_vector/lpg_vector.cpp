#include "lpg_vector.h"

#include "transaction.h"

#include <fcntl.h>
#include <iostream>
#include <string_view>
#include <optional>
#include <ranges>
#include <vector>
#include <algorithm>

namespace
{
    std::shared_ptr<graphquery::database::storage::CTransaction> transactions;
}

graphquery::database::storage::CMemoryModelVectorLPG::CMemoryModelVectorLPG(): m_flush_needed(false), m_save_lock(1)
{
    m_log_system = logger::CLogSystem::get_instance();

    m_labelled_vertices     = LabelGroup<std::vector<SVertexContainer>>();
    m_all_vertex_properties = LabelGroup<SPropertyContainer_t>();
    m_all_edge_properties   = std::vector<SPropertyContainer_t>();

    m_marked_vertices  = std::set<std::vector<SVertexContainer>::iterator>();
    m_vertex_lut       = LabelGroup<std::unordered_map<uint64_t, uint64_t>>();
    m_vertex_label_lut = std::unordered_map<uint64_t, uint64_t>();
    m_edge_label_lut   = std::unordered_map<uint16_t, uint64_t>();
}

void
graphquery::database::storage::CMemoryModelVectorLPG::close() noexcept
{
    transactions->close();
    (void) m_master_file.close();
    (void) m_connections_file.close();
    (void) m_vertices_prop_file.close();
    (void) m_edges_prop_file.close();
}

std::string_view
graphquery::database::storage::CMemoryModelVectorLPG::get_name() noexcept
{
    return this->m_graph_metadata.graph_name;
}

void
graphquery::database::storage::CMemoryModelVectorLPG::save_graph() noexcept
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
graphquery::database::storage::CMemoryModelVectorLPG::create_graph(std::filesystem::path path, const std::string_view graph) noexcept
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
graphquery::database::storage::CMemoryModelVectorLPG::load_graph(std::filesystem::path path, std::string_view graph) noexcept
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
graphquery::database::storage::CMemoryModelVectorLPG::access_preamble() noexcept
{
    m_save_lock.acquire();
    m_save_lock.release();
}

void
graphquery::database::storage::CMemoryModelVectorLPG::remove_marked_vertices() noexcept
{
    for (const auto vertex : m_marked_vertices)
    {
        const auto vertex_label_offset = m_vertex_label_lut[vertex->metadata.label_id];
        m_all_vertex_properties.erase(std::next(m_all_vertex_properties.begin(), static_cast<int64_t>(vertex->properties_ref)));
        m_vertex_lut[vertex_label_offset].erase(vertex->metadata.id);
        m_labelled_vertices[vertex_label_offset].erase(vertex);
    }
    m_marked_vertices.clear();
}

void
graphquery::database::storage::CMemoryModelVectorLPG::remove_unused_labels() noexcept
{
    for (auto it = m_vertex_labels.begin(); it != m_vertex_labels.end(); ++it)
    {
        if (it->item_c == 0)
        {
            m_vertex_labels.erase(it);
            m_vertex_label_lut.erase(it->label_id);
            --m_graph_metadata.vertex_label_c;
        }
    }

    for (auto it = m_edge_labels.begin(); it != m_edge_labels.end(); ++it)
    {
        if (it->item_c == 0)
        {
            m_edge_labels.erase(it);
            m_edge_label_lut.erase(it->label_id);
            --m_graph_metadata.edge_label_c;
        }
    }
}

void
graphquery::database::storage::CMemoryModelVectorLPG::define_graph_header() noexcept
{
    m_graph_metadata.vertices_c     = 0;
    m_graph_metadata.edges_c        = 0;
    m_graph_metadata.vertex_label_c = 0;
    m_graph_metadata.edge_label_c   = 0;
    (void) strcpy(m_graph_metadata.graph_name, m_graph_name.c_str());
    (void) strncpy(m_graph_metadata.graph_type, "lpg", CFG_GRAPH_MODEL_TYPE_LENGTH);
}

void
graphquery::database::storage::CMemoryModelVectorLPG::define_vertex_label_map() noexcept
{
    m_vertex_label_lut.reserve(m_graph_metadata.vertex_label_c);

    for (size_t offset = 0; offset < m_vertex_labels.size(); offset++)
    {
        m_vertex_label_lut[m_vertex_labels[offset].label_id] = offset;
    }
}

void
graphquery::database::storage::CMemoryModelVectorLPG::define_edge_label_map() noexcept
{
    m_edge_label_lut.reserve(m_graph_metadata.edge_label_c);

    for (size_t offset = 0; offset < m_edge_labels.size(); offset++)
        m_edge_label_lut[m_edge_labels[offset].label_id] = offset;
}

void
graphquery::database::storage::CMemoryModelVectorLPG::store_graph_header() noexcept
{
    m_master_file.seek(DEFAULT_FILE_START_ADDR);
    m_master_file.write(&m_graph_metadata, sizeof(SGraphMetaData), 1);
}

void
graphquery::database::storage::CMemoryModelVectorLPG::store_vertex_labels() noexcept
{
    m_master_file.seek(MASTER_VERTEX_LABELS_START_ADDR);
    m_master_file.write(&m_vertex_labels[0], sizeof(SLabel_t), m_graph_metadata.vertex_label_c);
}

void
graphquery::database::storage::CMemoryModelVectorLPG::store_edge_labels() noexcept
{
    m_master_file.seek(MASTER_VERTEX_LABELS_START_ADDR + static_cast<int64_t>(m_graph_metadata.vertex_label_c * sizeof(SLabel_t)));
    m_master_file.write(&m_edge_labels[0], sizeof(SLabel_t), m_graph_metadata.edge_label_c);
}

void
graphquery::database::storage::CMemoryModelVectorLPG::store_labelled_vertices() noexcept
{
    m_connections_file.seek(DEFAULT_FILE_START_ADDR);
    m_vertices_prop_file.seek(DEFAULT_FILE_START_ADDR);
    uint64_t v_idx = {};

    for (int label_id = 0; label_id < m_graph_metadata.vertex_label_c.load(); label_id++)
    {
        for (auto & [metadata, edge_labels, edges, v_properties_ref] : m_labelled_vertices[label_id])
        {
            m_connections_file.write(&metadata, sizeof(SVertex_t), 1);
            m_connections_file.write(&edge_labels[0], sizeof(SVertexEdgeLabelEntry_t), metadata.edge_label_c);

            for (const auto & labelled_edges : edges)
                for (const auto & [metadata, e_properties_ref] : labelled_edges)
                    m_connections_file.write(&metadata, sizeof(SEdge_t), 1);

            v_properties_ref  = v_idx++;
            auto & properties = m_all_vertex_properties[v_properties_ref];
            m_vertices_prop_file.write(&properties.ref_id, sizeof(uint64_t), 1);
            m_vertices_prop_file.write(&properties.property_c, sizeof(uint16_t), 1);
            m_vertices_prop_file.write(&properties.properties[0], sizeof(SProperty_t), properties.property_c);
        }
    }
}

void
graphquery::database::storage::CMemoryModelVectorLPG::store_all_edge_properties() noexcept
{
}

void
graphquery::database::storage::CMemoryModelVectorLPG::read_graph_header() noexcept
{
    m_master_file.seek(DEFAULT_FILE_START_ADDR);
    m_master_file.read(&m_graph_metadata, sizeof(SGraphMetaData), 1);
}

void
graphquery::database::storage::CMemoryModelVectorLPG::read_vertex_labels() noexcept
{
    m_master_file.seek(MASTER_VERTEX_LABELS_START_ADDR);
    m_vertex_labels.resize(m_graph_metadata.vertex_label_c);

    m_master_file.read(&m_vertex_labels[0], sizeof(SLabel_t), m_graph_metadata.vertex_label_c);
}

void
graphquery::database::storage::CMemoryModelVectorLPG::read_edge_labels() noexcept
{
    m_master_file.seek(MASTER_VERTEX_LABELS_START_ADDR + static_cast<int64_t>(m_graph_metadata.vertex_label_c * sizeof(SLabel_t)));
    m_edge_labels.resize(m_graph_metadata.edge_label_c);

    m_master_file.read(&m_edge_labels[0], sizeof(SLabel_t), m_graph_metadata.edge_label_c);
}

void
graphquery::database::storage::CMemoryModelVectorLPG::read_labelled_vertices() noexcept
{
    m_connections_file.seek(DEFAULT_FILE_START_ADDR);
    m_vertices_prop_file.seek(DEFAULT_FILE_START_ADDR);

    //~ resize vector for n labels.
    m_vertex_lut.resize(m_graph_metadata.vertex_label_c);
    m_labelled_vertices.resize(m_graph_metadata.vertex_label_c);
    m_all_vertex_properties.resize(m_graph_metadata.vertices_c);

    int64_t prop_idx   = {};
    int64_t vertex_idx = {};

    for (const auto & [lv_str, lv_count, lv_id] : m_vertex_labels)
    {
        vertex_idx = {};
        m_labelled_vertices[lv_id].resize(lv_count);
        for (auto & [metadata, edge_labels, edges, v_properties_ref] : m_labelled_vertices[lv_id])
        {
            m_connections_file.read(&metadata, sizeof(SVertex_t), 1);
            edge_labels.resize(metadata.edge_label_c);

            m_connections_file.read(&edge_labels[0], sizeof(SVertexEdgeLabelEntry_t), metadata.edge_label_c);
            edges.resize(metadata.edge_label_c);

            auto edge_label_i = 0;
            for (auto & [le_id, le_count, pos] : edge_labels)
            {
                edges[le_id].resize(le_count);
                for (auto & [metadata, e_properties_ref] : edges[le_id])
                    m_connections_file.read(&metadata, sizeof(SEdge_t), 1);
                pos = edge_label_i++;
            }

            v_properties_ref                             = prop_idx++;
            m_vertex_lut[metadata.label_id][metadata.id] = vertex_idx++;

            m_vertices_prop_file.read(&m_all_vertex_properties[v_properties_ref].ref_id, sizeof(uint64_t), 1);
            m_vertices_prop_file.read(&m_all_vertex_properties[v_properties_ref].property_c, sizeof(uint16_t), 1);
            m_all_vertex_properties[v_properties_ref].properties.resize(m_all_vertex_properties[v_properties_ref].property_c);
            m_vertices_prop_file.read(&m_all_vertex_properties[v_properties_ref].properties[0], sizeof(SProperty_t), m_all_vertex_properties[v_properties_ref].property_c);
        }
    }
}

void
graphquery::database::storage::CMemoryModelVectorLPG::read_all_edge_properties() noexcept
{
}

uint64_t
graphquery::database::storage::CMemoryModelVectorLPG::get_unassigned_vertex_id(const size_t label_idx) const noexcept
{
    auto base_id  = m_graph_metadata.vertices_c;
    auto assigned = m_vertex_lut[label_idx].contains(base_id);

    while (assigned)
        assigned = m_vertex_lut[label_idx].contains(++base_id);

    return base_id;
}

uint64_t
graphquery::database::storage::CMemoryModelVectorLPG::get_unassigned_vertex_label_id() const noexcept
{
    auto base_id  = m_graph_metadata.vertex_label_c;
    auto assigned = m_vertex_label_lut.contains(base_id);

    while (assigned)
        assigned = m_vertex_label_lut.contains(++base_id);

    return base_id;
}

uint64_t
graphquery::database::storage::CMemoryModelVectorLPG::get_unassigned_edge_label_id() const noexcept
{
    auto base_id  = m_graph_metadata.edge_label_c;
    auto assigned = m_edge_label_lut.contains(base_id);

    while (assigned)
        assigned = m_edge_label_lut.contains(++base_id);

    return base_id;
}

graphquery::database::storage::CMemoryModelVectorLPG::SLabel_t
graphquery::database::storage::CMemoryModelVectorLPG::create_label(const std::string_view label, const uint16_t label_id, const uint64_t item_c) const noexcept
{
    SLabel_t ret = {};
    strcpy(ret.label_s, label.data());
    ret.label_id = label_id;
    ret.item_c   = item_c;

    return ret;
}

const graphquery::database::storage::CMemoryModelVectorLPG::SLabel_t &
graphquery::database::storage::CMemoryModelVectorLPG::get_vertex_label(const std::string_view label) noexcept
{
    const std::optional<uint16_t> exists = check_if_vertex_label_exists(label);
    uint16_t label_id;

    if (!exists.has_value())
        label_id = create_vertex_label(label);
    else
        label_id = exists.value();

    return m_vertex_labels[m_vertex_label_lut[label_id]];
}

const graphquery::database::storage::CMemoryModelVectorLPG::SLabel_t &
graphquery::database::storage::CMemoryModelVectorLPG::get_edge_label(const std::string_view label) noexcept
{
    const std::optional<uint16_t> exists = check_if_edge_label_exists(label);
    uint16_t label_id;

    if (!exists.has_value())
        label_id = create_edge_label(label);
    else
        label_id = exists.value();

    return m_edge_labels[m_edge_label_lut[label_id]];
}

const graphquery::database::storage::CMemoryModelVectorLPG::SVertexEdgeLabelEntry_t &
graphquery::database::storage::CMemoryModelVectorLPG::get_vertex_edge_label(const SVertexContainer & vertex, uint16_t edge_label_id) noexcept
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
graphquery::database::storage::CMemoryModelVectorLPG::create_vertex_label(const std::string_view label_str) noexcept
{
    const auto label_id     = get_unassigned_vertex_label_id();
    auto vertex_label_entry = create_label(label_str, label_id, 0);

    m_vertex_label_lut[vertex_label_entry.label_id] = static_cast<int64_t>(m_vertex_labels.size());
    m_vertex_labels.emplace_back(vertex_label_entry);

    m_labelled_vertices.emplace_back();
    m_vertex_lut.emplace_back();
    ++m_graph_metadata.vertex_label_c;

    return label_id;
}

uint16_t
graphquery::database::storage::CMemoryModelVectorLPG::create_edge_label(const std::string_view label_str) noexcept
{
    const auto label_id   = get_unassigned_edge_label_id();
    auto edge_label_entry = create_label(label_str, label_id, 0);

    m_edge_label_lut[edge_label_entry.label_id] = static_cast<int64_t>(m_edge_labels.size());
    m_edge_labels.emplace_back(edge_label_entry);
    ++m_graph_metadata.edge_label_c;

    return label_id;
}

uint16_t
graphquery::database::storage::CMemoryModelVectorLPG::create_vertex_edge_label(SVertexContainer & vertex, const uint16_t edge_label_id) noexcept
{
    SVertexEdgeLabelEntry_t vertex_edge_label_entry = {};
    vertex_edge_label_entry.label_id_ref            = edge_label_id;
    vertex_edge_label_entry.item_c                  = 0;
    vertex_edge_label_entry.pos                     = vertex.edge_labels.size();

    vertex.edge_labels.emplace_back(vertex_edge_label_entry);
    vertex.labelled_edges.emplace_back();
    vertex.metadata.edge_label_c++;

    return vertex_edge_label_entry.pos;
}

std::optional<uint16_t>
graphquery::database::storage::CMemoryModelVectorLPG::check_if_vertex_label_exists(std::string_view label_str) const noexcept
{
    const auto label =
        std::find_if(m_vertex_labels.begin(), m_vertex_labels.end(), [&label_str](const SLabel_t & label) { return strcmp(label_str.data(), label.label_s) == 0; });

    if (label == m_vertex_labels.end())
        return std::nullopt;

    return label->label_id;
}

std::optional<uint16_t>
graphquery::database::storage::CMemoryModelVectorLPG::check_if_edge_label_exists(std::string_view label_str) const noexcept
{
    const auto label = std::find_if(m_edge_labels.begin(), m_edge_labels.end(), [&label_str](const SLabel_t & label) { return strcmp(label_str.data(), label.label_s) == 0; });

    if (label == m_edge_labels.end())
        return std::nullopt;

    return label->label_id;
}

std::optional<uint16_t>
graphquery::database::storage::CMemoryModelVectorLPG::check_if_vertex_edge_label_exists(const SVertexContainer & vertex, uint16_t edge_label_id) const noexcept
{
    const auto label = std::find_if(vertex.edge_labels.begin(), vertex.edge_labels.end(), [&edge_label_id](const auto & _) { return edge_label_id == _.label_id_ref; });

    if (label == vertex.edge_labels.end())
        return std::nullopt;

    return label->pos;
}

bool
graphquery::database::storage::CMemoryModelVectorLPG::check_if_vertex_exists(uint64_t id) noexcept
{
    const auto label = std::find_if(m_vertex_lut.begin(), m_vertex_lut.end(), [&id](auto & bucket) { return bucket.contains(id); });

    if (label == m_vertex_lut.end()) // ~ not found in any label type
        return false;

    return true;
}

std::optional<std::vector<graphquery::database::storage::CMemoryModelVectorLPG::SVertexContainer>::iterator>
graphquery::database::storage::CMemoryModelVectorLPG::get_vertex_by_id(const uint64_t id) noexcept
{
    const auto label = std::find_if(m_vertex_labels.begin(),
                                    m_vertex_labels.end(),
                                    [this, &id](const SLabel_t & label) { return this->m_vertex_lut[this->m_vertex_label_lut[label.label_id]].contains(id); });

    if (label == m_vertex_labels.end())
        return std::nullopt;

    const auto & label_offset = m_vertex_label_lut[label->label_id];
    const auto offset         = static_cast<int64_t>(m_vertex_lut[label_offset][id]);

    if (offset == ULONG_LONG_MAX)
        return std::nullopt;

    return m_labelled_vertices[label_offset].begin() + offset;
}

graphquery::database::storage::CMemoryModelVectorLPG::EActionState
graphquery::database::storage::CMemoryModelVectorLPG::add_vertex_entry(const uint64_t id, const std::string_view label, const std::vector<SProperty_t> & prop) noexcept
{
    access_preamble();
    //~ Check if vertex ID is present
    if (check_if_vertex_exists(id))
        return EActionState::invalid;

    //~ Assign vertex label
    SVertexContainer vertex    = {};
    const SLabel_t & label_ref = get_vertex_label(label);

    const auto & vertex_label_offset = m_vertex_label_lut[label_ref.label_id];
    vertex.metadata.label_id         = m_vertex_labels[vertex_label_offset].label_id;

    vertex.metadata.id = id;

    //~ Add vertex to DB
    ++m_graph_metadata.vertices_c;
    vertex.properties_ref = m_all_vertex_properties.size();
    m_all_vertex_properties.emplace_back(id, prop);

    SPropertyContainer_t tmp = {};
    tmp.ref_id               = id;
    tmp.property_c           = prop.size();
    tmp.properties           = prop;

    m_all_vertex_properties.emplace_back(tmp);

    m_vertex_labels[vertex_label_offset].item_c++;
    m_labelled_vertices[vertex_label_offset].push_back(vertex);
    m_vertex_lut[vertex_label_offset][vertex.metadata.id] = static_cast<int64_t>(m_labelled_vertices.size() - 1);

    m_flush_needed = true;
    return EActionState::valid;
}

graphquery::database::storage::CMemoryModelVectorLPG::EActionState
graphquery::database::storage::CMemoryModelVectorLPG::add_vertex_entry(const std::string_view label, const std::vector<SProperty_t> & prop) noexcept
{
    access_preamble();
    SVertexContainer vertex = {};

    //~ Assign vertex info
    const SLabel_t & label_ref = get_vertex_label(label);

    const auto & vertex_label_offset = m_vertex_label_lut[label_ref.label_id];
    vertex.metadata.label_id         = m_vertex_labels[vertex_label_offset].label_id;
    vertex.metadata.id               = get_unassigned_vertex_id(vertex_label_offset);

    //~ Add vertex to DB
    ++m_graph_metadata.vertices_c;
    vertex.properties_ref = m_all_vertex_properties.size();
    m_all_vertex_properties.emplace_back(vertex.metadata.id, prop);

    SPropertyContainer_t tmp = {};
    tmp.ref_id               = vertex.metadata.id;
    tmp.property_c           = prop.size();
    tmp.properties           = prop;

    m_all_vertex_properties.emplace_back(tmp);

    m_vertex_labels[vertex_label_offset].item_c++;
    m_labelled_vertices[vertex_label_offset].emplace_back(vertex);
    m_vertex_lut[vertex_label_offset][vertex.metadata.id] = static_cast<int64_t>(m_labelled_vertices.size());

    m_flush_needed = true;
    return EActionState::valid;
}

graphquery::database::storage::CMemoryModelVectorLPG::EActionState
graphquery::database::storage::CMemoryModelVectorLPG::add_edge_entry(const uint64_t src,
                                                                     const uint64_t dst,
                                                                     const std::string_view label,
                                                                     const std::vector<SProperty_t> & prop) noexcept
{
    access_preamble();
    if (src == dst)
        return EActionState::invalid;

    SEdgeContainer edge = {};
    //~ src vertex reference
    const auto src_vertex_opt = get_vertex_by_id(src);

    if (!src_vertex_opt.has_value()) [[unlikely]]
        return EActionState::invalid;

    auto & src_vertex = src_vertex_opt.value();

    //~ Assign edge info
    const SLabel_t & label_ref       = get_edge_label(label);
    const auto & edge_label_offset   = m_edge_label_lut[label_ref.label_id];
    const auto vertex_edge_label_ref = get_vertex_edge_label(*src_vertex, label_ref.label_id);

    edge.metadata.label_id = label_ref.label_id;
    edge.metadata.dst      = dst;
    edge.metadata.src      = src_vertex->metadata.id;

    if (std::find_if(src_vertex->labelled_edges[edge_label_offset].begin(),
                     src_vertex->labelled_edges[edge_label_offset].end(),
                     [&dst](const auto & edge) { return edge.metadata.dst == dst; }) != src_vertex->labelled_edges[edge_label_offset].end()) [[unlikely]]
        return EActionState::invalid;

    //~ Update graph header
    ++m_graph_metadata.edges_c;

    //~ Update vertex with new edge
    src_vertex->labelled_edges[vertex_edge_label_ref.pos].emplace_back(edge);
    src_vertex->edge_labels[vertex_edge_label_ref.pos].item_c++;
    src_vertex->metadata.neighbour_c++;

    m_flush_needed = true;
    return EActionState::valid;
}

graphquery::database::storage::CMemoryModelVectorLPG::EActionState
graphquery::database::storage::CMemoryModelVectorLPG::rm_vertex_entry(const uint64_t vertex_id) noexcept
{
    access_preamble();
    const auto src_vertex_opt = get_vertex_by_id(vertex_id);

    if (!src_vertex_opt.has_value()) [[unlikely]]
        return EActionState::invalid;

    auto src_vertex = src_vertex_opt.value();

    if (!m_vertex_label_lut.contains(src_vertex->metadata.label_id))
        return EActionState::invalid;

    //~ Update vertex metadata
    const uint16_t & v_idx         = m_vertex_label_lut[src_vertex->metadata.label_id];
    m_vertex_lut[v_idx][vertex_id] = ULONG_LONG_MAX;
    --m_graph_metadata.vertices_c;
    m_vertex_labels[v_idx].item_c--;

    //~ Update edge metadata
    m_graph_metadata.edges_c = std::min(m_graph_metadata.edges_c - src_vertex->metadata.neighbour_c, static_cast<uint64_t>(0));

    for (const auto [count, id, pos] : src_vertex->edge_labels)
        m_edge_labels[m_edge_label_lut[id]].item_c -= m_edge_labels[m_edge_label_lut[id]].item_c >= count ? count : 0;

    m_marked_vertices.insert(src_vertex);

    m_flush_needed = true;
    return EActionState::valid;
}

graphquery::database::storage::CMemoryModelVectorLPG::EActionState
graphquery::database::storage::CMemoryModelVectorLPG::rm_edge_entry(const uint64_t src_vertex_id, uint64_t dst_vertex_id) noexcept
{
    access_preamble();
    const auto src_vertex_opt = get_vertex_by_id(src_vertex_id);

    if (!src_vertex_opt.has_value()) [[unlikely]]
        return EActionState::invalid;

    const auto & src_vertex = src_vertex_opt.value();
    size_t erased = {}, total = {};

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

    m_graph_metadata.edges_c         = std::min(m_graph_metadata.edges_c - total, static_cast<uint64_t>(0));
    src_vertex->metadata.neighbour_c = std::min(src_vertex->metadata.neighbour_c - erased, 0UL);

    m_flush_needed = true;
    return EActionState::valid;
}

graphquery::database::storage::CMemoryModelVectorLPG::EActionState
graphquery::database::storage::CMemoryModelVectorLPG::rm_edge_entry(const std::string_view label, const uint64_t src_vertex_id, uint64_t dst_vertex_id) noexcept
{
    access_preamble();
    const auto src_vertex_opt = get_vertex_by_id(src_vertex_id);

    if (!src_vertex_opt.has_value()) [[unlikely]]
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
graphquery::database::storage::CMemoryModelVectorLPG::add_vertex(const uint64_t id,
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
graphquery::database::storage::CMemoryModelVectorLPG::add_vertex(const std::string_view label,
                                                                 const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop)
{
    const auto transformed_properties = transform_properties(prop);
    (void) add_vertex_entry(label, transformed_properties);
    transactions->commit_vertex(label, transformed_properties);
}

void
graphquery::database::storage::CMemoryModelVectorLPG::add_edge(const uint64_t src,
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
graphquery::database::storage::CMemoryModelVectorLPG::rm_vertex(const uint64_t vertex_id)
{
    if (rm_vertex_entry(vertex_id) == EActionState::invalid)
    {
        m_log_system->warning(fmt::format("Issue removing vertex({})", vertex_id));
        return;
    }
    transactions->commit_rm_vertex(vertex_id);
}

void
graphquery::database::storage::CMemoryModelVectorLPG::rm_edge(const uint64_t src, const uint64_t dst)
{
    if (rm_edge_entry(src, dst) == EActionState::invalid)
    {
        m_log_system->warning(fmt::format("Issue remvoing edge({}) to vertex({})", dst, src));
        return;
    }
    transactions->commit_rm_edge(src, dst);
}

void
graphquery::database::storage::CMemoryModelVectorLPG::rm_edge(uint64_t src, uint64_t dst, std::string_view label)
{
    if (rm_edge_entry(label, src, dst) == EActionState::invalid)
    {
        m_log_system->warning(fmt::format("Issue remvoing edge({}) to vertex({})", dst, src));
        return;
    }
    transactions->commit_rm_edge(src, dst, label);
}

void
graphquery::database::storage::CMemoryModelVectorLPG::update_vertex(uint64_t vertex_id, const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop)
{
}

void
graphquery::database::storage::CMemoryModelVectorLPG::update_edge(uint64_t edge_id, const std::initializer_list<std::pair<std::string_view, std::string_view>> & prop)
{
}

uint64_t
graphquery::database::storage::CMemoryModelVectorLPG::get_num_vertices()
{
    return this->m_graph_metadata.vertices_c;
}

uint64_t
graphquery::database::storage::CMemoryModelVectorLPG::get_num_edges()
{
    return this->m_graph_metadata.edges_c;
}

std::optional<graphquery::database::storage::CMemoryModelVectorLPG::SVertex_t>
graphquery::database::storage::CMemoryModelVectorLPG::get_vertex(const uint64_t vertex_id)
{
    const auto src_vertex_opt = get_vertex_by_id(vertex_id);

    if (!src_vertex_opt.has_value())
    {
        m_log_system->warning(fmt::format("Issue retrieving vertex({})", vertex_id));
        return std::nullopt;
    }

    return src_vertex_opt.value()->metadata;
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelVectorLPG::get_edges(uint64_t src, uint64_t dst)
{
    std::vector<SEdge_t> ret                           = {};
    std::vector<SVertexContainer>::iterator src_vertex = {};

    if (const auto src_vertex_opt = get_vertex_by_id(src); !src_vertex_opt.has_value())
    {
        m_log_system->warning(fmt::format("Issue retrieving edge({}) from vertex({})", dst, src));
        return {};
    }
    else
        src_vertex = src_vertex_opt.value();

    std::for_each(src_vertex->labelled_edges.begin(),
                  src_vertex->labelled_edges.end(),
                  [&ret, &dst](const auto & label_edges) -> void
                  {
                      const auto _edge = std::find_if(label_edges.begin(), label_edges.end(), [&dst](const auto & edge) { return edge.metadata.dst == dst; });

                      if (_edge != label_edges.end())
                          ret.emplace_back(_edge->metadata);
                  });

    return ret;
}

std::optional<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelVectorLPG::get_edge(uint64_t src, uint64_t dst, std::string_view label)
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
                                   [&dst](const auto & labelled_edge) { return labelled_edge.metadata.dst == dst; });

    if (edge == src_vertex->labelled_edges[vertex_edge_label_ref.value()].end())
        return std::nullopt;

    return edge->metadata;
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelVectorLPG::get_edges(const uint64_t src, const std::string_view edge_label, const std::string_view vertex_label)
{
    std::vector<SEdge_t> ret  = {};
    const auto src_vertex_opt = get_vertex_by_id(src);

    if (!src_vertex_opt.has_value())
        return ret;

    const auto & src_vertex     = src_vertex_opt.value();
    const auto & edge_label_ref = check_if_edge_label_exists(edge_label);

    if (!edge_label_ref.has_value())
        return ret;

    const auto & vertex_edge_label_ref = check_if_vertex_edge_label_exists(*src_vertex, edge_label_ref.value());

    if (!vertex_edge_label_ref.has_value())
        return ret;

    auto vrtx_label_id_search = check_if_vertex_label_exists(vertex_label);

    std::for_each(src_vertex->labelled_edges[vertex_edge_label_ref.value()].begin(),
                  src_vertex->labelled_edges[vertex_edge_label_ref.value()].end(),
                  [this, &vrtx_label_id_search, &ret](const SEdgeContainer & labelled_edge)
                  {
                      const auto dst_vertex = get_vertex_by_id(labelled_edge.metadata.dst).value();

                      if (dst_vertex->metadata.label_id == vrtx_label_id_search)
                          ret.push_back(labelled_edge.metadata);
                  });

    return ret;
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelVectorLPG::get_edges(const std::vector<SEdge_t> & edges, std::string_view edge_label, std::string_view vertex_label)
{
    static std::vector<SEdge_t> ret = {};

    for (const SEdge_t & edge : edges)
    {
        const std::vector<SVertexContainer>::iterator vertex = get_vertex_by_id(edge.dst).value();
        auto neighbours                                      = get_edges(edge.dst, edge_label, vertex_label);
        ret.insert(ret.end(), neighbours.begin(), neighbours.end());
    }
    return ret;
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelVectorLPG::get_edges(uint64_t src, std::initializer_list<std::pair<std::string_view, std::string_view>> edge_vertex_label_pairs)
{
    static const std::vector<std::pair<std::string_view, std::string_view>> vrtx_lbl_cpy(edge_vertex_label_pairs);
    std::vector<SEdge_t> base = get_edges(src, vrtx_lbl_cpy[0].first, vrtx_lbl_cpy[0].second);

    for (int i = 1; i < vrtx_lbl_cpy.size(); i++)
        base = get_edges(base, vrtx_lbl_cpy[i].first, vrtx_lbl_cpy[i].second);

    return base;
}

std::vector<graphquery::database::storage::ILPGModel::SVertex_t>
graphquery::database::storage::CMemoryModelVectorLPG::get_vertices(std::function<bool(const SVertex_t &)> pred)
{
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelVectorLPG::get_edges(std::function<bool(const SEdge_t &)>)
{
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelVectorLPG::get_edges(uint64_t src, std::function<bool(const SEdge_t &)>)
{
}

void
graphquery::database::storage::CMemoryModelVectorLPG::calc_outdegree(const std::shared_ptr<uint32_t[]> arr) noexcept
{
    for (uint16_t label = 0; label < m_graph_metadata.vertex_label_c; label++)
    {
        for (int64_t vertex = 0; vertex < m_vertex_labels[label].item_c; vertex++)
        {
            arr[vertex] = m_labelled_vertices[label][vertex].metadata.neighbour_c;
        }
    }
}

void
graphquery::database::storage::CMemoryModelVectorLPG::edgemap(const std::unique_ptr<analytic::IRelax> & relax) noexcept
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

std::vector<graphquery::database::storage::ILPGModel::SVertex_t>
graphquery::database::storage::CMemoryModelVectorLPG::get_vertices_by_label(const std::string_view label)
{
    // const std::optional<uint16_t> exists = check_if_vertex_label_exists(label);
    //
    // if (!exists.has_value())
    //     return {};
    //
    // const uint16_t label_id = exists.value();
    //
    // return {m_labelled_vertices[m_vertex_label_lut[label_id]].begin(), m_labelled_vertices[m_vertex_label_lut[label_id]].end()};
    return {};
}

std::vector<graphquery::database::storage::ILPGModel::SProperty_t>
graphquery::database::storage::CMemoryModelVectorLPG::get_vertex_properties(uint64_t id)
{
    const auto vertex = get_vertex(id);

    if (!vertex.has_value())
    {
        m_log_system->warning(fmt::format("Canont retreive properties of vertex({}) that does not exist", id));
        return {};
    }

    const auto vertex_label_off = m_vertex_label_lut[vertex.value().label_id];

    return m_all_vertex_properties[m_vertex_lut[vertex_label_off][id]].properties;
}

std::vector<graphquery::database::storage::ILPGModel::SEdge_t>
graphquery::database::storage::CMemoryModelVectorLPG::get_edges_by_label(std::string_view label_id)
{
    return {};
}

std::vector<graphquery::database::storage::ILPGModel::SProperty_t>
graphquery::database::storage::CMemoryModelVectorLPG::transform_properties(const std::vector<std::pair<std::string_view, std::string_view>> & properties) noexcept
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
        *graph_model = new graphquery::database::storage::CMemoryModelVectorLPG();
    }
}
