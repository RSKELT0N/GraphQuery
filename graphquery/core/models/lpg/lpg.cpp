#include "lpg.h"

#include "transaction.h"

#include <fcntl.h>
#include <iostream>
#include <string_view>
#include <sys/mman.h>
#include <optional>
#include <vector>

namespace
{
    std::shared_ptr<graphquery::database::storage::CTransaction> transactions;
}

graphquery::database::storage::CMemoryModelLPG::CMemoryModelLPG():
    m_flush_needed(false), m_master_file(O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED), m_connections_file(O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED)
{
    m_log_system = logger::CLogSystem::get_instance();

    m_labelled_vertices    = LabelGroup<std::vector<SVertexContainer>>();
    m_vertex_properties    = LabelGroup<std::vector<SProperty>>();
    m_edge_properties      = LabelGroup<std::vector<SProperty>>();
    m_marked_vertices      = std::vector<std::vector<SVertexContainer>::iterator>();
    m_marked_vertex_labels = std::vector<std::vector<SLabel>::iterator>();
    m_marked_edge_labels   = std::vector<std::vector<SLabel>::iterator>();

    m_vertex_map       = LabelGroup<std::unordered_map<uint64_t, size_t>>();
    m_vertex_label_map = std::unordered_map<uint64_t, size_t>();
    m_edge_label_map   = std::unordered_map<uint16_t, size_t>();
}

void
graphquery::database::storage::CMemoryModelLPG::close() noexcept
{
    transactions->close();
    (void) m_master_file.close();
    (void) m_connections_file.close();
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
        std::lock_guard lock(m_update);
        remove_marked_vertices();
        remove_marked_labels();

        store_graph_header();
        store_vertex_labels();
        store_edge_labels();
        store_labelled_vertices();

        transactions->reset();
        m_flush_needed = false;
        m_log_system->debug("Graph has been saved");
        m_log_system->warning("Graph has been saved");
    }
}

void
graphquery::database::storage::CMemoryModelLPG::create_graph(std::filesystem::path path, std::string_view graph) noexcept
{
    this->m_master_file.set_path(path);
    this->m_connections_file.set_path(path);
    transactions = std::make_shared<CTransaction>(path, this);

    (void) CDiskDriver::create_file(path, MASTER_FILE_NAME, MASTER_FILE_SIZE);
    (void) CDiskDriver::create_file(path, CONNECTIONS_FILE_NAME, CONNECTIONS_FILE_SIZE);
    (void) this->m_master_file.open(MASTER_FILE_NAME);
    (void) this->m_connections_file.open(CONNECTIONS_FILE_NAME);
    this->m_graph_name = graph;

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
    transactions = std::make_shared<CTransaction>(path, this);

    (void) this->m_master_file.open(MASTER_FILE_NAME);
    (void) this->m_connections_file.open(CONNECTIONS_FILE_NAME);

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
graphquery::database::storage::CMemoryModelLPG::remove_marked_vertices() noexcept
{
    for (const auto vertex : m_marked_vertices)
    {
        const auto & vertex_label_offset = m_vertex_label_map[vertex->vertex_metadata.label_id];
        m_labelled_vertices[vertex_label_offset].erase(vertex);
        m_vertex_map[vertex_label_offset].erase(vertex->vertex_metadata.id);
    }
}

void
graphquery::database::storage::CMemoryModelLPG::remove_marked_labels() noexcept
{
    for (const auto vertex_label : m_marked_vertex_labels)
    {
        m_vertex_labels.erase(vertex_label);
        m_vertex_label_map.erase(vertex_label->label_id);
    }

    for (const auto edge_label : m_marked_edge_labels)
    {
        m_edge_labels.erase(edge_label);
        m_vertex_label_map.erase(edge_label->label_id);
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
    m_vertex_label_map.reserve(m_graph_metadata.vertex_label_c);

    for (size_t offset = 0; offset < m_vertex_labels.size(); offset++)
        m_vertex_label_map[m_vertex_labels[offset].label_id] = offset;
}

void
graphquery::database::storage::CMemoryModelLPG::define_edge_label_map() noexcept
{
    m_edge_label_map.reserve(m_graph_metadata.edge_label_c);

    for (size_t offset = 0; offset < m_edge_labels.size(); offset++)
        m_edge_label_map[m_edge_labels[offset].label_id] = offset;
}

void
graphquery::database::storage::CMemoryModelLPG::store_graph_header() noexcept
{
    m_master_file.seek(MASTER_START_ADDR);
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
    m_connections_file.seek(CONNECTIONS_START_ADDR);

    for (int label_id = 0; label_id < m_graph_metadata.vertex_label_c; label_id++)
    {
        for (auto & [metadata, edge_labels, edges] : m_labelled_vertices[label_id])
        {
            m_connections_file.write(&metadata, sizeof(SVertex), 1);
            m_connections_file.write(&edge_labels[0], sizeof(SVertexEdgeLabelEntry), metadata.edge_label_c);

            for (const auto & [id, count, pos] : edge_labels)
            {
                m_connections_file.write(&edges[id][0], sizeof(SEdge), count);
            }
        }
    }
}

void
graphquery::database::storage::CMemoryModelLPG::read_graph_header() noexcept
{
    m_master_file.seek(MASTER_START_ADDR);
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
    m_connections_file.seek(CONNECTIONS_START_ADDR);

    //~ resize vector for n labels.
    m_vertex_map.resize(m_graph_metadata.vertex_label_c);
    m_labelled_vertices.resize(m_graph_metadata.vertex_label_c);

    for (const auto & [lv_str, lv_id, lv_count] : m_vertex_labels)
    {
        int64_t vertex_c = 0;
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
                m_connections_file.read(&edges[le_id][0], sizeof(SEdge), le_count);
                pos = edge_label_i++;
            }
            m_vertex_map[metadata.label_id][metadata.id] = vertex_c++;
        }
    }
}

