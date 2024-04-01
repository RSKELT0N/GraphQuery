#include "frame_db_query.h"

#include "imgui_stdlib.h"
#include "db/system.h"

#include <algorithm>

namespace
{
    const char * predefined_queries = {"Interaction Complex 2\0"
                                       "Interaction Complex 8\0"
                                       "Interaction Update 2\0"
                                       "Interaction Update 8\0"
                                       "Interaction Delete 2\0"
                                       "Interaction Delete 8\0"
                                       "Interaction Short 2\0"
                                       "Interaction Short 8\0\0"};
}

graphquery::interact::CFrameDBQuery::CFrameDBQuery(const bool & is_db_loaded,
                                                   const bool & is_graph_loaded,
                                                   std::shared_ptr<database::storage::ILPGModel *> graph,
                                                   std::shared_ptr<std::vector<database::utils::SResult<database::query::CQueryEngine::ResultType>>> result_table,
                                                   const std::unordered_map<std::string, std::shared_ptr<database::analytic::IGraphAlgorithm *>> & algorithms):
    m_algorithm_choice(0),
    m_result_has_changed(false), m_result_select(-1), m_is_excute_query_open(false), m_predefined_choice(0), m_is_db_loaded(is_db_loaded), m_is_graph_loaded(is_graph_loaded),
    m_graph(std::move(graph)), m_results(std::move(result_table)), m_algorithms(algorithms)
{
}

