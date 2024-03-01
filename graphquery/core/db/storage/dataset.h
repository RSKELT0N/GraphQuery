/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file dataset.h
 * \brief Abstract dataset class for the database to call for
 *        loading a derived (specific) dataset into the database.
 ************************************************************/

#pragma once

#include "graph_model.h"

#include <filesystem>

namespace graphquery::database::storage
{
    class CDataset
    {
      public:
        virtual ~CDataset() = default;
        explicit CDataset(std::shared_ptr<ILPGModel *> _graph): m_graph(std::move(_graph)), m_dataset_path(std::filesystem::current_path()) {}
        explicit CDataset(std::shared_ptr<ILPGModel *> _graph, std::filesystem::path & _path): m_graph(std::move(_graph)), m_dataset_path(std::move(_path)) {}

        CDataset(const CDataset &)                 = delete;
        CDataset(CDataset &&) noexcept             = delete;
        CDataset & operator=(const CDataset &)     = delete;
        CDataset & operator=(CDataset &&) noexcept = delete;

        virtual void load() const noexcept = 0;

        virtual void set_path(std::filesystem::path & _path) noexcept final { m_dataset_path = std::move(_path); }
        [[nodiscard]] virtual const std::filesystem::path & get_path() const noexcept final { return m_dataset_path; }

      protected:
        std::shared_ptr<ILPGModel *> m_graph; //~ Pointer to currently loaded graph.
        std::filesystem::path m_dataset_path; //~ Parent directory of dataset to be loaded.
    };
} // namespace graphquery::database::storage
