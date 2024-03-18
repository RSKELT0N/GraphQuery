#pragma once

#include "db/storage/graph_model.h"
#include "db/storage/model.h"

namespace graphquery::database::analytic
{
    class CLightWeightGraphModel : public storage::IModel
    {
    public:
        CLightWeightGraphModel(const std::vector<storage::ILPGModel::SEdge_t> &);
        ~CLightWeightGraphModel() = default;

        [[nodiscard]] int64_t get_num_edges() override;
        [[nodiscard]] int64_t get_num_vertices() override;
        uint32_t out_degree(storage::Id_t id) noexcept override;
        uint32_t in_degree(storage::Id_t id) noexcept override;
        void calc_outdegree(uint32_t[]) noexcept override;
        void calc_indegree(uint32_t[]) noexcept override;
        void calc_vertex_sparse_map(storage::Id_t[]) noexcept override;
        int64_t get_total_num_vertices() noexcept override;
        void edgemap(const std::unique_ptr<IRelax> & relax) override;
        void src_edgemap(storage::Id_t _src, const std::function<void(int64_t src, int64_t dst)> &) override;
        std::unique_ptr<std::vector<std::vector<int64_t>>> make_inverse_graph() noexcept override;

    private:
        std::vector<storage::ILPGModel::SEdge_t> remap(const std::vector<storage::ILPGModel::SEdge_t> &) noexcept;
        void build_graph(const std::vector<storage::ILPGModel::SEdge_t> & edges) noexcept;

        storage::Id_t n;
        uint32_t num_edges;
        std::vector<uint32_t> indegree;
        std::vector<std::vector<storage::Id_t>> m_graph;
    };

    inline
    CLightWeightGraphModel::CLightWeightGraphModel(const std::vector<storage::ILPGModel::SEdge_t> & edges) : IModel()
    {
        const auto remapped = remap(edges);
        build_graph(remapped);
    }

    inline std::vector<storage::ILPGModel::SEdge_t>
    CLightWeightGraphModel::remap(const std::vector<storage::ILPGModel::SEdge_t> & edges) noexcept
    {
        std::vector<storage::ILPGModel::SEdge_t> cpy = edges;

        storage::Id_t vertex_i = 0;
        std::unordered_map<storage::Id_t, storage::Id_t> mapped_ids;
        for(auto & edge : cpy)
        {
            if(!mapped_ids.contains(edge.src))
                mapped_ids[edge.src] = vertex_i++;

            if(!mapped_ids.contains(edge.dst))
                mapped_ids[edge.dst] = vertex_i++;

            edge.src = mapped_ids[edge.src];
            edge.dst = mapped_ids[edge.dst];
        }
        n = mapped_ids.size();

        return cpy;
    }

    inline void
    CLightWeightGraphModel::build_graph(const std::vector<storage::ILPGModel::SEdge_t> & edges) noexcept
    {
        m_graph.resize(n);
        indegree.resize(n);

        for(const storage::ILPGModel::SEdge_t & edge : edges)
        {
            m_graph[edge.src].emplace_back(edge.dst);
            indegree[edge.dst]++;
        }

        num_edges = edges.size();
    }

    inline int64_t
    CLightWeightGraphModel::get_num_edges()
    {
        return num_edges;
    }

    inline int64_t
    CLightWeightGraphModel::get_num_vertices()
    {
        return n;
    }

    inline uint32_t
    CLightWeightGraphModel::out_degree(const storage::Id_t id) noexcept
    {
        return m_graph[id].size();
    }

    inline uint32_t
    CLightWeightGraphModel::in_degree(const storage::Id_t id) noexcept
    {
        return indegree[id];
    }

    inline void
    CLightWeightGraphModel::calc_outdegree(uint32_t out[]) noexcept
    {
#pragma omp parallel for default(none) shared(out)
        for(storage::Id_t i = 0; i < n; i++)
            out[i] = m_graph[i].size();
    }

    inline void
    CLightWeightGraphModel::calc_indegree(uint32_t in[]) noexcept
    {
#pragma omp parallel for default(none) shared(in)
        for(storage::Id_t i = 0; i < n; i++)
            in[i] = indegree[i];
    }

    inline void
    CLightWeightGraphModel::calc_vertex_sparse_map(storage::Id_t sparse[]) noexcept
    {
#pragma omp parallel for default(none) shared(sparse)
        for(storage::Id_t i = 0; i < n; i++)
        {
            sparse[i] = i;
        }
    }

    inline int64_t
    CLightWeightGraphModel::get_total_num_vertices() noexcept
    {
        return n;
    }

    inline void
    CLightWeightGraphModel::edgemap(const std::unique_ptr<IRelax> & relax)
    {
#pragma omp parallel for default(none) shared(relax)
        for(storage::Id_t src = 0; src < n; src++)
        {
            for(const auto dst : m_graph[src])
                relax->relax(src, dst);
        }
    }

    inline void
    CLightWeightGraphModel::src_edgemap(const storage::Id_t _src, const std::function<void(int64_t src, int64_t dst)> & func)
    {
        for(const auto dst : m_graph[_src])
            func(_src, dst);
    }

    inline std::unique_ptr<std::vector<std::vector<int64_t>>>
    CLightWeightGraphModel::make_inverse_graph() noexcept
    {
        auto inv_graph = std::make_unique<std::vector<std::vector<int64_t>>>();
        inv_graph->resize(n);

        for(storage::Id_t src = 0; src < n; src++)
            for(storage::Id_t dst = 0; dst < m_graph[src].size(); dst++)
                (*inv_graph)[dst].emplace_back(src);

        return inv_graph;
    }
}
