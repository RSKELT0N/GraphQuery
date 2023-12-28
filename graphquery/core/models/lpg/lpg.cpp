#include "lpg.h"

#include <fcntl.h>
#include <iostream>
#include <string_view>
#include <sys/mman.h>
#include <optional>
#include <vector>

//~ static symbol link
std::shared_ptr<graphquery::logger::CLogSystem> graphquery::database::storage::CMemoryModelLPG::m_log_system;

graphquery::database::storage::CMemoryModelLPG::
CMemoryModelLPG(): m_master_file(O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED), m_connections_file(O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED)
{
    m_log_system = logger::CLogSystem::get_instance();
    m_labelled_vertices = std::vector<std::vector<SVertexContainer>>();
    m_vertex_properties = std::vector<std::vector<SProperty>>();
    m_edge_properties   = std::vector<std::vector<SProperty>>();
}

void
graphquery::database::storage::CMemoryModelLPG::close() noexcept
{
    (void) m_master_file.close();
    (void) m_connections_file.close();
}

std::string
graphquery::database::storage::CMemoryModelLPG::get_name() const noexcept
{
    return m_graph_metadata.graph_name;
}


void
graphquery::database::storage::CMemoryModelLPG::create_graph(std::filesystem::path path, std::string_view graph) noexcept
{
    this->m_master_file.set_path(path);
    this->m_connections_file.set_path(path);

    (void) CDiskDriver::create_file(path, MASTER_FILE_NAME, MASTER_FILE_SIZE);
    (void) CDiskDriver::create_file(path, CONNECTIONS_FILE_NAME, CONNECTIONS_FILE_SIZE);
    (void) this->m_master_file.open(MASTER_FILE_NAME);
    (void) this->m_connections_file.open(CONNECTIONS_FILE_NAME);

    this->m_graph_name = graph;

    define_graph_header();
    store_graph_header();
}

void
graphquery::database::storage::CMemoryModelLPG::load_graph(std::filesystem::path path, std::string_view graph) noexcept
{
    this->m_master_file.set_path(path);
    this->m_connections_file.set_path(path);
    (void) this->m_master_file.open(MASTER_FILE_NAME);
    (void) this->m_connections_file.open(CONNECTIONS_FILE_NAME);

    read_graph_header();
    read_vertex_labels();
    read_edge_labels();
    read_labelled_vertices();
}

void
graphquery::database::storage::CMemoryModelLPG::define_graph_header() noexcept
{
    m_graph_metadata.vertices_c     = 0;
    m_graph_metadata.edges_c        = 0;
    m_graph_metadata.vertex_label_c = 0;
    m_graph_metadata.edge_label_c   = 0;
    (void) strcpy(m_graph_metadata.graph_name, m_graph_name.c_str());
    (void) strncpy(m_graph_metadata.graph_type, "lpg", GRAPH_MODEL_TYPE_LENGTH);
}

void
graphquery::database::storage::CMemoryModelLPG::store_graph_header() noexcept
{
    m_master_file.seek(MASTER_METADATA_START_ADDR);
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

    for (auto & label : m_labelled_vertices)
    {
        for (auto & [metadata, edge_labels, edges] : label)
        {
            m_connections_file.write(&metadata, sizeof(SVertex), 1);
            m_connections_file.write(&edge_labels[0], sizeof(SLabel), metadata.edge_label_c);

            for (const auto & [id, count] : edge_labels)
            {
                m_connections_file.write(&edges[id][0], sizeof(SEdge), count);
            }
        }
    }
}

