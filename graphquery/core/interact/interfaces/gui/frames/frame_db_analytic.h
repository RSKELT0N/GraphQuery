#pragma once

#include "db/analytic/analytic.h"
#include "db/storage/dbstorage.h"
#include "interact/interfaces/gui/frame.h"

namespace graphquery::interact
{
    class CFrameDBAnalytic : public IFrame
    {
      public:
        CFrameDBAnalytic(const bool & is_db_loaded,
                         const bool & is_graph_loaded,
                         std::shared_ptr<database::storage::ILPGModel *> graph,
                         std::shared_ptr<std::vector<database::utils::SResult<double>>>,
                         const std::unordered_map<std::string, std::shared_ptr<database::analytic::IGraphAlgorithm *>> &);
        ~CFrameDBAnalytic() override = default;

        void render_frame() noexcept override;

    private:
        void render_analytic_opt() noexcept;
        void render_results_table() noexcept;
        void render_analytic_control() noexcept;

        int32_t m_algorithm_choice;
        const bool & m_is_db_loaded;
        const bool & m_is_graph_loaded;
        std::shared_ptr<database::storage::ILPGModel *> m_graph;
        std::shared_ptr<std::vector<database::utils::SResult<double>>> m_results;
        const std::unordered_map<std::string, std::shared_ptr<database::analytic::IGraphAlgorithm *>> & m_algorithms;
    };
} // namespace graphquery::interact