uint64_t
graphquery::database::storage::CMemoryModelLPG::get_unassigned_vertex_id(const size_t label_idx) const noexcept
{
    auto base_id  = m_graph_metadata.vertices_c;
    auto assigned = m_vertex_map[label_idx].contains(base_id);

    while (assigned)
        assigned = m_vertex_map[label_idx].contains(++base_id);

    return base_id;
}

uint64_t
graphquery::database::storage::CMemoryModelLPG::get_unassigned_vertex_label_id() const noexcept
{
    auto base_id  = m_graph_metadata.vertex_label_c;
    auto assigned = m_vertex_label_map.contains(base_id);

    while (assigned)
        assigned = m_vertex_label_map.contains(++base_id);

    return base_id;
}

uint64_t
graphquery::database::storage::CMemoryModelLPG::get_unassigned_edge_label_id() const noexcept
{
    auto base_id  = m_graph_metadata.edge_label_c;
    auto assigned = m_edge_label_map.contains(base_id);

    while (assigned)
        assigned = m_edge_label_map.contains(++base_id);

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

    return m_vertex_labels[m_vertex_label_map[label_id]];
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

    return m_edge_labels[m_edge_label_map[label_id]];
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

    m_vertex_label_map[vertex_label_entry.label_id] = static_cast<int64_t>(m_vertex_labels.size());
    m_vertex_labels.emplace_back(vertex_label_entry);

    m_labelled_vertices.emplace_back();
    m_vertex_properties.emplace_back();
    m_vertex_map.emplace_back();
    m_graph_metadata.vertex_label_c++;

    return label_id;
}

uint16_t
graphquery::database::storage::CMemoryModelLPG::create_edge_label(const std::string_view label_str) noexcept
{
    const auto label_id   = get_unassigned_edge_label_id();
    auto edge_label_entry = create_label(label_str, label_id, 0);

    m_edge_label_map[edge_label_entry.label_id] = static_cast<int64_t>(m_edge_labels.size());
    m_edge_labels.emplace_back(edge_label_entry);
    m_edge_properties.emplace_back();
    m_graph_metadata.edge_label_c++;

    return label_id;
}

