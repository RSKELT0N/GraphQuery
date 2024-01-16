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

#include <utility>

#include "db/storage/graph_model.h"

namespace graphquery::database::analytic
{
    class IGraphAlgorithm
    {
      public:
        IGraphAlgorithm(std::string graph_name): m_graph_name(std::move(graph_name)) {}

        virtual ~IGraphAlgorithm()               = default;
        IGraphAlgorithm(const IGraphAlgorithm &) = default;
        IGraphAlgorithm(IGraphAlgorithm &&)      = default;

        [[nodiscard]] virtual std::string_view get_name() const noexcept final { return m_graph_name; }
        virtual double compute(storage::ILPGModel *) noexcept = 0;

      protected:
        const std::string m_graph_name;
    };
} // namespace graphquery::database::analytic

extern "C"
{
    void create_graph_algorithm(graphquery::database::analytic::IGraphAlgorithm ** graph_algorithm);
}
