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
    std::unique_ptr<graphquery::database::storage::CTransaction> transactions;
}

//~ static symbol link
std::shared_ptr<graphquery::logger::CLogSystem> graphquery::database::storage::CMemoryModelLPG::m_log_system;

graphquery::database::storage::CMemoryModelLPG::CMemoryModelLPG(): m_master_file(O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED), m_connections_file(O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED), m_flush_needed(false)
{
    m_log_system        = logger::CLogSystem::get_instance();
    m_labelled_vertices = std::vector<std::vector<SVertexContainer>>();
    m_vertex_properties = std::vector<std::vector<SProperty>>();
    m_edge_properties   = std::vector<std::vector<SProperty>>();
    m_marked_vertices   = std::vector<std::vector<SVertexContainer>::iterator>();
    m_vertex_map        = std::vector<std::unordered_map<int64_t, int64_t>>();
}

void
graphquery::database::storage::CMemoryModelLPG::close() noexcept
{
    (void) m_master_file.close();
    (void) m_connections_file.close();
    transactions->close();
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
        store_graph_header();
        store_vertex_labels();
        store_edge_labels();
        store_labelled_vertices();
        transactions->reset();
        m_flush_needed = false;
    }
}

void
graphquery::database::storage::CMemoryModelLPG::create_graph(std::filesystem::path path, std::string_view graph) noexcept
{
    this->m_master_file.set_path(path);
    this->m_connections_file.set_path(path);
    transactions = std::make_unique<CTransaction>(path, this);

    (void) CDiskDriver::create_file(path, MASTER_FILE_NAME, MASTER_FILE_SIZE);
    (void) CDiskDriver::create_file(path, CONNECTIONS_FILE_NAME, CONNECTIONS_FILE_SIZE);
    (void) this->m_master_file.open(MASTER_FILE_NAME);
    (void) this->m_connections_file.open(CONNECTIONS_FILE_NAME);
    transactions->init();

    this->m_graph_name = graph;

    define_graph_header();
    store_graph_header();
}

void
graphquery::database::storage::CMemoryModelLPG::load_graph(std::filesystem::path path, std::string_view graph) noexcept
{
    this->m_master_file.set_path(path);
    this->m_connections_file.set_path(path);
    transactions = std::make_unique<CTransaction>(path, this);

    (void) this->m_master_file.open(MASTER_FILE_NAME);
    (void) this->m_connections_file.open(CONNECTIONS_FILE_NAME);
    transactions->init();

    read_graph_header();
    read_vertex_labels();
    read_edge_labels();
    read_labelled_vertices();
    transactions->handle_transactions();
}

