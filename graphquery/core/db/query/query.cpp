#include "query.h"

#include <sstream>
#include <unordered_set>
#include <algorithm>

void
graphquery::database::query::CQueryEngine::process_query([[maybe_unused]] const std::string_view query) const noexcept
{
}

std::vector<std::map<std::string, std::string>>
graphquery::database::query::CQueryEngine::interaction_complex_2(const uint32_t _person_id, [[maybe_unused]] uint32_t _max_date) const noexcept
{
    //~ MATCH (:Person {id: $personId })-[:KNOWS]-(friend:Person)
    std::unordered_set<uint32_t> _friends = get_graph()->get_edge_dst_vertices({_person_id, "Person"}, "KNOWS", "Person");

    //~ (friend:Person)<-[:HAS_CREATOR]-(message:Message)
    std::vector<storage::ILPGModel::SEdge_t> message_creators = get_graph()->get_edges("Message",
                                                                                       [&_friends](const storage::ILPGModel::SEdge_t & edge) -> bool
                                                                                       {
                                                                                           if (!_friends.contains(edge.dst))
                                                                                               return false;

                                                                                           return true;
                                                                                       });

    //~ Generating Map of properties
    std::vector<std::map<std::string, std::string>> properties_map;

    uint32_t prop_i = 0;
    for (const storage::ILPGModel::SEdge_t & edge : message_creators)
    {
        auto message_props = get_graph()->get_properties_by_id_map(edge.src);

        //~ WHERE message.creationDate < $maxDate
        if (static_cast<uint32_t>(std::stol(message_props.at("creationDate"))) >= _max_date)
            continue;

        message_props["postOrCommendId"]           = message_props.at("id");
        message_props["postOrCommentCreationDate"] = message_props.at("creationDate");

        message_props.erase("id");
        message_props.erase("creationDate");

        auto friend_props               = get_graph()->get_properties_by_id_map(edge.dst);
        friend_props["personId"]        = friend_props.at("id");
        friend_props["personFirstName"] = friend_props.at("firstName");
        friend_props["personLastName"]  = friend_props.at("lastName");

        friend_props.erase("id");
        friend_props.erase("firstName");
        friend_props.erase("lastName");

        properties_map.emplace_back();
        properties_map[prop_i].insert(friend_props.begin(), friend_props.end());
        properties_map[prop_i].insert(message_props.begin(), message_props.end());
        prop_i++;
    }

    return properties_map;
}

std::vector<std::map<std::string, std::string>>
graphquery::database::query::CQueryEngine::interation_complex_8(const uint32_t _person_id) const noexcept
{
    //~ MATCH (start:Person {id: $personId})<-[:HAS_CREATOR]-(:Message)
    std::vector<storage::ILPGModel::SEdge_t> person_comments = get_graph()->get_edges("Message", "HAS_CREATOR", [&_person_id](const auto & edge) -> bool { return edge.dst == _person_id; });

    std::unordered_set<uint32_t> uniq_messages = {};
    uniq_messages.reserve(person_comments.size());

    std::for_each(person_comments.begin(), person_comments.end(), [&uniq_messages](const storage::ILPGModel::SEdge_t & edge) { uniq_messages.emplace(edge.src); });

    //~ (start:Person {id: $personId})<-[:HAS_CREATOR]-(:Message)<-[:REPLY_OF]-(comment:Comment)
    std::vector<storage::ILPGModel::SEdge_t> comments =
        get_graph()->get_edges("Comments", "REPLY_OF", [&uniq_messages](const storage::ILPGModel::SEdge_t & edge) -> bool { return uniq_messages.contains(edge.dst); });

    std::vector<storage::ILPGModel::SEdge_t> comment_creators = {};

    // ~ (comment:Comment)-[:HAS_CREATOR]->(person:Person)
    for (const storage::ILPGModel::SEdge_t & edge : comments)
    {
        if (auto comment_creator_person = get_graph()->get_edge(edge.src, "HAS_CREATOR", "Person"); comment_creator_person.has_value())
            comment_creators.emplace_back(comment_creator_person.value());
    }

    //~ Generating Map of properties
    std::vector<std::map<std::string, std::string>> properties_map;

    uint32_t prop_i = 0;
    for (const storage::ILPGModel::SEdge_t & edge : comment_creators)
    {
        auto comment_props = get_graph()->get_properties_by_id_map(edge.src);

        comment_props["postOrCommendId"]           = comment_props.at("id");
        comment_props["postOrCommentCreationDate"] = comment_props.at("creationDate");
        comment_props["commentContent"]            = comment_props.at("content");

        comment_props.erase("id");
        comment_props.erase("creationDate");
        comment_props.erase("content");

        auto friend_props               = get_graph()->get_properties_by_id_map(edge.dst);
        friend_props["personId"]        = friend_props.at("id");
        friend_props["personFirstName"] = friend_props.at("firstName");
        friend_props["personLastName"]  = friend_props.at("lastName");

        friend_props.erase("id");
        friend_props.erase("firstName");
        friend_props.erase("lastName");

        properties_map.emplace_back();
        properties_map[prop_i].insert(friend_props.begin(), friend_props.end());
        properties_map[prop_i].insert(comment_props.begin(), comment_props.end());
        prop_i++;
    }

    return properties_map;
    return {};
}

