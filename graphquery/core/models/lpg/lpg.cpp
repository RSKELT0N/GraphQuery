#include "lpg.h"

#include <fcntl.h>
#include <string_view>
#include <sys/mman.h>
#include <vector>

graphquery::database::storage::CMemoryModelLPG::CMemoryModelLPG(): m_master_file(O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED), m_connections_file(O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED)
{
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
    m_master_file.seek(GRAPH_METADATA_START_ADDR);
    m_master_file.write(&m_graph_metadata, sizeof(SGraphMetaData_t), 1);
}

void
graphquery::database::storage::CMemoryModelLPG::store_vertex_labels() noexcept
{
    m_master_file.seek(GRAPH_VERTEX_LABELS_START_ADDR);
    m_master_file.write(&m_vertex_labels[0], sizeof(SVertexLabel_t), m_graph_metadata.vertex_label_c);
}

void
graphquery::database::storage::CMemoryModelLPG::read_graph_header() noexcept
{
    m_master_file.seek(GRAPH_METADATA_START_ADDR);
    m_master_file.read(&m_graph_metadata, sizeof(SGraphMetaData_t), 1);
}

void
graphquery::database::storage::CMemoryModelLPG::read_vertex_labels() noexcept
{
    m_master_file.seek(GRAPH_VERTEX_LABELS_START_ADDR);
    m_vertex_labels.resize(m_graph_metadata.vertex_label_c);

    m_master_file.read(&m_vertex_labels[0], sizeof(SVertexLabel_t), m_vertex_labels.size());
}

int16_t
graphquery::database::storage::CMemoryModelLPG::get_vertex_label_if_exists(std::string_view label_str) const noexcept
{
    int16_t exist = -1;

    std::for_each(m_vertex_labels.begin(),
                  m_vertex_labels.end(),
                  [&exist, &label_str](const auto & label) -> void
                  {
                      if (strcmp(label_str.data(), label.label_s) == 0)
                          exist = label.label_id;
                  });

    return exist;
}

graphquery::database::storage::CMemoryModelLPG::SVertexLabel_t
graphquery::database::storage::CMemoryModelLPG::create_vertex_label(std::string_view label, int16_t label_id, int64_t vertex_c) const noexcept
{
    SVertexLabel_t ret = {};
    strcpy(ret.label_s, label.data());
    ret.label_id = label_id;
    ret.vertex_c = vertex_c;

    return ret;
}

void
graphquery::database::storage::CMemoryModelLPG::add_vertex_entry(SVertex & vertex, std::string_view label) noexcept
{
    if (static_cast<size_t>(vertex.label_id) > m_vertex_labels.size())
    {
        m_labelled_vertices[vertex.label_id].push_back(vertex);
        m_vertex_labels[vertex.label_id].vertex_c++;
    }
    else
    {
        auto vertex_label_entry = create_vertex_label(label, vertex.label_id, 0);
        m_vertex_labels.emplace_back(vertex_label_entry);

        m_labelled_vertices.emplace_back();
        m_labelled_vertices[vertex.label_id].push_back(vertex);
    }

    store_graph_header();
    store_vertex_labels();
}

void
graphquery::database::storage::CMemoryModelLPG::add_vertex(std::string_view label, SProperty & properties)
{
    SProperty property = {};
    SVertex vertex     = {};

    property.property_c   = properties.property_c;
    property.prev_version = properties.prev_version;
    property.properties.reserve(property.property_c);
    property.properties.assign(properties.properties.begin(), properties.properties.end());

    vertex.id = this->m_graph_metadata.vertices_c++;

    if ((vertex.label_id = get_vertex_label_if_exists(label)) == -1)
        vertex.label_id = this->m_graph_metadata.vertex_label_c++;

    add_vertex_entry(vertex, label);
}

void
graphquery::database::storage::CMemoryModelLPG::add_edge(int64_t src, int64_t dst, SProperty & properties)
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
graphquery::database::storage::CMemoryModelLPG::update_vertex(int64_t vertex_id, SProperty & properties)
{
}

void
graphquery::database::storage::CMemoryModelLPG::update_edge(int64_t edge_id, SProperty & properties)
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

graphquery::database::storage::CLPGModel::SVertex
graphquery::database::storage::CMemoryModelLPG::get_vertex(int64_t vertex_id)
{
    return {};
}

graphquery::database::storage::CLPGModel::SVertex
graphquery::database::storage::CMemoryModelLPG::get_edge(int64_t edge_id)
{
    return {};
}

std::vector<graphquery::database::storage::CLPGModel::SVertex>
graphquery::database::storage::CMemoryModelLPG::get_vertices_by_label(int64_t label_id)
{
    return {};
}

std::vector<graphquery::database::storage::CLPGModel::SVertex>
graphquery::database::storage::CMemoryModelLPG::get_edges_by_label(int64_t label_id)
{
    return {};
}

extern "C"
{
    LIB_EXPORT void create_graph_model(std::unique_ptr<graphquery::database::storage::IMemoryModel> & model)
    {
        model = std::make_unique<graphquery::database::storage::CMemoryModelLPG>();
    }
}
