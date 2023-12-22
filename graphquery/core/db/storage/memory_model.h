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

#include <memory>
#include <string_view>

#include "fmt/format.h"
#include "diskdriver/diskdriver.h"

namespace graphquery::database::storage
{
    class IMemoryModel
    {
      public:
        IMemoryModel()          = default;
        virtual ~IMemoryModel() = default;

        virtual void init(const std::filesystem::path path, std::string_view graph) final
        {
            if (!CDiskDriver::check_if_folder_exists(path.generic_string() + fmt::format("/{}", graph)))
            {
                CDiskDriver::create_folder(path, graph);
                create_graph(path / graph, graph);
            }
            else
            {
                load_graph(path / graph, graph);
            }
        }

        friend class CDBStorage;

        virtual void close() noexcept                                                          = 0;
        virtual void load_graph(std::filesystem::path path, std::string_view graph) noexcept   = 0;
        virtual void create_graph(std::filesystem::path path, std::string_view graph) noexcept = 0;
    };
} // namespace graphquery::database::storage

extern "C"
{
    void create_memory_model(std::shared_ptr<graphquery::database::storage::IMemoryModel> & memory_model);
}
