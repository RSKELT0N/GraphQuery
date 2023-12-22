/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file lpg.h
 * \brief Dervied instance of a memory model, supporting
 *        the labelled property graph functionality.
 *
 *        (under development)
 ************************************************************/

#pragma once

#if defined(_WIN32) || defined(_WIN64)
#define LIB_EXPORT __declspec(dllexport)
#else
#define LIB_EXPORT
#endif

#include <db/storage/memory_model.h>

#include "db/storage/config.h"
#include "db/storage/diskdriver/diskdriver.h"
#include "db/storage/graph_model.h"

namespace graphquery::database::storage
{
	class CMemoryModelLPG final : public IMemoryModel, CLPGModel
	{
	  private:
		struct SGraphHead_t
		{
			char graph_name[GRAPH_NAME_LENGTH]		 = {};
			char graph_type[GRAPH_MODEL_TYPE_LENGTH] = {};
			int64_t vertices_c						 = {};
			int64_t edges_c							 = {};
			int16_t vertex_label_c					 = {};
			int16_t edge_label_c					 = {};
		} __attribute__((packed));

	  public:
		CMemoryModelLPG();
		~CMemoryModelLPG() override = default;

		void load_graph(std::filesystem::path path, std::string_view graph) noexcept override;
		void create_graph(std::filesystem::path path, std::string_view graph) noexcept override;
		void close() noexcept override;

		void add_node(const SProperty & properties) override;
		void add_edge(int64_t src, int64_t dst, const SProperty & properties) override;
		void delete_node(int64_t node_id) override;
		void delete_edge(int64_t src, int64_t dst) override;
		void update_node(int64_t node_id, const SProperty & properties) override;
		void update_edge(int64_t edge_id, const SProperty & properties) override;

		SNode get_node(int64_t node_id) override;
		SNode get_edge(int64_t edge_id) override;
		std::vector<SNode> get_nodes_by_label(int64_t label_id) override;
		std::vector<SNode> get_edges_by_label(int64_t label_id) override;

	  private:
		void CheckIfMainFileExists() noexcept;
		void StoreGraphHead() noexcept;
		void LoadGraphHead() noexcept;

		SGraphHead_t m_graph_header = {};
		CDiskDriver m_master_file;
		CDiskDriver m_connections;

		static constexpr uint16_t CONNECTIONS_FILE_SIZE		= KB(1);
		static constexpr const char * CONNECTIONS_FILE_NAME = "connections";
	};
} // namespace graphquery::database::storage
