/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file relax.h
 * \brief Abstract relax class for derived classes to implement
 *        relax function over edgemap.
 ************************************************************/

#pragma once

#include <cstdint>

namespace graphquery::database::analytic
{
    class IRelax
    {
      public:
        IRelax()          = default;
        virtual ~IRelax() = default;

        virtual void relax(int64_t src, int64_t dst) noexcept = 0;
    };
} // namespace graphquery::database::analytic
