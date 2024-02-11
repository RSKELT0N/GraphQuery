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
#include <utility>

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
            if (!std::filesystem::is_regular_file(entry))
                return;

            csv = rapidcsv::Document(entry.path().string(), label_params, sep_params);

            const auto file_name  = entry.path().stem();
            const auto rows       = csv.GetRowCount();
            const auto col_names  = csv.GetColumnNames();
            const auto id_col_idx = csv.GetColumnIdx("id");
            const auto type_idx   = csv.GetColumnIdx("type");

            for (size_t row = 0; row < rows; row++)
            {
                const auto row_data = csv.GetRow<std::string>(row);
                const uint32_t id   = std::stol(row_data[id_col_idx]);
                const auto type     = type_idx == -1 ? file_name.string() : row_data[type_idx];

                (*m_graph)->add_vertex(ILPGModel::SNodeID {id, type}, {});
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
