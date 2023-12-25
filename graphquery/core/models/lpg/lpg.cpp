#include "lpg.h"

#include <fcntl.h>
#include <iostream>
#include <string_view>
#include <sys/mman.h>
#include <optional>
#include <vector>

graphquery::database::storage::CMemoryModelLPG::
CMemoryModelLPG(): m_master_file(O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED), m_connections_file(O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED)
{
    m_log_system = logger::CLogSystem::get_instance();
}

void
graphquery::database::storage::CMemoryModelLPG::close() noexcept
{
    (void) m_master_file.close();
    (void) m_connections_file.close();
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
    read_labelled_vertices();

    m_log_system->debug(fmt::format("Vertices: {}", (int64_t) m_graph_metadata.vertices_c));
    m_log_system->debug(fmt::format("Edges: {}", (int64_t) m_graph_metadata.edges_c));
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
graphquery::database::storage::CMemoryModelLPG::define_labelled_vertices() noexcept
{
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
    m_master_file.seek(MASTER_VERTEX_MAP_START_ADDR);
    m_master_file.write(&m_vertex_labels[0], sizeof(SLabel), m_graph_metadata.vertex_label_c);
}

void
graphquery::database::storage::CMemoryModelLPG::store_labelled_vertices() noexcept
{
    m_connections_file.seek(CONNECTIONS_START_ADDR);

    for (auto & label : m_labelled_vertices)
    {
        for (auto & [metadata, edge_labels, edges, properties] : label)
        {
            m_connections_file.write(&metadata, sizeof(SVertex), 1);
            m_connections_file.write(&edge_labels[0], sizeof(SLabel), metadata.edge_label_c);
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
    m_master_file.seek(MASTER_VERTEX_MAP_START_ADDR);
    m_vertex_labels.resize(m_graph_metadata.vertex_label_c);

    m_master_file.read(&m_vertex_labels[0], sizeof(SLabel), m_graph_metadata.vertex_label_c);
}

// TODO: Will update, awful way of indexing into vertex offset, will create a mapping from indexing to vertex ID.
void
graphquery::database::storage::CMemoryModelLPG::read_labelled_vertices() noexcept
{
    m_connections_file.seek(CONNECTIONS_START_ADDR);

    //~ resize vector for n labels.
    m_labelled_vertices.resize(m_graph_metadata.vertex_label_c);

    for (auto & label : m_labelled_vertices)
    {
        for (auto & [metadata, edge_labels, edges, properties] : label)
        {
            m_connections_file.write(&metadata, sizeof(SVertex), 1);
            m_connections_file.write(&edge_labels[0], sizeof(SLabel), metadata.edge_label_c);
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
    m_vertex_labels.emplace_back(std::move(vertex_label_entry));

    m_labelled_vertices.emplace_back();

    return label_id;
}

std::optional<int64_t>
graphquery::database::storage::CMemoryModelLPG::check_if_vertex_label_exists(std::string_view label_str) const noexcept
{
    auto label_inst = std::find_if(m_vertex_labels.begin(), m_vertex_labels.end(), [&label_str](const SLabel & label) { return strcmp(label_str.data(), label.label_s) == 0; });

    if (label_inst != m_vertex_labels.end())
        return label_inst->label_id;

    return std::nullopt;
}

void
graphquery::database::storage::CMemoryModelLPG::add_vertex_entry(std::string_view label, const std::pair<std::string, std::string> & prop...) noexcept
{
    SVertexContainer vertex = {};

    //~ Assign an ID
    vertex.vertex_metadata.id = this->m_graph_metadata.vertices_c++;

    //~ Assign vertex label
    const std::optional<int64_t> label_id = check_if_vertex_label_exists(label);
    vertex.vertex_metadata.label_id       = label_id.has_value() ? label_id.value() : create_vertex_label(label);

    //~ Add vertex to DB
    m_labelled_vertices[vertex.vertex_metadata.label_id].push_back(vertex);
    m_vertex_labels[vertex.vertex_metadata.label_id].item_c++;

    store_graph_header();
    store_vertex_labels();
    store_labelled_vertices();
}

void
graphquery::database::storage::CMemoryModelLPG::add_vertex(std::string_view label, const std::pair<std::string, std::string> & prop...)
{
    add_vertex_entry(label, prop);
}

void
graphquery::database::storage::CMemoryModelLPG::add_edge(int64_t src, int64_t dst, const std::pair<std::string, std::string> & prop...)
{
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
graphquery::database::storage::CMemoryModelLPG::get_num_vertices()
{
    return this->m_graph_metadata.vertices_c;
}

int64_t
graphquery::database::storage::CMemoryModelLPG::get_num_edges()
{
    return this->m_graph_metadata.edges_c;
}

graphquery::database::storage::ILPGModel::SVertex
graphquery::database::storage::CMemoryModelLPG::get_vertex(int64_t vertex_id)
{
    return {};
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