void
graphquery::database::storage::CMemoryModelLPG::read_graph_header() noexcept
{
    m_master_file.seek(MASTER_METADATA_START_ADDR);
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

    //~ running vertex counter
    int64_t vertex_c = 0;
    //~ resize vector for n labels.
    m_labelled_vertices.resize(m_graph_metadata.vertex_label_c);

    for (const auto & [lv_str, lv_id, lv_count] : m_vertex_labels)
    {
        m_labelled_vertices[lv_id].resize(lv_count);
        for (auto & [metadata, edge_labels, edges] : m_labelled_vertices[lv_id])
        {
            m_connections_file.read(&metadata, sizeof(SVertex), 1);
            edge_labels.resize(metadata.edge_label_c);
            edges.resize(metadata.edge_label_c);
            m_connections_file.read(&edge_labels[0], sizeof(SLabel), metadata.edge_label_c);

            for (const auto & [le_id, le_count] : edge_labels)
            {
                edges[le_id].resize(le_count);
                m_connections_file.read(&edges[le_id][0], sizeof(SEdge), le_count);
            }
            m_vertex_map.emplace(metadata.id, vertex_c++);
        }
    }
}

graphquery::database::storage::CMemoryModelLPG::SLabel
graphquery::database::storage::CMemoryModelLPG::create_label(std::string_view label, int64_t label_id, int64_t vertex_c) const noexcept
{
    SLabel ret = {};
    strcpy(ret.label_s, label.data());
    ret.label_id = label_id;
    ret.item_c   = vertex_c;

    return ret;
}

int64_t
graphquery::database::storage::CMemoryModelLPG::create_vertex_label(std::string_view label_str) noexcept
{
    const auto label_id     = this->m_graph_metadata.vertex_label_c++;
    auto vertex_label_entry = create_label(label_str, label_id, 0);
    m_vertex_labels.emplace_back(vertex_label_entry);

    m_labelled_vertices.emplace_back();

    return static_cast<int64_t>(m_vertex_labels.size() - 1);
}

std::optional<int64_t>
graphquery::database::storage::CMemoryModelLPG::check_if_vertex_label_exists(std::string_view label_str) const noexcept
{
    auto label_inst = std::find_if(m_vertex_labels.begin(), m_vertex_labels.end(), [&label_str](const SLabel & label) { return strcmp(label_str.data(), label.label_s) == 0; });

    if (label_inst != m_vertex_labels.end())
        return std::distance(m_vertex_labels.begin(), label_inst);

    return std::nullopt;
}

int64_t
graphquery::database::storage::CMemoryModelLPG::create_edge_label(std::string_view label_str) noexcept
{
    const auto label_id   = this->m_graph_metadata.edge_label_c++;
    auto edge_label_entry = create_label(label_str, label_id, 0);
    m_edge_labels.emplace_back(edge_label_entry);

    return static_cast<int64_t>(m_edge_labels.size() - 1);
}

std::optional<int64_t>
graphquery::database::storage::CMemoryModelLPG::check_if_edge_label_exists(std::string_view label_str) const noexcept
{
    auto label_inst = std::find_if(m_edge_labels.begin(), m_edge_labels.end(), [&label_str](const SLabel & label) { return strcmp(label_str.data(), label.label_s) == 0; });

    if (label_inst != m_edge_labels.end())
        return std::distance(m_edge_labels.begin(), label_inst);

    return std::nullopt;
}

std::vector<graphquery::database::storage::CMemoryModelLPG::SVertexContainer>::iterator
graphquery::database::storage::CMemoryModelLPG::get_vertex_by_id(const int64_t id) noexcept
{
    const auto offset = m_vertex_map[id];
    int64_t label     = {};
    int64_t lo        = 0;

    for (auto & [str, id, count] : m_vertex_labels)
    {
        if (lo <= offset && offset <= lo + count)
        {
            label = id;
            break;
        }
        lo += count;
    }

    return m_labelled_vertices[label].begin() + offset - lo;
}

void
graphquery::database::storage::CMemoryModelLPG::add_vertex_entry(std::string_view label, const std::pair<std::string, std::string> & prop...) noexcept
{
    SVertexContainer vertex = {};

    //~ Assign vertex label
    const std::optional<int64_t> idx = check_if_vertex_label_exists(label);

    int64_t label_idx = 0;
    if (!idx.has_value()) [[likely]]
        label_idx = create_vertex_label(label);
    else
        label_idx = idx.value();

    vertex.vertex_metadata.label_id = m_vertex_labels[label_idx].label_id;
    vertex.vertex_metadata.id       = this->m_graph_metadata.vertices_c++;

    //~ Add vertex to DB
    m_labelled_vertices[label_idx].push_back(vertex);
    m_vertex_labels[label_idx].item_c++;

    store_graph_header();
    store_vertex_labels();
    store_labelled_vertices();
}

