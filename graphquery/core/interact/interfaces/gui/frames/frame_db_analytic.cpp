#include "frame_db_analytic.h"

#include "frame_db_query.h"
#include "db/system.h"

#include <algorithm>

graphquery::interact::CFrameDBAnalytic::
CFrameDBAnalytic(const bool & is_db_loaded,
                 const bool & is_graph_loaded,
                 std::shared_ptr<database::storage::ILPGModel *> graph,
                 std::shared_ptr<std::vector<database::utils::SResult<double>>> results,
                 const std::unordered_map<std::string, std::unique_ptr<database::analytic::IGraphAlgorithm *>> & algorithms):
    m_algorithm_choice(0), m_is_db_loaded(is_db_loaded), m_is_graph_loaded(is_graph_loaded), m_graph(std::move(graph)), m_results(std::move(results)), m_algorithms(algorithms)
{
}

void
graphquery::interact::CFrameDBAnalytic::render_frame() noexcept
{
    if (m_is_db_loaded && m_is_graph_loaded)
    {
        if (ImGui::Begin("Analytic"))
        {
            render_analytic_opt();
            render_analytic_control();
        }
    }
}

void
graphquery::interact::CFrameDBAnalytic::render_analytic_opt() noexcept
{
    if (ImGui::BeginChild("#choose_algorithm", {0, ImGui::GetWindowHeight() - 100}))
    {
        ImGui::SeparatorText("Choose an Algorithm");
        ImGui::Dummy(ImVec2(0.0f, 20.0f));
        std::string algorithms = {};

        ImGui::TextUnformatted("Execute: ");
        ImGui::SameLine();
        std::for_each(m_algorithms.begin(), m_algorithms.end(), [&algorithms](const auto & algorithm) -> void { algorithms += fmt::format("{}{}", algorithm.first, '\0'); });
        algorithms += fmt::format("\0");

        ImGui::Combo("##", &m_algorithm_choice, algorithms.c_str());

        ImGui::Dummy(ImVec2(0.0f, 20.0f));
        ImGui::SeparatorText("Results");
        ImGui::Dummy(ImVec2(0.0f, 20.0f));
        render_results_table();
        ImGui::EndChild();
    }
}

void
graphquery::interact::CFrameDBAnalytic::render_results_table() noexcept
{
    static constexpr uint8_t columns       = 2;
    static constexpr ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("#result_table", columns, flags, {0, ImGui::GetWindowHeight() / 2}))
    {
        ImGui::TableSetupColumn("name");
        ImGui::TableSetupColumn("return");
        ImGui::TableHeadersRow();
        std::for_each(m_results->begin(),
                      m_results->end(),
                      [](const database::utils::SResult<double> & res) -> void
                      {
                          ImGui::TableNextRow();
                          ImGui::TableSetColumnIndex(0);
                          ImGui::TextUnformatted(res.get_name().c_str());
                          ImGui::TableSetColumnIndex(1);

                          if (res.processed())
                              ImGui::Text("%.15f", res.get_resultant());
                          else
                              ImGui::TextUnformatted("Processing..");
                      });
        ImGui::EndTable();
    }
}

void
graphquery::interact::CFrameDBAnalytic::render_analytic_control() noexcept
{
    if (ImGui::BeginChild("#run_and_refresh"))
    {
        if (ImGui::Button("Run Algorithm"))
        {
            const auto algorithm = std::next(m_algorithms.begin(), m_algorithm_choice);
            database::_db_analytic->process_algorithm(algorithm->first);
        }
        ImGui::SameLine();
        if (ImGui::Button("Refresh Libraries"))
        {
            database::_db_analytic->load_libraries();
        }
        ImGui::EndChild();
    }
}