void
graphquery::database::query::CQueryEngine::interation_update_2([[maybe_unused]] const uint32_t _person_id, [[maybe_unused]] const uint32_t _post_id) const noexcept
{
    static auto time = std::time(nullptr);
    static std::stringstream date;
    date << std::put_time(std::gmtime(&time), "%c");

    get_graph()->add_edge({_person_id, "Person"}, {_post_id, "Post"}, "LIKES", {{"creationDate", date.str()}});
}

void
graphquery::database::query::CQueryEngine::interation_update_8([[maybe_unused]] const uint32_t _src_person_id, [[maybe_unused]] const uint32_t _dst_person_id) const noexcept
{
    static auto time = std::time(nullptr);
    static std::stringstream date;
    date << std::put_time(std::gmtime(&time), "%c");

    get_graph()->add_edge({_src_person_id, "Person"}, {_dst_person_id, "Person"}, "KNOWS", {{"creationDate", date.str()}});
}

void
graphquery::database::query::CQueryEngine::interation_delete_2([[maybe_unused]] const uint32_t _person_id, [[maybe_unused]] const uint32_t _post_id) const noexcept
{
    get_graph()->rm_edge({_person_id, "Person"}, {_post_id, "Post"}, "LIKES");
}

void
graphquery::database::query::CQueryEngine::interation_delete_8([[maybe_unused]] const uint32_t _src_person_id, [[maybe_unused]] const uint32_t _dst_person_id) const noexcept
{
    get_graph()->rm_edge({_src_person_id, "Person"}, {_dst_person_id, "Person"}, "KNOWS");
}

void
graphquery::database::query::CQueryEngine::interation_short_2([[maybe_unused]] const uint32_t _person_id) const noexcept
{
    //~ MATCH (:Person {id: $personId})<-[:HAS_CREATOR]-(message)
    auto person_comments = get_graph()->get_edges("Message", "HAS_CREATOR", [&_person_id](const auto & edge) -> bool { return edge.dst == _person_id; });
    //~ MATCH (message)-[:REPLY_OF*0..]->(post:Post)
    std::vector<storage::ILPGModel::SEdge_t> posts = {};

    for (const auto & edge : person_comments)
    {
        if (auto post = get_graph()->get_edge(edge.src, "REPLY_OF", "Post"); post.has_value())
            posts.emplace_back(post.value());
    }
    //~ (post)-[:HAS_CREATOR]->(person)
    std::vector<storage::ILPGModel::SEdge_t> creator_of_posts = {};

    for (const auto & post : posts)
    {
        if (auto creator = get_graph()->get_edge(post.dst, "HAS_CREATOR", "Person"); creator.has_value())
            creator_of_posts.emplace_back(creator.value());
    }

    //~ Generating Map of properties
}

void
graphquery::database::query::CQueryEngine::interation_short_7([[maybe_unused]] const uint32_t _message_id) const noexcept
{
    //~ MATCH (m:Message {id: $messageId })<-[:REPLY_OF]-(c:Comment)
    auto message_comments = get_graph()->get_edges("Message", "REPLY_OF", [&_message_id](const auto & edge) -> bool { return edge.dst == _message_id; });

    //~ (c:Comment)-[:HAS_CREATOR]->(p:Person)
    std::vector<storage::ILPGModel::SEdge_t> creator_of_comments = {};

    for (const storage::ILPGModel::SEdge_t & edge : message_comments)
    {
        if (auto creator = get_graph()->get_edge(edge.src, "HAS_CREATOR", "Person"); creator.has_value())
            creator_of_comments.emplace_back(creator.value());
    }

    //~ Generating Map of properties
}

graphquery::database::storage::ILPGModel *
graphquery::database::query::CQueryEngine::get_graph() const noexcept
{
    return *this->m_graph;
}
