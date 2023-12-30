/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file relax.h
 * \brief Abstract class for analytic algorithms to implement
 *        for the relative relax function to called over each
 *        edge.
 ************************************************************/

#pragma once
#include <cstdint>

namespace graphquery::database::analytic
{
    class CRelax final
    {
      public:
        /**********************************************
        ** \brief Abstract relax function to be called
        *         over each edge.
        ** @return void
        ***********************************************/
        void relax(int64_t src, int64_t dst) noexcept;
    };
} // namespace graphquery::database::analytic
