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

graphquery::interact::CFrameDBQuery::
CFrameDBQuery(const bool & is_db_loaded,
              const bool & is_graph_loaded,
              std::shared_ptr<database::storage::ILPGModel *> graph,
              std::shared_ptr<std::vector<database::utils::SResult<database::query::CQueryEngine::ResultType>>> result_table):
    m_result_has_changed(false), m_result_select(-1), m_is_excute_query_open(false), m_predefined_choice(0), m_is_db_loaded(is_db_loaded), m_is_graph_loaded(is_graph_loaded), m_graph(std::move(graph)), m_results(std::move(result_table))
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
                ImGui::EndChild();
            }
            ImGui::SameLine();
            if (ImGui::BeginChild("#db_query_result"))
            {
                render_result();
                ImGui::EndChild();
            }
            ImGui::End();
        }
    }
}

void
graphquery::interact::CFrameDBQuery::render_query() noexcept
{
    ImGui::SeparatorText("Query DB (Cypher)");
    ImGui::Dummy(ImVec2(0.0f, 20.0f));

    ImGui::TextUnformatted("Input: ");
    ImGui::SameLine();
    ImGui::InputText("", &this->m_query_str, ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CtrlEnterForNewLine);
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
    ImGui::Combo("##", &m_predefined_choice, predefined_queries);
    ImGui::SameLine();

    if (ImGui::Button("Execute"))
        m_is_excute_query_open = true;
}

void
graphquery::interact::CFrameDBQuery::render_predefined_query_input() noexcept
{
    if (m_is_excute_query_open)
        ImGui::OpenPopup("Enter query input");

    static int in0, in1;

    ImGui::SetNextWindowSize({200, 0});
    if (ImGui::BeginPopupModal("Enter query input", &m_is_excute_query_open, ImGuiWindowFlags_NoResize))
    {
        switch (m_predefined_choice)
        {
        case 0:
        {
            ImGui::Text("Person ID: ");
            ImGui::SameLine();
            ImGui::InputInt("##_pid0", &in0, 0, 0);
            ImGui::Text("Date Threshold: ");
            ImGui::SameLine();
            ImGui::InputInt("##_dt1", &in1, 0, 0);
            ImGui::NewLine();

            if (ImGui::Button("Ok"))
            {
                ImGui::CloseCurrentPopup();
                m_is_excute_query_open = false;
                database::_db_query->interaction_complex_2(in0, in1);
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
    if (ImGui::BeginChild("#query_result_table"))
    {
        ImGui::SeparatorText("Result table");
        ImGui::NewLine();
        static constexpr uint8_t columns       = 3;
        static constexpr ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;

        if (ImGui::BeginTable("#result_table", columns, flags, {0, ImGui::GetWindowHeight() / 2}))
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

                if(res.processed())
                    ImGui::Text("Processed");
                else ImGui::Text("Processing..");

                ImGui::TableSetColumnIndex(2);
                if (res.processed() && ImGui::RadioButton(fmt::format("##option: {}", i).c_str(), &m_result_select, i))
                {
                    m_result_select = i;
                    m_result_has_changed = true;
                }
            }
            ImGui::EndTable();
        }
        ImGui::NewLine();
        render_clear_selection();
        ImGui::EndChild();
    }
}

void
graphquery::interact::CFrameDBQuery::render_result() noexcept
{
    ImGui::SeparatorText("Query Result");

    if(m_result_select == -1)
    {
        ImGui::Text("No query has been selected");
        return;
    }

    if(m_result_has_changed)
    {
        m_result_has_changed = false;
        m_current_result.str("");

        const auto result = m_results->at(m_result_select);
        const auto & result_name = result.get_name();
        auto result_out = result.get_resultant();

        m_current_result << result_name << "\n";
        m_current_result << fmt::format("Query returned ({}) item(s)\n\n", result_out.size()).c_str();

        if(result_out.empty())
            return;

        for(auto & i : result_out)
        {
            for(const auto & [key, value] : i)
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
    if(ImGui::Button("Clear selection"))
    {
        m_result_select = -1;
        m_current_result.str("");
    }
}
