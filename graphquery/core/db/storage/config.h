/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file config.h
 * \brief Header file includes the configuration of the storage
 *        system of GraphQuery, which interests classes can include
 *        to follow the configured settings of the rest of the program.
 ************************************************************/

#pragma once

#include <cstdint>

namespace graphquery::database::storage
{
	//~ Current configuration of database and graph entry.

	static constexpr uint8_t GRAPH_NAME_LENGTH		 = 20; //~ Length for a graph entry name
	static constexpr uint8_t GRAPH_MODEL_TYPE_LENGTH = 10; //~ Length for a graph model type
} // namespace graphquery::database::storage