void
graphquery::database::storage::CMemoryModelLPG::add_edge_entry(int64_t src, int64_t dst, std::string_view label, const std::pair<std::string, std::string> & prop...) noexcept
{
    SEdge edge = {};
    //~ src vertex reference
    const auto src_vertex = get_vertex_by_id(src);

    //~ Assign edge info
    const std::optional<int64_t> idx = check_if_edge_label_exists(label);

    int64_t label_idx = 0;
    if (!idx.has_value())
        label_idx = create_edge_label(label);
    else
        label_idx = idx.value();

    edge.label_id = m_edge_labels[label_idx].label_id;
    edge.id       = this->m_graph_metadata.edges_c;
    edge.dst      = dst;

    //~ Update graph header
    m_graph_metadata.edges_c++;
    m_edge_labels[label_idx].item_c++;

    //~ Update vertex with new edge
    const auto contains = std::find_if(src_vertex->edge_labels.begin(), src_vertex->edge_labels.end(), [&edge](const auto & label_idx) { return edge.label_id == label_idx.label_id_ref; });
    auto distance       = std::distance(src_vertex->edge_labels.begin(), contains);

    if (contains == src_vertex->edge_labels.end())
    {
        src_vertex->edge_labels.push_back({.label_id_ref = edge.label_id, .item_c = 0});
        src_vertex->vertex_metadata.edge_label_c++;
        distance = static_cast<ptrdiff_t>(src_vertex->edge_labels.size() - 1);
        src_vertex->labelled_edges.emplace_back();
    }

    src_vertex->labelled_edges[distance]. push_back(edge);
    src_vertex->edge_labels[distance].item_c++;
    src_vertex->vertex_metadata.neighbour_c++;

    store_graph_header();
    store_edge_labels();
    store_labelled_vertices();
}

void
graphquery::database::storage::CMemoryModelLPG::add_vertex(std::string_view label, const std::pair<std::string, std::string> & prop...)
{
    add_vertex_entry(label, prop);
}

void
graphquery::database::storage::CMemoryModelLPG::add_edge(int64_t src, int64_t dst, std::string_view label, const std::pair<std::string, std::string> & prop...)
{
    add_edge_entry(src, dst, label, prop);
}

void
graphquery::database::storage::CMemoryModelLPG::delete_vertex(int64_t vertex_id)
{
}

void
graphquery::database::storage::CMemoryModelLPG::delete_edge(int64_t src, int64_t dst)
{
}

void
graphquery::database::storage::CMemoryModelLPG::update_vertex(int64_t vertex_id, const std::pair<std::string, std::string> & prop...)
{
}

void
graphquery::database::storage::CMemoryModelLPG::update_edge(int64_t edge_id, const std::pair<std::string, std::string> & prop...)
{
}

int64_t
graphquery::database::storage::CMemoryModelLPG::get_num_vertices() const
{
    return this->m_graph_metadata.vertices_c;
}

int64_t
graphquery::database::storage::CMemoryModelLPG::get_num_edges() const
{
    return this->m_graph_metadata.edges_c;
}

graphquery::database::storage::ILPGModel::SVertex
graphquery::database::storage::CMemoryModelLPG::get_vertex(int64_t vertex_id)
{
    return get_vertex_by_id(vertex_id)->vertex_metadata;
}

graphquery::database::storage::ILPGModel::SEdge
graphquery::database::storage::CMemoryModelLPG::get_edge(int64_t edge_id)
{
    return {};
}

std::vector<graphquery::database::storage::ILPGModel::SVertex>
graphquery::database::storage::CMemoryModelLPG::get_vertices_by_label(int64_t label_id)
{
    return {};
}

std::vector<graphquery::database::storage::ILPGModel::SVertex>
graphquery::database::storage::CMemoryModelLPG::get_edges_by_label(int64_t label_id)
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
