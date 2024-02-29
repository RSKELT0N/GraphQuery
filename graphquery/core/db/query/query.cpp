#include "query.h"

#include <sstream>
#include <unordered_set>
#include <algorithm>

void
graphquery::database::query::CQueryEngine::process_query([[maybe_unused]] const std::string_view query) const noexcept
{
}

std::vector<std::map<std::string, std::string>>
graphquery::database::query::CQueryEngine::interaction_complex_2(const int64_t _person_id, const uint32_t _max_date) const noexcept
{
    //~ MATCH (:Person {id: $personId })-[:KNOWS]-(friend:Person)
    std::unordered_set<int64_t> _friends = get_graph()->get_edge_dst_vertices(_person_id, "knows", "Person");

    //~ (friend:Person)<-[:HAS_CREATOR]-(message:Message)
    std::vector<storage::ILPGModel::SEdge_t> message_creators =
        get_graph()->get_edges("Message", "hasCreator", [&_friends](const storage::ILPGModel::SEdge_t & edge) -> bool { return _friends.contains(edge.dst); });

    //~ Generating Map of properties
    std::vector<std::map<std::string, std::string>> properties_map;
    properties_map.reserve(message_creators.size());

    uint32_t prop_i = 0;
    for (const storage::ILPGModel::SEdge_t & edge : message_creators)
    {
        auto message_props = get_graph()->get_properties_by_id_map(edge.src);

        //~ WHERE message.creationDate < $maxDate
        if (static_cast<int64_t>(std::stol(message_props.at("creationDate"))) >= _max_date)
            continue;

        message_props["postOrCommendId"]           = std::to_string(edge.src);
        message_props["postOrCommentCreationDate"] = message_props.at("creationDate");

        message_props.erase("creationDate");

        auto friend_props               = get_graph()->get_properties_by_id_map(edge.dst);
        friend_props["personId"]        = std::to_string(edge.dst);
        friend_props["personFirstName"] = friend_props.at("firstName");
        friend_props["personLastName"]  = friend_props.at("lastName");

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
graphquery::database::query::CQueryEngine::interaction_complex_8(const int64_t _person_id) const noexcept
{
    //~ MATCH (start:Person {id: $personId})<-[:HAS_CREATOR]-(:Message)
    std::vector<storage::ILPGModel::SEdge_t> person_comments = get_graph()->get_edges("Message", "hasCreator", _person_id);

    std::unordered_set<int64_t> uniq_messages = {};
    uniq_messages.reserve(person_comments.size());

    std::for_each(person_comments.begin(), person_comments.end(), [&uniq_messages](const storage::ILPGModel::SEdge_t & edge) { uniq_messages.insert(edge.src); });

    //~ (start:Person {id: $personId})<-[:HAS_CREATOR]-(:Message)<-[:REPLY_OF]-(comment:Comment)
    std::vector<storage::ILPGModel::SEdge_t> comments =
        get_graph()->get_edges("Comment", "replyOf", [&uniq_messages](const storage::ILPGModel::SEdge_t & edge) -> bool { return uniq_messages.contains(edge.dst); });

    std::vector<storage::ILPGModel::SEdge_t> comment_creators = {};

    // ~ (comment:Comment)-[:HAS_CREATOR]->(person:Person)
    for (const storage::ILPGModel::SEdge_t & edge : comments)
    {
        auto comment_creator_person = get_graph()->get_edges(edge.src, "hasCreator", "Person");
        comment_creators.insert(comment_creators.begin(), comment_creator_person.begin(), comment_creator_person.end());
    }

    //~ Generating Map of properties
    std::vector<std::map<std::string, std::string>> properties_map;
    properties_map.reserve(comment_creators.size());

    uint32_t prop_i = 0;
    for (const storage::ILPGModel::SEdge_t & edge : comment_creators)
    {
        auto comment_props = get_graph()->get_properties_by_id_map(edge.src);

        comment_props["postOrCommendId"]           = std::to_string(edge.src);
        comment_props["postOrCommentCreationDate"] = comment_props.at("creationDate");
        comment_props["commentContent"]            = comment_props.at("content");

        comment_props.erase("creationDate");
        comment_props.erase("content");

        auto friend_props               = get_graph()->get_properties_by_id_map(edge.dst);
        friend_props["personId"]        = std::to_string(edge.dst);
        friend_props["personFirstName"] = friend_props.at("firstName");
        friend_props["personLastName"]  = friend_props.at("lastName");

        friend_props.erase("firstName");
        friend_props.erase("lastName");

        properties_map.emplace_back();
        properties_map[prop_i].insert(friend_props.begin(), friend_props.end());
        properties_map[prop_i].insert(comment_props.begin(), comment_props.end());
        prop_i++;
    }

    return properties_map;
}

void
graphquery::database::query::CQueryEngine::interaction_update_2(const int64_t _person_id, const int64_t _post_id) const noexcept
{
    static auto time = std::time(nullptr);
    static std::stringstream date;
    date << std::put_time(std::gmtime(&time), "%c");

    get_graph()->add_edge(_person_id, _post_id, "likes", {{"creationDate", date.str()}});
}

void
graphquery::database::query::CQueryEngine::interaction_update_8(const int64_t _src_person_id, const int64_t _dst_person_id) const noexcept
{
    static auto time = std::time(nullptr);
    static std::stringstream date;
    date << std::put_time(std::gmtime(&time), "%c");

    get_graph()->add_edge(_src_person_id, _dst_person_id, "knows", {{"creationDate", date.str()}});
}

void
graphquery::database::query::CQueryEngine::interaction_delete_2(const int64_t _person_id, const int64_t _post_id) const noexcept
{
    get_graph()->rm_edge(_person_id, _post_id, "LIKES");
}

void
graphquery::database::query::CQueryEngine::interaction_delete_8(const int64_t _src_person_id, const int64_t _dst_person_id) const noexcept
{
    get_graph()->rm_edge(_src_person_id, _dst_person_id, "KNOWS");
}

std::vector<std::map<std::string, std::string>>
graphquery::database::query::CQueryEngine::interaction_short_2(const int64_t _person_id) const noexcept
{
    static constexpr size_t message_limit = 10;

    //~ MATCH (:Person {id: $personId})<-[:HAS_CREATOR]-(message)
    auto person_comments = get_graph()->get_edges("Message", "hasCreator", _person_id);

    //~ MATCH (message)-[:REPLY_OF*0..]->(post:Post)
    std::vector<storage::ILPGModel::SEdge_t> posts = {};
    for (size_t i = 0; i < std::min(message_limit, person_comments.size()); i++)
    {
        auto post = get_graph()->get_edges(person_comments[i].src, "replyOf", "Post");
        posts.insert(posts.begin(), post.begin(), post.end());
    }

    //~ (post)-[:HAS_CREATOR]->(person)
    std::vector<storage::ILPGModel::SEdge_t> creator_of_posts = {};
    for (const auto & post : posts)
    {
        auto creator = get_graph()->get_edges(post.dst, "hasCreator", "Person");
        creator_of_posts.insert(creator_of_posts.begin(), creator.begin(), creator.end());
    }

    //~ Generating Map of properties
    std::vector<std::map<std::string, std::string>> properties_map;
    properties_map.reserve(creator_of_posts.size());

    uint32_t prop_i = 0;
    for (const storage::ILPGModel::SEdge_t & edge : posts)
    {
        auto message_props = get_graph()->get_properties_by_id_map(edge.src);

        message_props["messageId"]           = std::to_string(edge.src);
        message_props["messageCreationDate"] = message_props.at("creationDate");

        message_props.erase("creationDate");

        auto post_props      = get_graph()->get_properties_by_id_map(edge.dst);
        post_props["postId"] = std::to_string(edge.dst);

        auto person_props               = get_graph()->get_properties_by_id_map(creator_of_posts[prop_i].dst);
        person_props["personId"]        = std::to_string(creator_of_posts[prop_i].dst);
        person_props["personFirstName"] = person_props.at("firstName");
        person_props["personLastName"]  = person_props.at("lastName");

        person_props.erase("firstName");
        person_props.erase("lastName");

        properties_map.emplace_back();
        properties_map[prop_i].insert(message_props.begin(), message_props.end());
        properties_map[prop_i].insert(post_props.begin(), post_props.end());
        properties_map[prop_i].insert(person_props.begin(), person_props.end());
        prop_i++;
    }

    return properties_map;
}

std::vector<std::map<std::string, std::string>>
graphquery::database::query::CQueryEngine::interaction_short_7(const int64_t _message_id) const noexcept
{
    //~ MATCH (m:Message {id: $messageId })<-[:REPLY_OF]-(c:Comment)
    const auto message_comments = get_graph()->get_edges("Comment", "replyOf", _message_id);

    //~ (c:Comment)-[:HAS_CREATOR]->(p:Person)
    std::vector<storage::ILPGModel::SEdge_t> creator_of_comments = {};

    for (const storage::ILPGModel::SEdge_t & edge : message_comments)
    {
        auto creator = get_graph()->get_edges(edge.src, "hasCreator", "Person");
        creator_of_comments.insert(creator_of_comments.begin(), creator.begin(), creator.end());
    }

    //~ OPTIONAL MATCH (m)-[:HAS_CREATOR]->(a:Person)-[r:KNOWS]-(p)
    auto message_author                                  = get_graph()->get_edges(_message_id, "hasCreator", "Person")[0].dst;
    std::vector<bool> message_auther_know_reply_author_v = {};
    message_auther_know_reply_author_v.resize(creator_of_comments.size());
    for (size_t i = 0; i < creator_of_comments.size(); i++)
    {
        auto knows                            = get_graph()->get_edge(message_author, "knows", creator_of_comments[i].dst);
        message_auther_know_reply_author_v[i] = knows.has_value();
    }

    //~ Generating Map of properties
    std::vector<std::map<std::string, std::string>> properties_map;
    properties_map.reserve(message_comments.size());

    uint32_t prop_i = 0;
    for (const storage::ILPGModel::SEdge_t & edge : creator_of_comments)
    {
        auto comment_props = get_graph()->get_properties_by_id_map(edge.src);

        comment_props["commentId"]           = std::to_string(edge.src);
        comment_props["commentContent"]      = comment_props.at("content");
        comment_props["commentCreationDate"] = comment_props.at("creationDate");

        comment_props.erase("content");
        comment_props.erase("creationDate");

        auto person_props = get_graph()->get_properties_by_id_map(edge.dst);

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

    return properties_map;
}

graphquery::database::storage::ILPGModel *
graphquery::database::query::CQueryEngine::get_graph() const noexcept
{
    return *this->m_graph;
}
