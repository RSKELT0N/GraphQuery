#include "lpg.h"

#include <fcntl.h>
#include <sys/mman.h>

graphquery::database::storage::CMemoryModelLPG::CMemoryModelLPG():
	m_master_file(O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED),
	m_connections(O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED)
{
}

void
graphquery::database::storage::CMemoryModelLPG::create_graph(std::filesystem::path path,
															 std::string_view graph) noexcept
{
	m_master_file.set_path(path);
	m_connections.set_path(path);

	m_master_file.create_folder(graph);
	m_connections.set_path(m_master_file.get_path());

	m_master_file.create_file(MASTER_FILE_NAME, MASTER_FILE_SIZE);
	m_connections.create_file(CONNECTIONS_FILE_NAME, CONNECTIONS_FILE_SIZE);
	m_master_file.open(MASTER_FILE_NAME);
	m_connections.open(CONNECTIONS_FILE_NAME);
}

void
graphquery::database::storage::CMemoryModelLPG::load_graph(std::filesystem::path path,
														   std::string_view graph) noexcept
{
	m_master_file.set_path(path);
	m_connections.set_path(path);
	m_master_file.open(MASTER_FILE_NAME);
	m_connections.open(CONNECTIONS_FILE_NAME);
}

void
graphquery::database::storage::CMemoryModelLPG::close() noexcept
{
	m_master_file.close();
	m_connections.close();
}

void
graphquery::database::storage::CMemoryModelLPG::add_node(
	const graphquery::database::storage::CLPGModel::SProperty & properties)
{
}
void
graphquery::database::storage::CMemoryModelLPG::add_edge(
	int64_t src,
	int64_t dst,
	const graphquery::database::storage::CLPGModel::SProperty & properties)
{
}

void
graphquery::database::storage::CMemoryModelLPG::delete_node(int64_t node_id)
{
}

void
graphquery::database::storage::CMemoryModelLPG::delete_edge(int64_t src, int64_t dst)
{
}

void
graphquery::database::storage::CMemoryModelLPG::update_node(
	int64_t node_id,
	const graphquery::database::storage::CLPGModel::SProperty & properties)
{
}

void
graphquery::database::storage::CMemoryModelLPG::update_edge(
	int64_t edge_id,
	const graphquery::database::storage::CLPGModel::SProperty & properties)
{
}

graphquery::database::storage::CLPGModel::SNode
graphquery::database::storage::CMemoryModelLPG::get_node(int64_t node_id)
{
	return {};
}

graphquery::database::storage::CLPGModel::SNode
graphquery::database::storage::CMemoryModelLPG::get_edge(int64_t edge_id)
{
	return {};
}

std::vector<graphquery::database::storage::CLPGModel::SNode>
graphquery::database::storage::CMemoryModelLPG::get_nodes_by_label(int64_t label_id)
{
	return {};
}

std::vector<graphquery::database::storage::CLPGModel::SNode>
graphquery::database::storage::CMemoryModelLPG::get_edges_by_label(int64_t label_id)
{
	return {};
}

extern "C"
{
	LIB_EXPORT void create_graph_model(std::unique_ptr<graphquery::database::storage::IMemoryModel> & model)
	{
		model = std::make_unique<graphquery::database::storage::CMemoryModelLPG>();
	}
}
