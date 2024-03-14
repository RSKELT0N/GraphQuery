/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file model.h
 * \brief Header of the Graph model API for a lightweight API
 *        for accessing graphs with a simple structure.
 *        (under development)
 ************************************************************/

#pragma once

#include "diskdriver/diskdriver.h"
#include "db/analytic/relax.h"

namespace graphquery::database::storage
{
    class IModel
    {
    public:
        IModel()          = default;
        virtual ~IModel() = default;

        friend class CDBStorage;
        [[nodiscard]] virtual int64_t get_num_edges() = 0;
        [[nodiscard]] virtual int64_t get_num_vertices() = 0;
        virtual uint32_t out_degree(Id_t id) noexcept = 0;
        virtual uint32_t in_degree(Id_t id) noexcept = 0;
        virtual void calc_outdegree(uint32_t []) noexcept = 0;
        virtual void calc_indegree(uint32_t []) noexcept = 0;
        virtual void calc_vertex_sparse_map(Id_t []) noexcept = 0;
        virtual int64_t get_total_num_vertices() noexcept = 0;
        virtual void edgemap(const std::unique_ptr<analytic::IRelax> & relax) = 0;
        virtual void src_edgemap(Id_t vertex_offset, const std::function<void(int64_t src, int64_t dst)> &) = 0;
        virtual std::unique_ptr<std::vector<std::vector<int64_t>>> make_inverse_graph() noexcept = 0;
    };
} // namespace graphquery::database::storage