uint16_t
graphquery::database::storage::CMemoryModelLPG::create_vertex_edge_label(SVertexContainer & vertex, uint16_t edge_label_id) noexcept
{
    SVertexEdgeLabelEntry vertex_edge_label_entry = {};
    vertex_edge_label_entry.label_id_ref          = edge_label_id;
    vertex_edge_label_entry.item_c                = 0;
    vertex_edge_label_entry.pos                   = vertex.edge_labels.size();

    vertex.edge_labels.emplace_back(vertex_edge_label_entry);
    vertex.labelled_edges.emplace_back();
    vertex.vertex_metadata.edge_label_c++;

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
    const auto label = std::find_if(m_vertex_map.begin(), m_vertex_map.end(), [&id](auto & bucket) { return bucket.contains(id); });

    if (label == m_vertex_map.end())
        return std::nullopt;

    const uint64_t offset = (*label)[id];

    if (offset == -1UL)
        return std::nullopt;

    return m_labelled_vertices[std::distance(m_vertex_map.begin(), label)].begin() + static_cast<int64_t>(offset);
}

graphquery::database::storage::CMemoryModelLPG::EActionState
graphquery::database::storage::CMemoryModelLPG::add_vertex_entry(const uint64_t id,
                                                                 const std::string_view label,
                                                                 const std::vector<std::pair<std::string, std::string>> & prop) noexcept
{
    std::lock_guard lock(m_update);

    //~ Assign vertex label
    SVertexContainer vertex  = {};
    const SLabel & label_ref = get_vertex_label(label);

    const auto & vertex_label_offset = m_vertex_label_map[label_ref.label_id];
    vertex.vertex_metadata.label_id  = m_vertex_labels[vertex_label_offset].label_id;

    //~ Check if vertex ID is present
    if (get_vertex_by_id(id).has_value())
        return EActionState::invalid;

    vertex.vertex_metadata.id = id;

    //~ Add vertex to DB
    m_graph_metadata.vertices_c++;

    m_vertex_labels[vertex_label_offset].item_c++;
    m_labelled_vertices[vertex_label_offset].emplace_back(vertex);
    m_vertex_map[vertex_label_offset][vertex.vertex_metadata.id] = static_cast<int64_t>(m_labelled_vertices.size() - 1);

    m_flush_needed = true;
    return EActionState::valid;
}

graphquery::database::storage::CMemoryModelLPG::EActionState
graphquery::database::storage::CMemoryModelLPG::add_vertex_entry(const std::string_view label, const std::vector<std::pair<std::string, std::string>> & prop) noexcept
{
    std::lock_guard lock(m_update);
    SVertexContainer vertex = {};

    //~ Assign vertex label
    const SLabel & label_ref = get_vertex_label(label);

    const auto & vertex_label_offset = m_vertex_label_map[label_ref.label_id];
    vertex.vertex_metadata.label_id  = m_vertex_labels[vertex_label_offset].label_id;
    vertex.vertex_metadata.id        = get_unassigned_vertex_id(vertex_label_offset);

    //~ Add vertex to DB
    m_graph_metadata.vertices_c++;

    m_vertex_labels[vertex_label_offset].item_c++;
    m_labelled_vertices[vertex_label_offset].emplace_back(vertex);
    m_vertex_map[vertex_label_offset][vertex.vertex_metadata.id] = static_cast<int64_t>(m_labelled_vertices.size());

    m_flush_needed = true;
    return EActionState::valid;
}