void
graphquery::interact::CFrameDBQuery::render_frame() noexcept
{
    if (m_is_db_loaded && m_is_graph_loaded)
    {
        if (ImGui::Begin("Query"))
        {
            if (ImGui::BeginChild("#db_query", {ImGui::GetWindowWidth() / 2, 0}))
            {
                render_query();
                render_predefined_queries();
                render_predefined_query_input();
                render_result_table();
                render_footer();
            }

            ImGui::EndChild();
            ImGui::SameLine();
            if (ImGui::BeginChild("#db_query_result"))
            {
                render_result();
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }
}

void
graphquery::interact::CFrameDBQuery::render_query() noexcept
{
    ImGui::SeparatorText("Query DB (Cypher)");
    ImGui::Dummy(ImVec2(0.0f, 20.0f));

    ImGui::TextUnformatted("Input: ");
    ImGui::SameLine();
    ImGui::InputText("##", &this->m_query_str, ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CtrlEnterForNewLine);
    ImGui::SameLine();

    if (ImGui::Button("Query"))
    {
        // ~ TODO: Parse input into cypher parsing lib.
    }

    ImGui::Dummy(ImVec2(0.0f, 20.0f));
}

void
graphquery::interact::CFrameDBQuery::render_predefined_queries() noexcept
{
    ImGui::SeparatorText("Predefined queries");
    ImGui::Dummy(ImVec2(0.0f, 20.0f));
    ImGui::TextUnformatted("Select: ");
    ImGui::SameLine();
    ImGui::Combo("##choices", &m_predefined_choice, predefined_queries);
    ImGui::SameLine();

    if (ImGui::Button("Execute"))
        m_is_excute_query_open = true;
}

void
graphquery::interact::CFrameDBQuery::render_predefined_query_input() noexcept
{
    if (m_is_excute_query_open)
        ImGui::OpenPopup("Enter query input");

    static int32_t in0, in1;
    static std::string sn0, sn1;

    ImGui::SetNextWindowSize({0, 0});
    if (ImGui::BeginPopupModal("Enter query input", &m_is_excute_query_open))
    {
        switch (m_predefined_choice)
        {
        case 0:
        {
            ImGui::Text("Person ID: ");
            ImGui::SameLine();
            ImGui::InputText("##_pid0", &sn0, 0, nullptr);
            ImGui::Text("Max date: ");
            ImGui::SameLine();
            ImGui::InputText("##_dt1", &sn1, 0, nullptr);
            ImGui::NewLine();

            if (ImGui::Button("Ok"))
            {
                ImGui::CloseCurrentPopup();
                m_is_excute_query_open = false;
                database::_db_query->interaction_complex_2(std::stoll(sn0), std::stol(sn1));
            }
            break;
        }
        case 1:
        {
            ImGui::Text("Person ID: ");
            ImGui::SameLine();
            ImGui::InputInt("##_pid1", &in0, 0, 0);
            ImGui::NewLine();

            if (ImGui::Button("Ok"))
            {
                ImGui::CloseCurrentPopup();
                m_is_excute_query_open = false;
                database::_db_query->interaction_complex_8(in0);
            }
            break;
        }
        case 2:
        {
            ImGui::Text("Person ID: ");
            ImGui::SameLine();
            ImGui::InputInt("##_pid2", &in0, 0, 0);
            ImGui::Text("Post ID: ");
            ImGui::SameLine();
            ImGui::InputInt("##_po0", &in1, 0, 0);
            ImGui::NewLine();

            if (ImGui::Button("Ok"))
            {
                ImGui::CloseCurrentPopup();
                m_is_excute_query_open = false;
                database::_db_query->interaction_update_2(in0, in1);
            }
            break;
        }
        case 3:
        {
            ImGui::Text("Person ID: ");
            ImGui::SameLine();
            ImGui::InputInt("##_pid3", &in0, 0, 0);
            ImGui::Text("Person ID: ");
            ImGui::SameLine();
            ImGui::InputInt("##_pid4", &in1, 0, 0);
            ImGui::NewLine();

            if (ImGui::Button("Ok"))
            {
                ImGui::CloseCurrentPopup();
                m_is_excute_query_open = false;
                database::_db_query->interaction_update_8(in0, in1);
            }
            break;
        }
        case 4:
        {
            ImGui::Text("Person ID: ");
            ImGui::SameLine();
            ImGui::InputInt("##_pid5", &in0, 0, 0);
            ImGui::Text("Post ID: ");
            ImGui::SameLine();
            ImGui::InputInt("##_po1", &in1, 0, 0);
            ImGui::NewLine();

            if (ImGui::Button("Ok"))
            {
                ImGui::CloseCurrentPopup();
                m_is_excute_query_open = false;
                database::_db_query->interaction_delete_2(in0, in1);
            }
            break;
        }
        case 5:
        {
            ImGui::Text("Person ID: ");
            ImGui::SameLine();
            ImGui::InputInt("##_pid6", &in0, 0, 0);
            ImGui::Text("Person ID: ");
            ImGui::SameLine();
            ImGui::InputInt("##_pid7", &in1, 0, 0);
            ImGui::NewLine();

            if (ImGui::Button("Ok"))
            {
                ImGui::CloseCurrentPopup();
                m_is_excute_query_open = false;
                database::_db_query->interaction_delete_8(in0, in1);
            }
            break;
        }
        case 6:
        {
            ImGui::Text("Person ID: ");
            ImGui::SameLine();
            ImGui::InputInt("##_pid8", &in0, 0, 0);
            ImGui::NewLine();

            if (ImGui::Button("Ok"))
            {
                ImGui::CloseCurrentPopup();
                m_is_excute_query_open = false;
                database::_db_query->interaction_short_2(in0);
            }
            break;
        }
        case 7:
        {
            ImGui::Text("Message ID: ");
            ImGui::SameLine();
            ImGui::InputInt("##_mid0", &in0, 0, 0);
            ImGui::NewLine();

            if (ImGui::Button("Ok"))
            {
                ImGui::CloseCurrentPopup();
                m_is_excute_query_open = false;
                database::_db_query->interaction_short_7(in0);
            }
            break;
        }
        default: database::_log_system->warning("Option not recognised");
        }
        ImGui::EndPopup();
    }
}

void
graphquery::interact::CFrameDBQuery::render_result_table() noexcept
{
    ImGui::NewLine();
    ImGui::SeparatorText("Result table");
    ImGui::NewLine();
    static constexpr uint8_t columns       = 3;
    static constexpr ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;

    if (ImGui::BeginTable("#result_table", columns, flags, {0, ImGui::GetWindowHeight() / 3}))
    {
        ImGui::TableSetupColumn("name");
        ImGui::TableSetupColumn("state");
        ImGui::TableSetupColumn("selected");
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < m_results->size(); i++)
        {
            auto res = m_results->at(i);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(res.get_name().c_str());
            ImGui::TableSetColumnIndex(1);

            if (res.processed())
                ImGui::Text("Processed");
            else
                ImGui::Text("Processing..");

            ImGui::TableSetColumnIndex(2);
            if (res.processed() && ImGui::RadioButton(fmt::format("##option: {}", i).c_str(), &m_result_select, i))
            {
                m_result_select      = static_cast<int32_t>(i);
                m_result_has_changed = true;
            }
        }
        ImGui::EndTable();
        }
        ImGui::NewLine();
}

void
graphquery::interact::CFrameDBQuery::render_footer() noexcept
{
    if (ImGui::BeginChild("##clear_selection", {ImGui::GetWindowWidth() / 2, 0}))
    {
        render_clear_selection();
    }
    ImGui::EndChild();
    ImGui::SameLine();
    if (ImGui::BeginChild("##analytic_after_querying"))
    {
        render_analytic_after_querying();
    }
    ImGui::EndChild();
}

void
graphquery::interact::CFrameDBQuery::render_result() noexcept
{
    ImGui::SeparatorText("Query Result");

    if (m_result_select == -1)
    {
        ImGui::Text("No query has been selected");
        return;
    }

    if (m_result_has_changed)
    {
        m_result_has_changed = false;
        m_current_result.str("");

        const auto result        = m_results->at(m_result_select);
        const auto & result_name = result.get_name();
        auto result_out          = result.get_resultant().properties;

        m_current_result << result_name << "\n";
        m_current_result << fmt::format("Query returned ({}) item(s)\n\n", result_out.size()).c_str();

        if (result_out.empty())
            return;

        for (auto & i : result_out)
        {
            for (const auto & [key, value] : i)
            {
                m_current_result << key << " : " << value << "\n";
            }
            m_current_result << "\n";
        }
    }

    ImGui::TextUnformatted(m_current_result.str().c_str());
}

void
graphquery::interact::CFrameDBQuery::render_clear_selection() noexcept
{
    if (ImGui::Button("Clear selection"))
    {
        m_result_select = -1;
        m_current_result.str("");
    }
}

void
graphquery::interact::CFrameDBQuery::render_analytic_after_querying() noexcept
{
    if (m_result_select == -1)
        return;

    std::string algorithms = {};

    std::for_each(m_algorithms.begin(), m_algorithms.end(), [&algorithms](const auto & algorithm) -> void { algorithms += fmt::format("{}{}", algorithm.first, '\0'); });
    algorithms += fmt::format("\0");

    ImGui::Combo("##algorithms", &m_algorithm_choice, algorithms.c_str());

    ImGui::SameLine();
    if (ImGui::Button("Run"))
    {
        const auto algorithm = std::next(m_algorithms.begin(), m_algorithm_choice);
        database::_db_analytic->process_algorithm(m_results->at(m_result_select).get_resultant().edges, algorithm->first);
    }
}