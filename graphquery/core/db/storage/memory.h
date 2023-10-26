/************************************************************
* \author Ryan Skelton
* \date 18/09/2023
* \file memory.h
* \brief Memory header file, providing in-memory datamodels
*        to represent graph disk datamodel to give stored information
 *       between storage and query, analytic engines.
************************************************************/

#pragma once

#include "db/storage.h"

namespace graphquery::database::storage
{
    class IMemory
    {
    public:
        IMemory();
        virtual ~IMemory() = 0;

        friend class CStorage;
    };
}