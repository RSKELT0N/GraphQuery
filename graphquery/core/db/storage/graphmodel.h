/************************************************************
* \author Ryan Skelton
* \date 18/09/2023
* \file graphmodel.h
* \brief
************************************************************/

#pragma once

namespace graphquery::database::storage
{
class IGraphModel
    {
    public:
        IGraphModel() = default;
        virtual ~IGraphModel() = default;

        virtual void Load() = 0;
        friend class CDBStorage;
    };
}