/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file query.h
 * \brief Query engine for interpret pre-specified
 *        query statements and input cypher parsing
 *        library for non-specified statements.
 *
 *        Pre-specified statements based on LDBC SNB-Interactive
 *        https://github.com/ldbc/ldbc_snb_interactive_v2_impls/tree/main/cypher/queries
 ************************************************************/

#pragma once

#include "db/storage/graph_model.h"
#include "db/utils/result.h"

#include <map>

namespace graphquery::database::query
{
    class CQueryEngine
    {
      public:
        enum class EPredefinedQuery : uint8_t
        {
            InteractionComplex2 = 0,
            InteractionComplex8 = 1,
            InteractionUpdate2  = 2,
            InteractionUpdate8  = 3,
            InteractionDelete2  = 4,
            InteractionDelete8  = 5,
            InteractionShort2   = 6,
            InteractionShort7   = 7,
        };

        typedef std::vector<std::map<std::string, std::string>> ResultType;
        explicit CQueryEngine(std::shared_ptr<storage::ILPGModel *> graph_model);

        ~CQueryEngine()                                    = default;
        CQueryEngine(const CQueryEngine &)                 = delete;
        CQueryEngine(CQueryEngine &&) noexcept             = delete;
        CQueryEngine & operator=(const CQueryEngine &)     = delete;
        CQueryEngine & operator=(CQueryEngine &&) noexcept = delete;

        /****************************************************************
        ** \brief Provided a person ID, return all sorted messages from
        *        that person’s friends, where the creation date of the
        *        message is less than the max date parameter. (limited to 20)
        *
        *  \param _person_id int64_t _person_id - ID of person
        *  \param _max_date uint32_t - max date threshold
        ***************************************************************/
        void interaction_complex_2(int64_t _person_id, int64_t _max_date) const noexcept;

        /****************************************************************
         ** \brief Provided a person ID, find the most recent comments that
         *         are replies to messages of the start person. Only single-hop
         *         replies, and return each person and their reply comment.
         *         (limited to 20)
         *
         *  \param _person_id int64_t - ID of person
         ***************************************************************/
        void interaction_complex_8(int64_t _person_id) const noexcept;

        /****************************************************************
         ** \brief Creates an edge between two nodes (person, post),
         *         with passed parameters ID parameters.
         *
         *  \param _person_id int64_t - ID of person
         *  \param _post_id int64_t - ID of post
         ***************************************************************/
        void interaction_update_2(int64_t _person_id, int64_t _post_id) const noexcept;

        /****************************************************************
        ** \brief Creates an edge between two nodes (person, person),
        *         with passed parameters ID parameters.
        *
        *  \param _src_person_id int64_t - ID of src person
        *  \param _dst_person_id int64_t - ID of dst person
        ***************************************************************/
        void interaction_update_8(int64_t _src_person_id, int64_t _dst_person_id) const noexcept;

        /****************************************************************
         ** \brief Given a person and post ID, delete the "LIKES" relationship
         *         between a specific person and a specific post, and
         *         then return the count of the deleted relationships.
         *
         *  \param _person_id - ID of person
         *  \param _post_id - ID of post
         ***************************************************************/
        void interaction_delete_2(int64_t _person_id, int64_t _post_id) const noexcept;

        /****************************************************************
         ** \brief Given two person IDs, delete the "KNOWS" relationship
         *         between the two person nodes, and then return the count
         *         of the deleted relationships.
         *
         *  \param _src_person_id int64_t - ID of person
         *  \param _dst_person_id int64_t- ID of person
         ***************************************************************/
        void interaction_delete_8(int64_t _src_person_id, int64_t _dst_person_id) const noexcept;

        /****************************************************************
        ** \brief Given a person ID, returns the 10 most recent messages
        *         created by a specific person, along with the details
        *         of the posts to which these messages are replies and
        *         the creators of those posts.
        *
        *  \param _person_id - ID of person
s        ***************************************************************/
        void interaction_short_2(int64_t _person_id) const noexcept;

        /****************************************************************
         ** \brief Given the message ID, returns comments made on a specific
         *         message, including details about the comment authors and
         *         their relationship with the original message author.
         *         It then orders the results by comment creation date and
         *         the ID of the comment authors.
         *
         *  \param _message_id int64_t - ID of message
         ***************************************************************/
        void interaction_short_7(int64_t _message_id) const noexcept;

        [[nodiscard]] std::shared_ptr<std::vector<utils::SResult<ResultType>>> get_result_table() const noexcept;

      private:
        std::shared_ptr<std::vector<utils::SResult<ResultType>>> m_results;
        [[nodiscard]] storage::ILPGModel * get_graph() const noexcept;
        std::shared_ptr<storage::ILPGModel *> m_graph;
    };
} // namespace graphquery::database::query