void
graphquery::database::storage::CMemoryModelLPG::remove_marked_vertices() noexcept
{
    for (const auto vertex : m_marked_vertices)
    {
        const auto & label = vertex->vertex_metadata.label_id;
        m_labelled_vertices[label].erase(vertex);
        m_vertex_map[label].erase(vertex->vertex_metadata.id);
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

    for (int label_id = 0; label_id < m_graph_metadata.vertex_label_c; label_id++)
    {
        int64_t vertex_c = {};
        for (auto & [metadata, edge_labels, edges] : m_labelled_vertices[label_id])
        {
            m_connections_file.write(&metadata, sizeof(SVertex), 1);
            m_connections_file.write(&edge_labels[0], sizeof(SEdgeLabelEntry), metadata.edge_label_c);

            for (const auto & [id, count] : edge_labels)
            {
                m_connections_file.write(&edges[id][0], sizeof(SEdge), count);
            }
            m_vertex_map[label_id][metadata.id] = vertex_c++;
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

            m_connections_file.read(&edge_labels[0], sizeof(SEdgeLabelEntry), metadata.edge_label_c);
            edges.resize(metadata.edge_label_c);

            for (const auto & [le_id, le_count] : edge_labels)
            {
                edges[le_id].resize(le_count);
                m_connections_file.read(&edges[le_id][0], sizeof(SEdge), le_count);
            }
            m_vertex_map[metadata.label_id][metadata.id] = vertex_c++;
        }
    }
}

int64_t
graphquery::database::storage::CMemoryModelLPG::get_unassigned_vertex_id(const int64_t label_idx) const noexcept
{
    auto base_id  = m_graph_metadata.vertices_c;
    auto assigned = m_vertex_map[label_idx].contains(base_id);

    while (assigned)
        assigned = m_vertex_map[label_idx].contains(++base_id);

    return base_id;
}

graphquery::database::storage::CMemoryModelLPG::SLabel
graphquery::database::storage::CMemoryModelLPG::create_label(const std::string_view label, const int64_t label_id, const int64_t vertex_c) const noexcept
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
    m_vertex_map.emplace_back();

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

std::optional<int64_t>
graphquery::database::storage::CMemoryModelLPG::check_if_vertex_label_exists(int64_t idx) const noexcept
{
    auto label_inst = std::find_if(m_vertex_labels.begin(), m_vertex_labels.end(), [&idx](const SLabel & label) { return idx == label.label_id; });

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
    const auto label_inst = std::find_if(m_edge_labels.begin(), m_edge_labels.end(), [&label_str](const SLabel & label) { return strcmp(label_str.data(), label.label_s) == 0; });

    if (label_inst != m_edge_labels.end())
        return std::distance(m_edge_labels.begin(), label_inst);

    return std::nullopt;
}

std::optional<std::vector<graphquery::database::storage::CMemoryModelLPG::SVertexContainer>::iterator>
graphquery::database::storage::CMemoryModelLPG::get_vertex_by_id(const int64_t id) noexcept
{
    const auto label = std::find_if(m_vertex_map.begin(), m_vertex_map.end(), [&id](auto & bucket) { return bucket.contains(id); });

    if (label == m_vertex_map.end())
        return std::nullopt;

    const int64_t offset = (*label)[id];

    if (offset == -1)
        return std::nullopt;

    return m_labelled_vertices[std::distance(m_vertex_map.begin(), label)].begin() + offset;
}

graphquery::database::storage::CMemoryModelLPG::EActionState
graphquery::database::storage::CMemoryModelLPG::add_vertex_entry(const int64_t id, const std::string_view label, const std::vector<std::pair<std::string, std::string>> & prop) noexcept
{
    std::lock_guard lock(m_update);
    SVertexContainer vertex = {};

    //~ Assign vertex label
    const std::optional<int64_t> idx = check_if_vertex_label_exists(label);

    int64_t label_idx = 0;
    if (!idx.has_value()) [[likely]]
        label_idx = create_vertex_label(label);
    else
        label_idx = idx.value();

    vertex.vertex_metadata.label_id = m_vertex_labels[label_idx].label_id;

    //~ Check if vertex ID is present
    if (get_vertex_by_id(id).has_value())
        return EActionState::invalid;

    vertex.vertex_metadata.id = id;

    //~ Add vertex to DB
    m_graph_metadata.vertices_c++;
    const auto vertex_offset = static_cast<int64_t>(m_labelled_vertices.size());

    m_vertex_labels[label_idx].item_c++;
    m_labelled_vertices[label_idx].emplace_back(vertex);
    m_vertex_map[label_idx][vertex.vertex_metadata.id] = vertex_offset;

    m_flush_needed = true;
    return EActionState::valid;
}

graphquery::database::storage::CMemoryModelLPG::EActionState
graphquery::database::storage::CMemoryModelLPG::add_vertex_entry(const std::string_view label, const std::vector<std::pair<std::string, std::string>> & prop) noexcept
{
    std::lock_guard lock(m_update);
    SVertexContainer vertex = {};

    //~ Assign vertex label
    const std::optional<int64_t> idx = check_if_vertex_label_exists(label);

    int64_t label_idx = 0;
    if (!idx.has_value()) [[likely]]
        label_idx = create_vertex_label(label);
    else
        label_idx = idx.value();

    vertex.vertex_metadata.label_id = m_vertex_labels[label_idx].label_id;
    vertex.vertex_metadata.id       = get_unassigned_vertex_id(label_idx);

    //~ Add vertex to DB
    m_graph_metadata.vertices_c++;
    const auto vertex_offset = static_cast<int64_t>(m_labelled_vertices.size());

    m_vertex_labels[label_idx].item_c++;
    m_labelled_vertices[label_idx].emplace_back(vertex);
    m_vertex_map[label_idx][vertex.vertex_metadata.id] = vertex_offset;

    m_flush_needed = true;
    return EActionState::valid;
}

graphquery::database::storage::CMemoryModelLPG::EActionState
graphquery::database::storage::CMemoryModelLPG::add_edge_entry(const int64_t src, const int64_t dst, const std::string_view label, const std::vector<std::pair<std::string, std::string>> & prop) noexcept
{
    std::lock_guard lock(m_update);
    SEdge edge = {};
    //~ src vertex reference
    const auto src_vertex_opt = get_vertex_by_id(src);

    if (!src_vertex_opt.has_value())
        return EActionState::invalid;

    const auto & src_vertex = src_vertex_opt.value();

    //~ Assign edge info
    const std::optional<int64_t> idx = check_if_edge_label_exists(label);

    int64_t label_idx = 0;
    if (!idx.has_value())
        label_idx = create_edge_label(label);
    else
        label_idx = idx.value();

    edge.label_id = m_edge_labels[label_idx].label_id;
    edge.dst      = dst;

    //~ Update graph header
    m_graph_metadata.edges_c++;
    m_edge_labels[label_idx].item_c++;

    //~ Update vertex with new edge
    const auto contains = std::find_if(src_vertex->edge_labels.begin(), src_vertex->edge_labels.end(), [&edge](const auto & _) { return edge.label_id == _.label_id_ref; });
    auto distance       = std::distance(src_vertex->edge_labels.begin(), contains);

    if (contains == src_vertex->edge_labels.end())
    {
        src_vertex->edge_labels.push_back({.label_id_ref = edge.label_id, .item_c = 0});
        src_vertex->vertex_metadata.edge_label_c++;
        distance = static_cast<ptrdiff_t>(src_vertex->edge_labels.size() - 1);
        src_vertex->labelled_edges.emplace_back();
    }

    src_vertex->labelled_edges[distance].push_back(edge);
    src_vertex->edge_labels[distance].item_c++;
    src_vertex->vertex_metadata.neighbour_c++;
    m_flush_needed = true;

    return EActionState::valid;
}

graphquery::database::storage::CMemoryModelLPG::EActionState
graphquery::database::storage::CMemoryModelLPG::rm_vertex_entry(const int64_t vertex_id) noexcept
{
    std::lock_guard lock(m_update);
    const auto src_vertex_opt = get_vertex_by_id(vertex_id);

    if (!src_vertex_opt.has_value())
        return EActionState::invalid;

    auto src_vertex                  = src_vertex_opt.value();
    const std::optional<int64_t> idx = check_if_vertex_label_exists(src_vertex->vertex_metadata.label_id);

    m_marked_vertices.emplace_back(src_vertex);
    m_vertex_map[idx.value()][vertex_id] = -1;
    m_graph_metadata.vertices_c--;

    m_vertex_labels[idx.value()].item_c--;

    return EActionState::valid;
}

graphquery::database::storage::CMemoryModelLPG::EActionState
graphquery::database::storage::CMemoryModelLPG::rm_edge_entry(const int64_t src_vertex_id, int64_t dst_vertex_id) noexcept
{
    std::lock_guard lock(m_update);
    const auto src_vertex_opt = get_vertex_by_id(src_vertex_id);

    if (!src_vertex_opt.has_value())
        return EActionState::invalid;

    const auto & src_vertex = src_vertex_opt.value();

    size_t erased = 0;

    for (auto & label : src_vertex->labelled_edges)
        erased += std::erase_if(label, [&dst_vertex_id](const auto & edge) { return edge.dst == dst_vertex_id; });

    if (erased == 0)
        return EActionState::invalid;

    m_graph_metadata.edges_c--;
    return EActionState::valid;
}

void
graphquery::database::storage::CMemoryModelLPG::add_vertex(const int64_t id, const std::string_view label, const std::initializer_list<std::pair<std::string, std::string>> & prop)
{
    (void) add_vertex_entry(id, label, prop);
    transactions->commit_vertex(label, prop, id);
}

void
graphquery::database::storage::CMemoryModelLPG::add_vertex(const std::string_view label, const std::initializer_list<std::pair<std::string, std::string>> & prop)
{
    (void) add_vertex_entry(label, prop);
    transactions->commit_vertex(label, prop);
}

void
graphquery::database::storage::CMemoryModelLPG::add_edge(const int64_t src, const int64_t dst, const std::string_view label, const std::initializer_list<std::pair<std::string, std::string>> & prop)
{
    if (add_edge_entry(src, dst, label, prop) == EActionState::invalid)
        m_log_system->warning(fmt::format("Issue adding edge({}) to vertex({}) from transaction entry", dst, src));

    transactions->commit_edge(src, dst, label, prop);
}

void
graphquery::database::storage::CMemoryModelLPG::rm_vertex(const int64_t vertex_id)
{
    if (rm_vertex_entry(vertex_id) == EActionState::invalid)
        m_log_system->warning(fmt::format("Issue removing vertex({})", vertex_id));

    transactions->commit_rm_vertex(vertex_id);
}

void
graphquery::database::storage::CMemoryModelLPG::rm_edge(const int64_t src, const int64_t dst)
{
    if (rm_edge_entry(src, dst) == EActionState::invalid)
        m_log_system->warning(fmt::format("Issue remvoing edge({}) to vertex({}) from transaction entry", dst, src));

    transactions->commit_rm_edge(src, dst);
}

void
graphquery::database::storage::CMemoryModelLPG::update_vertex(int64_t vertex_id, const std::initializer_list<std::pair<std::string, std::string>> & prop)
{
}

void
graphquery::database::storage::CMemoryModelLPG::update_edge(int64_t edge_id, const std::initializer_list<std::pair<std::string, std::string>> & prop)
{
}

const int64_t &
graphquery::database::storage::CMemoryModelLPG::get_num_vertices() const
{
    return this->m_graph_metadata.vertices_c;
}

const int64_t &
graphquery::database::storage::CMemoryModelLPG::get_num_edges() const
{
    return this->m_graph_metadata.edges_c;
}

std::optional<graphquery::database::storage::ILPGModel::SVertex>
graphquery::database::storage::CMemoryModelLPG::get_vertex(const int64_t vertex_id)
{
    const auto src_vertex_opt = get_vertex_by_id(vertex_id);

    if (!src_vertex_opt.has_value())
    {
        m_log_system->warning(fmt::format("Issue retrieving vertex({})", vertex_id));
        return std::nullopt;
    }

    return src_vertex_opt.value()->vertex_metadata;
}

std::optional<graphquery::database::storage::ILPGModel::SEdge>
graphquery::database::storage::CMemoryModelLPG::get_edge(int64_t src, int64_t dst)
{
    const auto src_vertex_opt = get_vertex_by_id(src);

    if (!src_vertex_opt.has_value())
    {
        m_log_system->warning(fmt::format("Issue retrieving edge({}) from vertex({})", dst, src));
        return std::nullopt;
    }

    const auto & src_vertex = src_vertex_opt.value();

    for (const auto & label_edges : src_vertex->labelled_edges)
    {
        const auto edge = std::find_if(label_edges.begin(), label_edges.end(), [&dst](const auto & _) { return _.dst == dst; });

        if (edge != label_edges.end())
            return *edge;
    }

    return std::nullopt;
}

void
graphquery::database::storage::CMemoryModelLPG::relax(analytic::CRelax relax) noexcept
{
    //~ Iterating over all edges once, and the edges once. Multiple for-loops
    //~ due to label separation between vertices and edges.
    for (int16_t label = 0; label < m_graph_metadata.vertex_label_c; label++)
        for (int64_t vertex = 0; vertex < m_vertex_labels[0].item_c; vertex++)
        {
            const auto & ref = m_labelled_vertices[label][vertex];

            for (const auto & label_edges : ref.labelled_edges)
                for (const auto & edge : label_edges)
                    relax.relax(ref.vertex_metadata.id, edge.dst);
        }
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
