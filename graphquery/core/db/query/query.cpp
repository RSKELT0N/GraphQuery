#include "query.h"

#include <iomanip>
#include <sstream>

void
graphquery::database::query::CQueryEngine::process_query(std::string_view query) const noexcept
{
}
void
graphquery::database::query::CQueryEngine::interaction_complex_2(uint64_t _person_id, uint64_t _max_date) const noexcept
{
}
void
graphquery::database::query::CQueryEngine::interation_complex_8(uint64_t _person_id) const noexcept
{
}
void
graphquery::database::query::CQueryEngine::interation_update_2(const uint64_t _person_id, const uint64_t _post_id) const noexcept
{
    static constexpr auto time = std::time(nullptr);
    static std::stringstream date;
    date << std::put_time(std::gmtime(&time), "%c");

    get_graph()->add_edge(_person_id, _post_id, "LIKES", {{"creationDate", date.str()}});
}
void
graphquery::database::query::CQueryEngine::interation_update_8(const uint64_t _src_person_id, const uint64_t _dst_person_id) const noexcept
{
    static constexpr auto time = std::time(nullptr);
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
graphquery::database::query::CQueryEngine::interation_short_2(uint64_t _person_id) const noexcept
{
}
void
graphquery::database::query::CQueryEngine::interation_short_7(uint64_t _message_id) const noexcept
{
}

graphquery::database::storage::ILPGModel *
graphquery::database::query::CQueryEngine::get_graph() const noexcept
{
    return *this->m_graph;
}
