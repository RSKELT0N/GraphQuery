#include "query.h"

#include "db/system.h"
#include "db/utils/lib.h"

#include <sstream>
#include <unordered_set>
#include <algorithm>

namespace
{
    graphquery::database::query::CQueryEngine::ResultType
    _interaction_complex_2(graphquery::database::storage::ILPGModel * graph, const graphquery::database::storage::Id_t _person_id, const int64_t _max_date) noexcept
    {
        std::unordered_set<graphquery::database::storage::Id_t> _friends;
        std::vector<graphquery::database::storage::ILPGModel::SEdge_t> message_creators;
        std::vector<graphquery::database::storage::ILPGModel::SEdge_t> res;

#pragma omp declare reduction(merge : std::vector<graphquery::database::storage::ILPGModel::SEdge_t> : omp_out.insert(omp_out.end(), omp_in.begin(), omp_in.end())) initializer(omp_priv = std::vector<graphquery::database::storage::ILPGModel::SEdge_t>())
#pragma omp parallel default(none) shared(graph, _person_id, _friends, message_creators, res)
        {
#pragma omp sections
            {
#pragma omp section
                {
                    //~ MATCH (:Person {id: $personId })-[:KNOWS]-(friend:Person)
                    _friends = graph->get_edge_dst_vertices(_person_id, "knows", "Person");
                }

#pragma omp section
                {
                    //~ (friend:Person)<-[:HAS_CREATOR]-(message:Message)
                    message_creators = graph->get_edges("Message", "hasCreator");
                }
            }
            res.reserve(message_creators.size());
#pragma omp for reduction(merge : res)
            for (size_t i = 0; i < message_creators.size(); i++)
            {
                if (_friends.contains(message_creators[i].dst))
                    res.emplace_back(message_creators[i]);
            }
        }

        res.shrink_to_fit();

        //~ Generating Map of properties
        std::vector<std::map<std::string, std::string>> properties_map;
        properties_map
            .
            reserve(res
                .
                size()
                );

        uint32_t prop_i = 0;
        for
        (

            const graphquery::database::storage::ILPGModel::SEdge_t & edge :
            res

        )
        {
            auto message_props   = graph->get_properties_by_id_map(edge.src);
            auto friend_props    = graph->get_properties_by_id_map(edge.dst);
            auto w_message_props = std::unordered_map<std::string, std::string>();
            auto w_friend_props  = std::unordered_map<std::string, std::string>();

            //~ WHERE message.creationDate < $maxDate
            if (std::stol(message_props.at("creationDate")) >= _max_date)
                continue;

            w_message_props["postOrCommendId"]           = std::to_string(edge.src);
            w_message_props["postOrCommentCreationDate"] = message_props.at("creationDate");

            w_friend_props["personId"]        = std::to_string(edge.dst);
            w_friend_props["personFirstName"] = friend_props.at("firstName");
            w_friend_props["personLastName"]  = friend_props.at("lastName");

            properties_map.emplace_back();
            properties_map[prop_i].insert(w_friend_props.begin(), w_friend_props.end());
            properties_map[prop_i].insert(w_message_props.begin(), w_message_props.end());
            prop_i++;
        }

        return {res, properties_map};
    }