graphquery::database::storage::CMemoryModelLPG::EActionState
graphquery::database::storage::CMemoryModelLPG::add_edge_entry(const uint64_t src,
                                                               const uint64_t dst,
                                                               const std::string_view label,
                                                               const std::vector<std::pair<std::string, std::string>> & prop) noexcept
{
    std::lock_guard lock(m_update);
    SEdge edge = {};
    //~ src vertex reference
    const auto src_vertex_opt = get_vertex_by_id(src);

    if (!src_vertex_opt.has_value())
        return EActionState::invalid;

    const auto & src_vertex = src_vertex_opt.value();

    //~ Assign edge info
    const SLabel & label_ref       = get_edge_label(label);
    const auto & edge_label_offset = m_edge_label_map[label_ref.label_id];

    edge.label_id = m_edge_labels[edge_label_offset].label_id;
    edge.dst      = dst;

    if (std::find_if(src_vertex->labelled_edges[edge_label_offset].begin(),
                     src_vertex->labelled_edges[edge_label_offset].end(),
                     [&dst](const SEdge & _) { return _.dst == dst; }) != src_vertex->labelled_edges[edge_label_offset].end())
        return EActionState::invalid;

    //~ Update graph header
    m_graph_metadata.edges_c++;
    m_edge_labels[edge_label_offset].item_c++;

    //~ Update vertex with new edge
    const auto vertex_edge_label_ref = get_vertex_edge_label(*src_vertex, label_ref.label_id);
    src_vertex->labelled_edges[vertex_edge_label_ref.pos].push_back(edge);
    src_vertex->edge_labels[vertex_edge_label_ref.pos].item_c++;
    src_vertex->vertex_metadata.neighbour_c++;

    m_flush_needed = true;
    return EActionState::valid;
}

graphquery::database::storage::CMemoryModelLPG::EActionState
graphquery::database::storage::CMemoryModelLPG::rm_vertex_entry(const uint64_t vertex_id) noexcept
{
    std::lock_guard lock(m_update);
    const auto src_vertex_opt = get_vertex_by_id(vertex_id);

    if (!src_vertex_opt.has_value())
        return EActionState::invalid;

    auto src_vertex = src_vertex_opt.value();

    if (!m_vertex_label_map.contains(src_vertex->vertex_metadata.label_id))
        return EActionState::invalid;

    //~ Update vertex metadata
    const uint16_t & v_idx         = m_vertex_label_map[src_vertex->vertex_metadata.label_id];
    m_vertex_map[v_idx][vertex_id] = -1;
    m_graph_metadata.vertices_c--;
    m_vertex_labels[v_idx].item_c--;

    if (m_vertex_labels[v_idx].item_c == 0)
    {
        m_marked_vertex_labels.emplace_back(m_vertex_labels.begin() + v_idx);
        m_graph_metadata.vertex_label_c--;
    }

    //~ Update edge metadata
    for (const auto edge_label : src_vertex->edge_labels)
    {
        auto & root_edge_label_ref = m_edge_labels[m_edge_label_map[edge_label.label_id_ref]];
        root_edge_label_ref.item_c -= edge_label.item_c;

        if (root_edge_label_ref.item_c == 0)
        {
            m_marked_vertex_labels.emplace_back(m_edge_labels.begin() + static_cast<int64_t>(m_edge_label_map[edge_label.label_id_ref]));
            m_graph_metadata.vertex_label_c--;
        }
    }
    m_marked_vertices.emplace_back(src_vertex);

    m_flush_needed = true;
    return EActionState::valid;
}

