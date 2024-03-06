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

#include <utility>

namespace graphquery::database::analytic
{
    class IGraphAlgorithm
    {
      public:
        IGraphAlgorithm(std::string graph_name, const std::shared_ptr<logger::CLogSystem> & log_system): m_graph_name(std::move(graph_name)), m_log_system(log_system) {}

        virtual ~IGraphAlgorithm()               = default;
        IGraphAlgorithm(const IGraphAlgorithm &) = default;
        IGraphAlgorithm(IGraphAlgorithm &&)      = default;

        [[nodiscard]] virtual std::string_view get_name() const noexcept final { return m_graph_name; }
        virtual double compute(storage::ILPGModel *) const noexcept = 0;

      protected:
        const std::string m_graph_name;
        const std::shared_ptr<logger::CLogSystem> m_log_system;
    };
} // namespace graphquery::database::analytic

extern "C"
{
    void create_graph_algorithm(graphquery::database::analytic::IGraphAlgorithm ** graph_algorithm, const std::shared_ptr<graphquery::logger::CLogSystem> & log_system);
}
