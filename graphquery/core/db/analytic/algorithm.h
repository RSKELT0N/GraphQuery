/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file algorithm.h
 * \brief Abstract algorithm class for derived classes to implement
 *        required functionality for the analytic engine to load.
 ************************************************************/

#pragma once

#if defined(_WIN32) || defined(_WIN64)
#define LIB_EXPORT __declspec(dllexport)
#else
#define LIB_EXPORT
#endif

#include "db/storage/graph_model.h"

namespace graphquery::database::analytic
{
    class IGraphAlgorithm
    {
      public:
        IGraphAlgorithm()          = default;
        virtual ~IGraphAlgorithm() = default;

        virtual double compute(std::shared_ptr<storage::ILPGModel>) noexcept = 0;
    };
} // namespace graphquery::database::analytic

extern "C"
{
    void create_graph_algorithm(graphquery::database::analytic::IGraphAlgorithm ** graph_algorithm);
}