    graphquery::database::query::CQueryEngine::ResultType
    _interaction_complex_8(graphquery::database::storage::ILPGModel * graph, const graphquery::database::storage::Id_t _person_id) noexcept
    {
        //~ MATCH (start:Person {id: $personId})<-[:HAS_CREATOR]-(:Message)
        std::vector<graphquery::database::storage::ILPGModel::SEdge_t> person_comments = graph->get_edges("Message", "hasCreator", _person_id);

        std::unordered_set<int64_t> uniq_messages = {};
        uniq_messages.reserve(person_comments.size());

        std::for_each(person_comments.begin(), person_comments.end(), [&uniq_messages](const graphquery::database::storage::ILPGModel::SEdge_t & edge) { uniq_messages.insert(edge.src); });

        //~ (start:Person {id: $personId})<-[:HAS_CREATOR]-(:Message)<-[:REPLY_OF]-(comment:Comment)
        std::vector<graphquery::database::storage::ILPGModel::SEdge_t> comments =
            graph->get_edges("Comment", "replyOf", [&uniq_messages](const graphquery::database::storage::ILPGModel::SEdge_t & edge) -> bool { return uniq_messages.contains(edge.dst); });

        std::vector<graphquery::database::storage::ILPGModel::SEdge_t> comment_creators = {};

        // ~ (comment:Comment)-[:HAS_CREATOR]->(person:Person)
        for (const graphquery::database::storage::ILPGModel::SEdge_t & edge : comments)
        {
            auto comment_creator_person = graph->get_edges(edge.src, "hasCreator", "Person");
            comment_creators.insert(comment_creators.begin(), comment_creator_person.begin(), comment_creator_person.end());
        }

        //~ Generating Map of properties
        std::vector<std::map<std::string, std::string>> properties_map;
        properties_map.reserve(comment_creators.size());

        uint32_t prop_i = 0;
        for (const graphquery::database::storage::ILPGModel::SEdge_t & edge : comment_creators)
        {
            auto comment_props   = graph->get_properties_by_id_map(edge.src);
            auto friend_props    = graph->get_properties_by_id_map(edge.dst);
            auto w_comment_props = std::unordered_map<std::string, std::string>();
            auto w_friend_props  = std::unordered_map<std::string, std::string>();

            w_comment_props["postOrCommendId"]           = std::to_string(edge.src);
            w_comment_props["postOrCommentCreationDate"] = comment_props.at("creationDate");
            w_comment_props["commentContent"]            = comment_props.at("content");

            w_friend_props["personId"]        = std::to_string(edge.dst);
            w_friend_props["personFirstName"] = friend_props.at("firstName");
            w_friend_props["personLastName"]  = friend_props.at("lastName");

            properties_map.emplace_back();
            properties_map[prop_i].insert(w_comment_props.begin(), w_comment_props.end());
            properties_map[prop_i].insert(w_friend_props.begin(), w_friend_props.end());
            prop_i++;
        }

        return {comment_creators, properties_map};
    }

    void
    _interaction_update_2(graphquery::database::storage::ILPGModel * graph, const graphquery::database::storage::Id_t _person_id, const graphquery::database::storage::Id_t _post_id) noexcept
    {
        static auto time = std::time(nullptr);
        static std::stringstream date;
        date << std::put_time(std::gmtime(&time), "%c");

        graph->add_edge(_person_id, _post_id, "likes", {{"creationDate", date.str()}});
    }

    void
    _interaction_update_8(graphquery::database::storage::ILPGModel * graph,
                          const graphquery::database::storage::Id_t _src_person_id,
                          const graphquery::database::storage::Id_t _dst_person_id) noexcept
    {
        static auto time = std::time(nullptr);
        static std::stringstream date;
        date << std::put_time(std::gmtime(&time), "%c");

        graph->add_edge(_src_person_id, _dst_person_id, "knows", {{"creationDate", date.str()}});
    }

    void
    _interaction_delete_2(graphquery::database::storage::ILPGModel * graph, const graphquery::database::storage::Id_t _person_id, const graphquery::database::storage::Id_t _post_id) noexcept
    {
        graph->rm_edge(_person_id, _post_id, "LIKES");
    }

    void
    _interaction_delete_8(graphquery::database::storage::ILPGModel * graph,
                          const graphquery::database::storage::Id_t _src_person_id,
                          const graphquery::database::storage::Id_t _dst_person_id) noexcept
    {
        graph->rm_edge(_src_person_id, _dst_person_id, "KNOWS");
    }

