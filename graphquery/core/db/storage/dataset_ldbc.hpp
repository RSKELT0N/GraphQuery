/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file dataset_ldbc.hpp
 * \brief Derived instance of the dataset class for loading
 *        LDBC SNB datasets into the graph model API.
 ************************************************************/

#pragma once

#include "dataset.h"
#include "db/system.h"

#include <filesystem>
#include <rapidcsv.h>

namespace graphquery::database::storage
{
    class CDatasetLDBC final : public CDataset
    {
      public:
        ~CDatasetLDBC() override;
        explicit CDatasetLDBC(std::shared_ptr<ILPGModel *> _graph): CDataset(std::move(_graph)) {}
        explicit CDatasetLDBC(std::shared_ptr<ILPGModel *> _graph, std::filesystem::path & _path): CDataset(std::move(_graph), _path) {}

        CDatasetLDBC(const CDatasetLDBC &)                 = delete;
        CDatasetLDBC(CDatasetLDBC &&) noexcept             = delete;
        CDatasetLDBC & operator=(const CDatasetLDBC &)     = delete;
        CDatasetLDBC & operator=(CDatasetLDBC &&) noexcept = delete;

        void load() const noexcept override;

      private:
        struct SEdgeInfo
        {
            uint32_t src_idx;
            uint32_t dst_idx;
            std::string src_type;
            std::string dst_type;
            std::string edge_type;
        };

        void load_vertex_file(std::string_view file_name, const rapidcsv::Document & fd) const noexcept;
        void load_edge_file(std::string_view file_name, const rapidcsv::Document & fd) const noexcept;
        void load_dataset_static(const std::filesystem::path & path) const noexcept;
        void load_dataset_dynamic(const std::filesystem::path & path) const noexcept;

        std::unordered_map<std::string, SEdgeInfo> m_relation_info = {{"Organisation_isLocatedIn_Place", SEdgeInfo {.src_idx = 0, .dst_idx = 1}}};
    };

    inline CDatasetLDBC::~CDatasetLDBC() = default;

    inline void CDatasetLDBC::load() const noexcept
    {
        if (const bool folders_exist = exists(m_dataset_path / "initial_snapshot") && exists(m_dataset_path / "deletes") && exists(m_dataset_path / "inserts"); !folders_exist)
        {
            _log_system->warning(fmt::format("Dataset path {} does the follow LDBC SNB directory structure", m_dataset_path.string()));
            return;
        }

        static const auto initial_static_path  = m_dataset_path / "initial_snapshot" / "static";
        static const auto initial_dynamic_path = m_dataset_path / "initial_snapshot" / "dynamic";

        load_dataset_static(initial_static_path);
        load_dataset_dynamic(initial_dynamic_path);
    }

    inline void CDatasetLDBC::load_vertex_file(const std::string_view file_name, const rapidcsv::Document & fd) const noexcept
    {
        const auto rows          = fd.GetRowCount();
        const auto col_names     = fd.GetColumnNames();
        const int32_t id_col_idx = fd.GetColumnIdx("id");
        const int32_t type_idx   = fd.GetColumnIdx("type");

        std::vector<uint32_t> prop_indices = {};
        prop_indices.reserve(fd.GetColumnCount());

        for (int32_t i = 0; i < static_cast<int32_t>(fd.GetColumnCount()); i++)
        {
            if (i != id_col_idx && i != type_idx)
                prop_indices.emplace_back(i);
        }

        std::vector<std::pair<std::string_view, std::string_view>> props;
        for (size_t row = 0; row < rows; row++)
        {
            const auto row_data = fd.GetRow<std::string>(row);
            const uint32_t id   = std::stol(row_data[id_col_idx]);
            const auto type     = type_idx == -1 ? file_name : row_data[type_idx];

            props.reserve(prop_indices.size());

            for (const auto & prop_idx : prop_indices)
                props.emplace_back(fd.GetColumnName(prop_idx), row_data[prop_idx]);

            (*m_graph)->add_vertex(ILPGModel::SNodeID {id, type}, props);
            props.clear();
        }
    }

    inline void CDatasetLDBC::load_edge_file(std::string_view file_name, const rapidcsv::Document & fd) const noexcept
    {
        const auto rows          = fd.GetRowCount();
        const auto col_names     = fd.GetColumnNames();
        const int32_t id_col_idx = fd.GetColumnIdx("id");
        const int32_t type_idx   = fd.GetColumnIdx("type");

        std::vector<uint32_t> prop_indices = {};
        prop_indices.reserve(fd.GetColumnCount());

        for (int32_t i = 0; i < static_cast<int32_t>(fd.GetColumnCount()); i++)
        {
            if (i != id_col_idx && i != type_idx)
                prop_indices.emplace_back(i);
        }

        std::vector<std::pair<std::string_view, std::string_view>> props;
        for (size_t row = 0; row < rows; row++)
        {
            const auto row_data = fd.GetRow<std::string>(row);
            const uint32_t id   = std::stol(row_data[id_col_idx]);
            const auto type     = type_idx == -1 ? file_name : row_data[type_idx];

            props.reserve(prop_indices.size());

            for (const auto & prop_idx : prop_indices)
                props.emplace_back(fd.GetColumnName(prop_idx), row_data[prop_idx]);

            (*m_graph)->add_vertex(ILPGModel::SNodeID {id, type}, props);
            props.clear();
        }
    }

    inline void CDatasetLDBC::load_dataset_static(const std::filesystem::path & path) const noexcept
    {
        const auto entries             = std::filesystem::recursive_directory_iterator(path);
        static const auto label_params = rapidcsv::LabelParams();
        static const auto sep_params   = rapidcsv::SeparatorParams('|');
        rapidcsv::Document csv, place;

        //~ Process Vertex Files
        for (const auto & entry : entries)
        {
            if (!std::filesystem::is_regular_file(entry) || !entry.path().extension().compare("csv"))
                continue;

            csv = rapidcsv::Document(entry.path().string(), label_params, sep_params);
            load_vertex_file(entry.path().string(), csv);
        }

        //~ Process Edge Files
        for (const auto & entry : entries)
        {
            if (!std::filesystem::is_regular_file(entry) || !entry.path().extension().compare("csv"))
                return;

            csv = rapidcsv::Document(entry.path().string(), label_params, sep_params);
            load_edge_file(entry.path().string(), csv);
        }
    }

    inline void CDatasetLDBC::load_dataset_dynamic([[maybe_unused]] const std::filesystem::path & path) const noexcept
    {
    }

} // namespace graphquery::database::storage
