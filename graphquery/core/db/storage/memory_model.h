/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file memory_model.h
 * \brief Header of the Graph data model API for shared libraries
 *        to include to define an appropiate implementation.
 *        Such as the Labelled directed graph, will be instantiated
 *        and linked at run-time to read a graph of this type.
 *
 *        (under development)
 ************************************************************/

#pragma once

#include "fmt/format.h"
#include "diskdriver/diskdriver.h"
#include "db/analytic/relax.h"

#include <string_view>

namespace graphquery::database::storage
{
    class IMemoryModel
    {
    public:
        IMemoryModel()          = default;
        virtual ~IMemoryModel() = default;

        friend class CDBStorage;
        virtual void close() noexcept = 0;
        virtual void sync_graph() noexcept = 0;
        virtual void create_rollback(std::string_view) noexcept = 0;
        virtual void rollback(uint8_t rollback_entry) noexcept = 0;
        virtual std::vector<std::string> fetch_rollback_table() const noexcept = 0;
        virtual uint32_t out_degree(Id_t id) noexcept = 0;
        virtual uint32_t in_degree(Id_t id) noexcept = 0;
        virtual void calc_outdegree(uint32_t []) noexcept = 0;
        virtual void calc_indegree(uint32_t []) noexcept = 0;
        virtual void calc_vertex_sparse_map(Id_t []) noexcept = 0;
        virtual double get_avg_out_degree() noexcept = 0;
        [[nodiscard]] virtual std::string_view get_name() noexcept = 0;
        virtual uint32_t out_degree_by_offset(Id_t id) noexcept = 0;
        virtual int64_t get_total_num_vertices() noexcept = 0;
        virtual std::optional<Id_t> get_vertex_idx(Id_t id) noexcept = 0;
        virtual void edgemap(const std::unique_ptr<analytic::IRelax> & relax) = 0;
        virtual void src_edgemap(Id_t vertex_offset, const std::function<void(int64_t src, int64_t dst)> &) = 0;
        virtual void load_graph(std::filesystem::path path, std::string_view graph) noexcept = 0;
        virtual void create_graph(std::filesystem::path path, std::string_view graph) noexcept = 0;
        virtual std::unique_ptr<std::vector<std::vector<int64_t>>> make_inverse_graph() noexcept = 0;

    private:
        virtual void init(const std::filesystem::path path, std::string_view graph) final
        {
            if (!CDiskDriver::check_if_folder_exists(path.generic_string() + fmt::format("/{}", graph)))
            {
                (void) CDiskDriver::create_folder(path, graph);
                create_graph(path / graph, graph);
            }
            else
                load_graph(path / graph, graph);
        }
    };
} // namespace graphquery::database::storage