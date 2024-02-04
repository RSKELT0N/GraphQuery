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
#include <chrono>

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

namespace graphquery::database::storage
{
    //~ Current configuration of database and graph entry.
    static constexpr uint8_t CFG_LPG_LABEL_LENGTH          = 20; //~ Length for a graph entry name
    static constexpr uint8_t CFG_LPG_PROPERTY_KEY_LENGTH   = 20; //~ Length for a graph entry name
    static constexpr uint8_t CFG_LPG_PROPERTY_VALUE_LENGTH = 20; //~ Length for a graph entry name

    static constexpr uint8_t CFG_GRAPH_NAME_LENGTH       = 20; //~ Length for a graph entry name
    static constexpr uint8_t CFG_GRAPH_MODEL_TYPE_LENGTH = 20; //~ Length for a graph model type

    //~ System config
    static constexpr auto CFG_SYSTEM_HEARTBEAT_INTERVAL = std::chrono::seconds(30);
} // namespace graphquery::database::storage