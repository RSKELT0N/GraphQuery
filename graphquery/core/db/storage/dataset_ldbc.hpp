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

        void load_vertex_file(std::string_view file_name, const rapidcsv::Document & fd) const noexcept;
        void load_edge_file(std::string_view file_name, const rapidcsv::Document & fd) const noexcept;
        void load_dataset_segment(const std::filesystem::path & path) const noexcept;

        std::unordered_map<std::string, std::pair<int32_t, int32_t>> m_edge_file_idx_pos = {
            {"Organisation_isLocatedIn_Place", {0, 1}},
            {"Place_isPartOf_Place", {0, 1}},
            {"TagClass_isSubclassOf_TagClass", {0, 1}},
            {"Tag_hasType_TagClass", {0, 1}},
            {"Post_isLocatedIn_Country", {1, 2}},
            {"Post_hasTag_Tag", {1, 2}},
            {"Post_hasCreator_Person", {1, 2}},
            {"Person_workAt_Company", {1, 2}},
            {"Person_studyAt_University", {1, 2}},
            {"Person_likes_Post", {1, 2}},
            {"Person_likes_Comment", {1, 2}},
            {"Person_knows_Person", {1, 2}},
            {"Person_isLocatedIn_City", {1, 2}},
            {"Person_hasInterest_Tag", {1, 2}},
            {"Forum_hasTag_Tag", {1, 2}},
            {"Forum_containerOf_Post", {1, 2}},
            {"Forum_hasMember_Person", {1, 2}},
            {"Forum_hasModerator_Person", {1, 2}},
            {"Comment_replyOf_Post", {1, 2}},
            {"Comment_replyOf_Comment", {1, 2}},
            {"Comment_isLocatedIn_Country", {1, 2}},
            {"Comment_hasTag_Tag", {1, 2}},
            {"Comment_hasCreator_Person", {1, 2}},
        };
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

        // load_dataset_segment(initial_static_path);
        load_dataset_segment(initial_dynamic_path);
    }

    inline void CDatasetLDBC::load_vertex_file(const std::string_view file_name, const rapidcsv::Document & fd) const noexcept
    {
        const auto rows          = fd.GetRowCount();
        const auto col_names     = fd.GetColumnNames();
        const int32_t id_col_idx = fd.GetColumnIdx("id");
        const int32_t type_idx   = fd.GetColumnIdx("type");

        std::vector<uint32_t> prop_indices = {};
        prop_indices.reserve(fd.GetColumnCount());

        std::vector<std::string_view> labels;
        labels.reserve(2);
        labels.emplace_back(file_name);

        std::string type       = {};
        const bool type_exists = type_idx != -1;

        for (int32_t i = 0; i < static_cast<int32_t>(fd.GetColumnCount()); i++)
        {
            if (i != id_col_idx && i != type_idx)
                prop_indices.emplace_back(i);
        }

        std::vector<ILPGModel::SProperty_t> props;
        for (size_t row = 0; row < rows; row++)
        {
            const auto row_data = fd.GetRow<std::string>(row);
            const int64_t id    = std::stoll(row_data[id_col_idx]);

            if (type_exists)
            {
                type = row_data[type_idx];
                labels.emplace_back(type);
            }

            for (const auto & prop_idx : prop_indices)
                props.emplace_back(fd.GetColumnName(prop_idx), row_data[prop_idx]);

            (*m_graph)->add_vertex(ILPGModel::SNodeID {id, labels}, props);

            props.clear();

            if (type_exists)
                labels.pop_back();
        }
    }

    inline void CDatasetLDBC::load_edge_file(const std::string_view file_name, const rapidcsv::Document & fd) const noexcept
    {
        const std::vector<std::string> edge_parts = utils::split(file_name, '_');
        const std::string & src_label             = edge_parts[0];
        const std::string & edge_label            = edge_parts[1];
        const std::string & dst_label             = edge_parts[2];

        const auto edge_idx_map = m_edge_file_idx_pos.at(file_name.data());

        const auto rows      = fd.GetRowCount();
        const auto col_names = fd.GetColumnNames();

        std::vector<uint32_t> prop_indices = {};
        prop_indices.reserve(fd.GetColumnCount());

        for (int32_t i = 0; i < static_cast<int32_t>(fd.GetColumnCount()); i++)
        {
            if (i != edge_idx_map.first && i != edge_idx_map.second)
                prop_indices.emplace_back(i);
        }

        std::vector<ILPGModel::SProperty_t> props;
        for (size_t row = 0; row < rows; row++)
        {
            const auto row_data   = fd.GetRow<std::string>(row);
            const int64_t src_id  = std::stoll(row_data[edge_idx_map.first]);
            const int64_t dst_id  = std::stoll(row_data[edge_idx_map.second]);

            for (const auto & prop_idx : prop_indices)
                props.emplace_back(col_names[prop_idx], row_data[prop_idx]);

            (*m_graph)->add_edge(ILPGModel::SNodeID {src_id, {src_label}}, ILPGModel::SNodeID {dst_id, {dst_label}}, edge_label, props);
            props.clear();
        }
    }

    inline void CDatasetLDBC::load_dataset_segment(const std::filesystem::path & path) const noexcept
    {
        static const auto label_params = rapidcsv::LabelParams();
        static const auto sep_params   = rapidcsv::SeparatorParams('|');
        rapidcsv::Document csv;

        //~ Process Vertex Files
        for (const auto & entry : std::filesystem::recursive_directory_iterator(path / "vertices"))
        {
            if (!is_regular_file(entry) || !entry.path().extension().compare("csv"))
                return;

            csv = rapidcsv::Document(entry.path().string(), label_params, sep_params);
            load_vertex_file(entry.path().stem().string(), csv);
        }

        // ~ Process Edge Files
        for (const auto & entry : std::filesystem::recursive_directory_iterator(path / "edges"))
        {
            if (!is_regular_file(entry) || !entry.path().extension().compare("csv"))
                return;

            csv = rapidcsv::Document(entry.path().string(), label_params, sep_params);
            load_edge_file(entry.path().stem().string(), csv);
        }
    }

} // namespace graphquery::database::storage
