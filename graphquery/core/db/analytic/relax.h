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
    class CRelax
    {
      public:
        /**********************************************
        ** \brief Abstract relax function to be called
        *         over each edge.
        ** @return void
        ***********************************************/
        virtual void relax(uint64_t src, uint64_t dst) noexcept = 0;
    };
} // namespace graphquery::database::analytic
