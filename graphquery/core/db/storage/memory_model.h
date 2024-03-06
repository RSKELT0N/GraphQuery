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
        virtual void close() noexcept                                                                          = 0;
        virtual void save_graph() noexcept                                                                     = 0;
        virtual uint32_t out_degree(int64_t id) noexcept                                                        = 0;
        virtual void calc_outdegree(uint32_t[]) noexcept                                                       = 0;
        [[nodiscard]] virtual std::string_view get_name() noexcept                                             = 0;
        virtual uint32_t out_degree_by_offset(uint32_t id) noexcept                                             = 0;
        virtual void edgemap(const std::unique_ptr<analytic::IRelax> & relax)                                  = 0;
        virtual void src_edgemap(int32_t vertex_offset, const std::function<void(int64_t src, int64_t dst)> &) = 0;
        virtual void load_graph(std::filesystem::path path, std::string_view graph) noexcept                   = 0;
        virtual void create_graph(std::filesystem::path path, std::string_view graph) noexcept                 = 0;
        virtual std::unique_ptr<std::vector<std::vector<int64_t>>> make_inverse_graph() noexcept               = 0;

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
