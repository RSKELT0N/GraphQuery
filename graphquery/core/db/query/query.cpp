#include "query.h"

#include <iomanip>
#include <sstream>
#include <unordered_set>

void
graphquery::database::query::CQueryEngine::process_query([[maybe_unused]] const std::string_view query) const noexcept
{
}

void
graphquery::database::query::CQueryEngine::interaction_complex_2(const uint64_t _person_id, [[maybe_unused]] uint64_t _max_date) const noexcept
{
    //~ MATCH (:Person {id: $personId })-[:KNOWS]-(friend:Person)
    std::unordered_set<uint64_t> _friends = get_graph()->get_edge_dst_vertices(_person_id, "KNOWS", "Person");

    //~ (friend:Person)<-[:HAS_CREATOR]-(message:Message)
    std::vector<storage::ILPGModel::SEdge_t> message_creators = get_graph()->get_edges("Message",
                                                                                       [&_friends](const storage::ILPGModel::SEdge_t & edge) -> bool
                                                                                       {
                                                                                           if (!_friends.contains(edge.dst))
                                                                                               return false;

                                                                                           return true;
                                                                                       });


    //~ WHERE message.creationDate < $maxDate
}

void
graphquery::database::query::CQueryEngine::interation_complex_8([[maybe_unused]] uint64_t _person_id) const noexcept
{
    //~ MATCH (start:Person {id: $personId})<-[:HAS_CREATOR]-(:Message)
     std::vector<storage::ILPGModel::SEdge_t> person_comments = get_graph()->get_edges("Message", "HAS_CREATOR", [&_person_id](const storage::ILPGModel::SEdge_t & edge) -> bool
     {
         return edge.dst == _person_id;
     });

    std::unordered_set<uint64_t> uniq_messages = {};
    uniq_messages.reserve(person_comments.size());

    std::for_each(person_comments.begin(), person_comments.end(), [&uniq_messages](const storage::ILPGModel::SEdge_t & edge)
    {
       uniq_messages.emplace(edge.src);
    });

    //~ (start:Person {id: $personId})<-[:HAS_CREATOR]-(:Message)<-[:REPLY_OF]-(comment:Comment)
    std::vector<storage::ILPGModel::SEdge_t> comments = get_graph()->get_edges("Comments", "REPLY_OF", [&uniq_messages](const storage::ILPGModel::SEdge_t & edge) -> bool
    {
        return uniq_messages.contains(edge.dst);
    });

    //~ (comment:Comment)-[:HAS_CREATOR]->(person:Person)


}

void
graphquery::database::query::CQueryEngine::interation_update_2(const uint64_t _person_id, const uint64_t _post_id) const noexcept
{
    static auto time = std::time(nullptr);
    static std::stringstream date;
    date << std::put_time(std::gmtime(&time), "%c");

    get_graph()->add_edge(_person_id, _post_id, "LIKES", {{"creationDate", date.str()}});
}

void
graphquery::database::query::CQueryEngine::interation_update_8(const uint64_t _src_person_id, const uint64_t _dst_person_id) const noexcept
{
    static auto time = std::time(nullptr);
    static std::stringstream date;
    date << std::put_time(std::gmtime(&time), "%c");

    get_graph()->add_edge(_src_person_id, _dst_person_id, "KNOWS", {{"creationDate", date.str()}});
}

void
graphquery::database::query::CQueryEngine::interation_delete_2(const uint64_t _person_id, const uint64_t _post_id) const noexcept
{
    get_graph()->rm_edge(_person_id, _post_id, "LIKES");
}

void
graphquery::database::query::CQueryEngine::interation_delete_8(const uint64_t _src_person_id, const uint64_t _dst_person_id) const noexcept
{
    get_graph()->rm_edge(_src_person_id, _dst_person_id, "KNOWS");
}

void
graphquery::database::query::CQueryEngine::interation_short_2([[maybe_unused]] uint64_t _person_id) const noexcept
{
}

void
graphquery::database::query::CQueryEngine::interation_short_7([[maybe_unused]] uint64_t _message_id) const noexcept
{
}

graphquery::database::storage::ILPGModel *
graphquery::database::query::CQueryEngine::get_graph() const noexcept
{
    return *this->m_graph;
}