graphquery::database::storage::CMemoryModelLPG::EActionState
graphquery::database::storage::CMemoryModelLPG::rm_edge_entry(const uint64_t src_vertex_id, uint64_t dst_vertex_id) noexcept
{
    std::lock_guard lock(m_update);
    const auto src_vertex_opt = get_vertex_by_id(src_vertex_id);

    if (!src_vertex_opt.has_value())
        return EActionState::invalid;

    const auto & src_vertex = src_vertex_opt.value();

    size_t erased = {}, total = {};

    for (auto & label : src_vertex->edge_labels)
    {
        erased += std::erase_if(src_vertex->labelled_edges[label.pos], [&dst_vertex_id](const auto & edge) { return edge.dst == dst_vertex_id; });

        m_edge_labels[m_edge_label_map[label.label_id_ref]].item_c -= erased;
        label.item_c -= erased;

        if (m_edge_labels[m_edge_label_map[label.label_id_ref]].item_c == 0)
        {
            m_marked_edge_labels.push_back(m_edge_labels.begin() + static_cast<int64_t>(m_edge_label_map[label.label_id_ref]));
            m_graph_metadata.edge_label_c--;
        }

        if (label.item_c == 0)
        {
            src_vertex->edge_labels.erase(src_vertex->edge_labels.begin() + label.pos);
            src_vertex->labelled_edges.erase(src_vertex->labelled_edges.begin() + label.pos);
            src_vertex->vertex_metadata.edge_label_c--;
        }

        total += erased;
        erased = {};
    }

    if (total == 0)
        return EActionState::invalid;

    m_graph_metadata.edges_c -= total;
    src_vertex->vertex_metadata.neighbour_c -= erased;

    m_flush_needed = true;
    return EActionState::valid;
}

graphquery::database::storage::CMemoryModelLPG::EActionState
graphquery::database::storage::CMemoryModelLPG::rm_edge_entry(std::string_view label, const uint64_t src_vertex_id, uint64_t dst_vertex_id) noexcept
{
    std::lock_guard lock(m_update);
    const auto src_vertex_opt = get_vertex_by_id(src_vertex_id);

    if (!src_vertex_opt.has_value())
        return EActionState::invalid;

    const auto & src_vertex     = src_vertex_opt.value();
    const auto & edge_label_ref = check_if_edge_label_exists(label);

    if (!edge_label_ref.has_value())
        return EActionState::invalid;

    const auto & vertex_edge_label = check_if_vertex_edge_label_exists(*src_vertex, edge_label_ref.value());

    if (!vertex_edge_label.has_value())
        return EActionState::invalid;

    auto & vertex_edge_label_ref = src_vertex->edge_labels[m_edge_label_map[edge_label_ref.value()]];

    const size_t erased =
        std::erase_if(src_vertex->labelled_edges[m_edge_label_map[edge_label_ref.value()]], [&dst_vertex_id](const auto & edge) { return edge.dst == dst_vertex_id; });

    if (erased == 0)
        return EActionState::invalid;

    if (m_edge_labels[m_edge_label_map[edge_label_ref.value()]].item_c == 0)
    {
        m_marked_edge_labels.push_back(m_edge_labels.begin() + static_cast<int64_t>(m_edge_label_map[edge_label_ref.value()]));
        m_graph_metadata.edge_label_c--;
    }

    if (src_vertex->edge_labels[m_edge_label_map[edge_label_ref.value()]].item_c == 0)
    {
        src_vertex->edge_labels.erase(src_vertex->edge_labels.begin() + vertex_edge_label_ref.pos);
        src_vertex->labelled_edges.erase(src_vertex->labelled_edges.begin() + vertex_edge_label_ref.pos);
        src_vertex->vertex_metadata.edge_label_c--;
    }

    m_graph_metadata.edges_c--;
    src_vertex->vertex_metadata.neighbour_c -= erased;
    m_edge_labels[m_edge_label_map[edge_label_ref.value()]].item_c--;

    m_flush_needed = true;
    return EActionState::valid;
}

void
graphquery::database::storage::CMemoryModelLPG::add_vertex(const uint64_t id,
                                                           const std::string_view label,
                                                           const std::initializer_list<std::pair<std::string, std::string>> & prop)
{
    if (add_vertex_entry(id, label, prop) == EActionState::invalid)
    {
        m_log_system->warning(fmt::format("Issue adding vertex"));
        return;
    }

    m_log_system->debug("Vertex has been added");
    transactions->commit_vertex(label, prop, id);
}

void
graphquery::database::storage::CMemoryModelLPG::add_vertex(const std::string_view label, const std::initializer_list<std::pair<std::string, std::string>> & prop)
{
    (void) add_vertex_entry(label, prop);

    m_log_system->debug("Vertex has been added");
    transactions->commit_vertex(label, prop);
}

