/************************************************************
* \author Ryan Skelton
* \date 18/09/2023
* \file loader.h
* \brief Loader header file, providing on-disk graph datamodels
*        to load database stored on disk (hdd/ssd).
************************************************************/

#pragma once

#include "db/storage.h"

namespace graphquery::database::storage
{
    class ILoader
    {
    public:
        ILoader();
        virtual ~ILoader() = 0;

        friend class CStorage;
    };
}