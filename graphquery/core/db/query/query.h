/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file query.h
 * \brief Query engine for interpret pre-specified
 *        query statements and input cypher parsing
 *        library for non-specified statements.
 ************************************************************/

#pragma once

#include "statement.h"
#include "db/storage/graph_model.h"

#include <functional>
#include <unordered_map>

namespace graphquery::database::query
{
    class CQueryEngine
    {
      public:
        enum class SQuery : uint8_t
        {
            interaction_complex_2,
            interation_complex_8,
            interation_update_2,
            interation_update_8,
            interation_delete_2,
            interation_delete_8,
            interation_short_2,
            interation_short_7,
        };

        CQueryEngine() = default;

        ~CQueryEngine()                                    = delete;
        CQueryEngine(const CQueryEngine &)                 = delete;
        CQueryEngine(CQueryEngine &&) noexcept             = delete;
        CQueryEngine & operator=(const CQueryEngine &)     = delete;
        CQueryEngine & operator=(CQueryEngine &&) noexcept = delete;

        void process_query(SQuery query) const noexcept;
        void process_query(std::string_view query) const noexcept;

      private:
        std::unique_ptr<std::unordered_map<SQuery, std::function<void()>>> m_query_lut;
    };
} // namespace graphquery::database::query