void
graphquery::database::storage::CMemoryModelLPG::add_edge(const uint64_t src,
                                                         const uint64_t dst,
                                                         const std::string_view label,
                                                         const std::initializer_list<std::pair<std::string, std::string>> & prop)
{
    if (add_edge_entry(src, dst, label, prop) == EActionState::invalid)
    {
        m_log_system->warning(fmt::format("Issue adding edge({}) to vertex({}) from transaction entry", dst, src));
        return;
    }

    m_log_system->debug("Edge has been added");
    transactions->commit_edge(src, dst, label, prop);
}

void
graphquery::database::storage::CMemoryModelLPG::rm_vertex(const uint64_t vertex_id)
{
    if (rm_vertex_entry(vertex_id) == EActionState::invalid)
    {
        m_log_system->warning(fmt::format("Issue removing vertex({})", vertex_id));
        return;
    }

    m_log_system->debug("Vertex has been removed");
    transactions->commit_rm_vertex(vertex_id);
}

void
graphquery::database::storage::CMemoryModelLPG::rm_edge(const uint64_t src, const uint64_t dst)
{
    if (rm_edge_entry(src, dst) == EActionState::invalid)
    {
        m_log_system->warning(fmt::format("Issue remvoing edge({}) to vertex({}) from transaction entry", dst, src));
        return;
    }
    m_log_system->debug("Edge has been removed");
    transactions->commit_rm_edge(src, dst);
}

void
graphquery::database::storage::CMemoryModelLPG::rm_edge(uint64_t src, uint64_t dst, std::string_view label)
{
    if (rm_edge_entry(label, src, dst) == EActionState::invalid)
    {
        m_log_system->warning(fmt::format("Issue remvoing edge({}) to vertex({}) from transaction entry", dst, src));
        return;
    }

    m_log_system->debug("Edge has been removed");
    transactions->commit_rm_edge(src, dst, label);
}

void
graphquery::database::storage::CMemoryModelLPG::update_vertex(uint64_t vertex_id, const std::initializer_list<std::pair<std::string, std::string>> & prop)
{
}

void
graphquery::database::storage::CMemoryModelLPG::update_edge(uint64_t edge_id, const std::initializer_list<std::pair<std::string, std::string>> & prop)
{
}

const uint64_t &
graphquery::database::storage::CMemoryModelLPG::get_num_vertices() const
{
    return this->m_graph_metadata.vertices_c;
}

const uint64_t &
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

    return src_vertex_opt.value()->vertex_metadata;
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
        const auto edge = std::find_if(label_edges.begin(), label_edges.end(), [&dst](const auto & _) { return _.dst == dst; });

        if (edge != label_edges.end())
            ret.emplace_back(*edge);
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
                                   [&dst](const auto & _) { return _.dst == dst; });

    if (edge == src_vertex->labelled_edges[vertex_edge_label_ref.value()].end())
        return std::nullopt;

    return *edge;
}

void
graphquery::database::storage::CMemoryModelLPG::relax(analytic::CRelax * relax) noexcept
{
    //~ Iterating over all vertices and edges once. Multiple for-loops
    //~ due to label separation between vertices and edges.
    for (uint16_t label = 0; label < m_graph_metadata.vertex_label_c; label++)
        for (uint64_t vertex = 0; vertex < m_vertex_labels[0].item_c; vertex++)
        {
            const auto & ref = m_labelled_vertices[label][vertex];

            for (const auto & label_edges : ref.labelled_edges)
                for (const auto & edge : label_edges)
                    relax->relax(ref.vertex_metadata.id, edge.dst);
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

extern "C"
{
    LIB_EXPORT void create_graph_model(std::shared_ptr<graphquery::database::storage::ILPGModel> & graph_model)
    {
        graph_model = std::make_shared<graphquery::database::storage::CMemoryModelLPG>();
    }
}