    graphquery::database::query::CQueryEngine::ResultType
    _interaction_short_2(graphquery::database::storage::ILPGModel * graph, const graphquery::database::storage::Id_t _person_id) noexcept
    {
        constexpr size_t message_limit = 10;

        //~ MATCH (:Person {id: $personId})<-[:HAS_CREATOR]-(message)
        auto person_comments = graph->get_edges("Message", "hasCreator", _person_id);

        //~ MATCH (message)-[:REPLY_OF*0..]->(post:Post)
        std::vector<graphquery::database::storage::ILPGModel::SEdge_t> posts = {};
        for (size_t i = 0; i < std::min(message_limit, person_comments.size()); i++)
        {
            auto post = graph->get_edges(person_comments[i].src, "replyOf", "Post");
            posts.insert(posts.begin(), post.begin(), post.end());
        }

        //~ (post)-[:HAS_CREATOR]->(person)
        std::vector<graphquery::database::storage::ILPGModel::SEdge_t> creator_of_posts = {};
        for (const auto & post : posts)
        {
            auto creator = graph->get_edges(post.dst, "hasCreator", "Person");
            creator_of_posts.insert(creator_of_posts.begin(), creator.begin(), creator.end());
        }

        //~ Generating Map of properties
        std::vector<std::map<std::string, std::string>> properties_map;
        properties_map.reserve(creator_of_posts.size());

        uint32_t prop_i = 0;
        for (const graphquery::database::storage::ILPGModel::SEdge_t & edge : posts)
        {
            auto message_props   = graph->get_properties_by_id_map(edge.src);
            auto post_props      = graph->get_properties_by_id_map(edge.dst);
            auto person_props    = graph->get_properties_by_id_map(creator_of_posts[prop_i].dst);
            auto w_message_props = std::unordered_map<std::string, std::string>();
            auto w_post_props    = std::unordered_map<std::string, std::string>();
            auto w_person_props  = std::unordered_map<std::string, std::string>();

            w_message_props["messageId"]           = std::to_string(edge.src);
            w_message_props["messageCreationDate"] = message_props.at("creationDate");

            post_props["postId"] = std::to_string(edge.dst);

            w_person_props["personId"]        = std::to_string(creator_of_posts[prop_i].dst);
            w_person_props["personFirstName"] = person_props.at("firstName");
            w_person_props["personLastName"]  = person_props.at("lastName");

            properties_map.emplace_back();
            properties_map[prop_i].insert(w_message_props.begin(), w_message_props.end());
            properties_map[prop_i].insert(w_post_props.begin(), w_post_props.end());
            properties_map[prop_i].insert(w_person_props.begin(), w_person_props.end());
            prop_i++;
        }

        return {posts, properties_map};
    }

    graphquery::database::query::CQueryEngine::ResultType
    _interaction_short_7(graphquery::database::storage::ILPGModel * graph, const graphquery::database::storage::Id_t _message_id) noexcept
    {
        //~ MATCH (m:Message {id: $messageId })<-[:REPLY_OF]-(c:Comment)
        const auto message_comments = graph->get_edges("Comment", "replyOf", _message_id);

        //~ (c:Comment)-[:HAS_CREATOR]->(p:Person)
        std::vector<graphquery::database::storage::ILPGModel::SEdge_t> creator_of_comments = {};

        for (const graphquery::database::storage::ILPGModel::SEdge_t & edge : message_comments)
        {
            auto creator = graph->get_edges(edge.src, "hasCreator", "Person");
            creator_of_comments.insert(creator_of_comments.begin(), creator.begin(), creator.end());
        }

        //~ OPTIONAL MATCH (m)-[:HAS_CREATOR]->(a:Person)-[r:KNOWS]-(p)
        auto message_author                                  = graph->get_edges(_message_id, "hasCreator", "Person")[0].dst;
        std::vector<bool> message_auther_know_reply_author_v = {};
        message_auther_know_reply_author_v.resize(creator_of_comments.size());
        for (size_t i = 0; i < creator_of_comments.size(); i++)
        {
            auto knows                            = graph->get_edge(message_author, "knows", creator_of_comments[i].dst);
            message_auther_know_reply_author_v[i] = knows.has_value();
        }

        //~ Generating Map of properties
        std::vector<std::map<std::string, std::string>> properties_map;
        properties_map.reserve(message_comments.size());

        uint32_t prop_i = 0;
        for (const graphquery::database::storage::ILPGModel::SEdge_t & edge : creator_of_comments)
        {
            auto comment_props = graph->get_properties_by_id_map(edge.src);

            comment_props["commentId"]           = std::to_string(edge.src);
            comment_props["commentContent"]      = comment_props.at("content");
            comment_props["commentCreationDate"] = comment_props.at("creationDate");

            comment_props.erase("content");
            comment_props.erase("creationDate");

            auto person_props = graph->get_properties_by_id_map(edge.dst);

            person_props["replyAuthorId"]        = std::to_string(edge.dst);
            person_props["replyAuthorFirstName"] = person_props.at("firstName");
            person_props["replyAuthorLastName"]  = person_props.at("lastName");

            person_props.erase("firstName");
            person_props.erase("lastName");

            properties_map.emplace_back();
            properties_map[prop_i].insert(comment_props.begin(), comment_props.end());
            properties_map[prop_i].insert(person_props.begin(), person_props.end());
            properties_map[prop_i].emplace("replyAuthorKnowsOriginalMessageAuthor", message_auther_know_reply_author_v[prop_i] ? "true" : "false");
            prop_i++;
        }

        return {creator_of_comments, properties_map};
    }
} // namespace

