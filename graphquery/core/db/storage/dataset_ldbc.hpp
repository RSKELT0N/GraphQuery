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
#include <unordered_map>

namespace graphquery::database::storage
{
    class CDatasetLDBC final : public CDataset
    {
      public:
        ~CDatasetLDBC() override;
        explicit CDatasetLDBC(std::shared_ptr<ILPGModel *> _graph): CDataset(std::move(_graph)) { define_column_edgelabel_map(); }
        explicit CDatasetLDBC(std::shared_ptr<ILPGModel *> _graph, std::filesystem::path & _path): CDataset(std::move(_graph), _path) { define_column_edgelabel_map(); }

        CDatasetLDBC(const CDatasetLDBC &)                 = delete;
        CDatasetLDBC(CDatasetLDBC &&) noexcept             = delete;
        CDatasetLDBC & operator=(const CDatasetLDBC &)     = delete;
        CDatasetLDBC & operator=(CDatasetLDBC &&) noexcept = delete;

        void load() const noexcept override;

      private:
        void define_column_edgelabel_map() noexcept;
        void load_dataset_static(const std::filesystem::path & path) const noexcept;
        void load_dataset_dynamic(const std::filesystem::path & path) const noexcept;

        std::unordered_map<std::string, std::string> m_column_to_edgelabel_map; //~ Column name is to edge label. Based on the LDBC SNB datasets.
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

    inline void CDatasetLDBC::load_dataset_static(const std::filesystem::path & path) const noexcept
    {
        const auto entries = std::filesystem::recursive_directory_iterator(path);
        rapidcsv::Document csv;
        auto label_params = rapidcsv::LabelParams();
        auto sep_params   = rapidcsv::SeparatorParams('|');

        for (const auto & entry : entries)
        {
            if (!std::filesystem::is_regular_file(entry) || !entry.path().extension().compare("csv"))
                return;

            csv = rapidcsv::Document(entry.path().string(), label_params, sep_params);

            const auto file_name     = entry.path().stem();
            const auto rows          = csv.GetRowCount();
            const auto col_names     = csv.GetColumnNames();
            const int32_t id_col_idx = csv.GetColumnIdx("id");
            const int32_t type_idx   = csv.GetColumnIdx("type");

            std::vector<uint32_t> prop_indices = {};
            std::vector<uint32_t> edge_indices = {};
            prop_indices.reserve(csv.GetColumnCount());
            edge_indices.reserve(csv.GetColumnCount());

            for (int32_t i = 0; i < static_cast<int32_t>(csv.GetColumnCount()); i++)
            {
                if (i != id_col_idx && i != type_idx && !m_column_to_edgelabel_map.contains(csv.GetColumnName(i)))
                    prop_indices.emplace_back(i);

                if (m_column_to_edgelabel_map.contains(csv.GetColumnName(i)))
                    edge_indices.emplace_back(i);
            }

            for (size_t row = 0; row < rows; row++)
            {
                const auto row_data = csv.GetRow<std::string>(row);
                const uint32_t id   = std::stol(row_data[id_col_idx]);
                const auto type     = type_idx == -1 ? file_name.string() : row_data[type_idx];

                std::vector<std::pair<std::string_view, std::string_view>> props;
                props.reserve(prop_indices.size());

                for (const auto & prop_idx : prop_indices)
                    props.emplace_back(csv.GetColumnName(prop_idx), row_data[prop_idx]);

                (*m_graph)->add_vertex(ILPGModel::SNodeID {id, type}, props);
            }

            for (size_t row = 0; row < rows; row++)
            {
                const auto row_data   = csv.GetRow<std::string>(row);
                const uint32_t src_id = std::stol(row_data[id_col_idx]);
                const auto src_type   = type_idx == -1 ? file_name.string() : row_data[type_idx];

                for (const auto & edge_idx : edge_indices)
                {
                    const std::string & dst_id = row_data[edge_idx];

                    if (dst_id.empty())
                        continue;

                    const auto dst_type         = type_idx == -1 ? file_name.string() : csv.GetCell<std::string>(type_idx, dst_id);
                    const std::string edge_type = m_column_to_edgelabel_map.at(csv.GetColumnName(edge_idx));
                    (*m_graph)->add_edge(ILPGModel::SNodeID {src_id, src_type}, ILPGModel::SNodeID {static_cast<uint32_t>(std::stol(dst_id)), dst_type}, edge_type, {});
                }
            }
        }
    }

    inline void CDatasetLDBC::load_dataset_dynamic([[maybe_unused]] const std::filesystem::path & path) const noexcept
    {
    }

    inline void CDatasetLDBC::define_column_edgelabel_map() noexcept
    {
        m_column_to_edgelabel_map = {{"SubclassOfTagClassId", "isSubClassOf"},
                                     {"", "hasType"},
                                     {"", "hasInterest"},
                                     {"LocationPlaceID", "isLocatedIn"},
                                     {"LocationCityId", "isLocatedIn"},
                                     {"LocationCountryId", "isLocatedIn"},
                                     {"", "studyAt"},
                                     {"", "workAt"},
                                     {"", "knows"},
                                     {"", "hasModerator"},
                                     {"CreatorPersonId", "hasCreator"},
                                     {"ContainerForumId", "containerOf"},
                                     {"", "hasMember"},
                                     {"TypeTagClassId", "hasTag"},
                                     {"", "replyOf"},
                                     {"", "likes"},
                                     {"PartOfPlaceID", "isPartOf"}};
    }

} // namespace graphquery::database::storage
