#pragma once

#include "db/analytic/algorithm.h"
#include "db/query/query.h"
#include "db/storage/dbstorage.h"
#include "interact/interfaces/gui/frame.h"

namespace graphquery::interact
{
    class CFrameDBQuery : public IFrame
    {
      public:
        CFrameDBQuery(const bool & is_db_loaded,
                      const bool & is_graph_loaded,
                      std::shared_ptr<database::storage::ILPGModel *> graph,
                      std::shared_ptr<std::vector<database::utils::SResult<database::query::CQueryEngine::ResultType>>>,
                      const std::unordered_map<std::string, std::shared_ptr<database::analytic::IGraphAlgorithm *>> &);
        ~CFrameDBQuery() override = default;

        void render_frame() noexcept override;
        void render_query() noexcept;
        void render_predefined_queries() noexcept;
        void render_predefined_query_input() noexcept;
        void render_result_table() noexcept;
        void render_footer() noexcept;
        void render_result() noexcept;
        void render_clear_selection() noexcept;
        void render_analytic_after_querying() noexcept;

      private:
        int32_t m_algorithm_choice;
        std::stringstream m_current_result;
        bool m_result_has_changed;
        int32_t m_result_select;
        bool m_is_excute_query_open;
        int32_t m_predefined_choice;
        std::string m_query_str;
        const bool & m_is_db_loaded;
        const bool & m_is_graph_loaded;
        std::shared_ptr<database::storage::ILPGModel *> m_graph;
        std::shared_ptr<std::vector<database::utils::SResult<database::query::CQueryEngine::ResultType>>> m_results;
        const std::unordered_map<std::string, std::shared_ptr<database::analytic::IGraphAlgorithm *>> & m_algorithms;
    };
} // namespace graphquery::interact