graphquery::database::query::CQueryEngine::
CQueryEngine(std::shared_ptr<storage::ILPGModel *> graph_model):
    m_graph(std::move(graph_model))
{
    this->m_results = std::make_shared<std::vector<utils::SResult<ResultType>>>();
}

std::shared_ptr<std::vector<graphquery::database::utils::SResult<graphquery::database::query::CQueryEngine::ResultType>>>
graphquery::database::query::CQueryEngine::get_result_table() const noexcept
{
    return this->m_results;
}

graphquery::database::storage::ILPGModel *
graphquery::database::query::CQueryEngine::get_graph() const noexcept
{
    return *this->m_graph;
}

void
graphquery::database::query::CQueryEngine::interaction_complex_2(storage::Id_t _person_id, int64_t _max_date) const noexcept
{
    m_results->emplace_back("IC2",
                            [capture0 = *m_graph, _person_id, _max_date]
                            {
                                auto [res, elapsed] = utils::measure<ResultType>(&_interaction_complex_2, capture0, _person_id, _max_date);
                                _log_system->info(fmt::format("Query ({}) executed within {}s", "IC2", elapsed.count()));
                                return res;
                            });
}

void
graphquery::database::query::CQueryEngine::interaction_complex_8(storage::Id_t _person_id) const noexcept
{
    m_results->emplace_back("IC8",
                            [capture0 = *m_graph, _person_id]
                            {
                                auto [res, elapsed] = utils::measure<ResultType>(&_interaction_complex_8, capture0, _person_id);
                                _log_system->info(fmt::format("Query ({}) executed within {}s", "IC8", elapsed.count()));
                                return res;
                            });
}

void
graphquery::database::query::CQueryEngine::interaction_update_2(storage::Id_t _person_id, storage::Id_t _post_id) const noexcept
{
    auto [elapsed] = utils::measure(&_interaction_update_2, *m_graph, _person_id, _post_id);
    _log_system->info(fmt::format("Query ({}) executed within {}s", "IU2", elapsed.count()));
}

void
graphquery::database::query::CQueryEngine::interaction_update_8(storage::Id_t _src_person_id, storage::Id_t _dst_person_id) const noexcept
{
    auto [elapsed] = utils::measure(&_interaction_update_8, *m_graph, _src_person_id, _dst_person_id);
    _log_system->info(fmt::format("Query ({}) executed within {}s", "IU8", elapsed.count()));
}

void
graphquery::database::query::CQueryEngine::interaction_delete_2(storage::Id_t _person_id, storage::Id_t _post_id) const noexcept
{
    auto [elapsed] = utils::measure(&_interaction_delete_2, *m_graph, _person_id, _post_id);
    _log_system->info(fmt::format("Query ({}) executed within {}s", "ID2", elapsed.count()));
}

void
graphquery::database::query::CQueryEngine::interaction_delete_8(storage::Id_t _src_person_id, storage::Id_t _dst_person_id) const noexcept
{
    auto [elapsed] = utils::measure(&_interaction_delete_8, *m_graph, _src_person_id, _dst_person_id);
    _log_system->info(fmt::format("Query ({}) executed within {}s", "ID8", elapsed.count()));
}

void
graphquery::database::query::CQueryEngine::interaction_short_2(storage::Id_t _person_id) const noexcept
{
    m_results->emplace_back("IS2",
                            [capture0 = *m_graph, _person_id]
                            {
                                auto [res, elapsed] = utils::measure<ResultType>(&_interaction_short_2, capture0, _person_id);
                                _log_system->info(fmt::format("Query ({}) executed within {}s", "IS2", elapsed.count()));
                                return res;
                            });
}

void
graphquery::database::query::CQueryEngine::interaction_short_7(storage::Id_t _message_id) const noexcept
{
    m_results->emplace_back("IS7",
                            [capture0 = *m_graph, _message_id]
                            {
                                auto [res, elapsed] = utils::measure<ResultType>(&_interaction_short_7, capture0, _message_id);
                                _log_system->info(fmt::format("Query ({}) executed within {}s", "IS7", elapsed.count()));
                                return res;
                            });
}