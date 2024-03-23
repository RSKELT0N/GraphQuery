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
#include "libcsv-parser/include/csv.hpp"

#include <filesystem>

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

        void load_vertex_file(std::string_view file_name, csv::CSVReader & fd) const noexcept;
        void load_edge_file(std::string_view file_name, csv::CSVReader & fd) const noexcept;
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

        _disable_sync_();
        load_dataset_segment(initial_static_path);
        load_dataset_segment(initial_dynamic_path);
        _enable_sync_();
    }

    inline void CDatasetLDBC::load_vertex_file([[maybe_unused]] const std::string_view file_name, [[maybe_unused]] csv::CSVReader & fd) const noexcept
    {
        _log_system->info(fmt::format("Loading vertex file {} into graph", file_name));
        const auto col_names     = fd.get_col_names();
        const int32_t id_col_idx = fd.index_of("id");
        const int32_t type_idx   = fd.index_of("type");

        std::vector<uint32_t> prop_indices = {};
        prop_indices.reserve(col_names.size());

        std::string type       = {};
        const bool type_exists = type_idx != -1;

        for (int32_t i = 0; i < static_cast<int32_t>(col_names.size()); i++)
        {
            if (i != id_col_idx && i != type_idx)
                prop_indices.emplace_back(i);
        }

        std::vector<ILPGModel::SProperty_t> props;
        std::vector<std::string_view> labels;
        for (const auto & row : fd)
        {
            labels.emplace_back(file_name);
            auto id = row[id_col_idx].get<int64_t>();

            if (type_exists)
            {
                type = row[type_idx].get<std::string>();
                labels.emplace_back(type);
            }

            if (file_name.compare("Post") == 0 || file_name.compare("Comment") == 0)
                labels.emplace_back("Message");

            for (const auto & prop_idx : prop_indices)
                props.emplace_back(col_names[prop_idx], row[prop_idx].get<>());

            (*m_graph)->add_vertex(id, labels, props);

            props.clear();
            labels.clear();
        }
    }

    inline void CDatasetLDBC::load_edge_file([[maybe_unused]] const std::string_view file_name, [[maybe_unused]] csv::CSVReader & fd) const noexcept
    {
        _log_system->info(fmt::format("Loading edge file {} into graph", file_name));
        const std::vector<std::string> edge_parts = utils::split(file_name, '_');
        const std::string & edge_label            = edge_parts[1];
        const bool undirected                     = edge_label == "knows";

        const auto edge_idx_map = m_edge_file_idx_pos.at(file_name.data());
        const auto col_names    = fd.get_col_names();

        std::vector<uint32_t> prop_indices = {};
        prop_indices.reserve(col_names.size());

        for (int32_t i = 0; i < static_cast<int32_t>(col_names.size()); i++)
        {
            if (i != edge_idx_map.first && i != edge_idx_map.second)
                prop_indices.emplace_back(i);
        }

        std::vector<ILPGModel::SProperty_t> props;
        for (const auto & row : fd)
        {
            const auto src_id = row[edge_idx_map.first].get<int64_t>();
            const auto dst_id = row[edge_idx_map.second].get<int64_t>();

            for (const auto & prop_idx : prop_indices)
                props.emplace_back(col_names[prop_idx], row[prop_idx].get<>());

            (*m_graph)->add_edge(src_id, dst_id, edge_label, props, undirected);

            props.clear();
        }
    }

    inline void CDatasetLDBC::load_dataset_segment(const std::filesystem::path & path) const noexcept
    {
        //~ Process Vertex Files
        for (const auto & entry : std::filesystem::recursive_directory_iterator(path / "vertices"))
        {
            if (!is_regular_file(entry) || !entry.path().extension().compare("csv"))
                return;

            csv::CSVReader csv(entry.path().string());
            load_vertex_file(entry.path().stem().string(), csv);
        }

        // ~ Process Edge Files
        for (const auto & entry : std::filesystem::recursive_directory_iterator(path / "edges"))
        {
            if (!is_regular_file(entry) || !entry.path().extension().compare("csv"))
                return;

            csv::CSVReader csv(entry.path().string());
            load_edge_file(entry.path().stem().string(), csv);
        }
    }

} // namespace graphquery::database::storage
