/************************************************************
* \author Ryan Skelton
* \date 18/09/2023
* \file storage.h
* \brief Storage header file providing an API class to access
*        graph underlying on-disk and in-memory datamodels.
************************************************************/

#pragma once

#include <memory>
#include <string>

namespace graphquery::database::storage
{
    class IGraphDiskStorage
    {
    public:
        IGraphDiskStorage() = default;
        virtual ~IGraphDiskStorage() = default;

        virtual void Load() = 0;

        friend class CGraphStorage;
    };

    class IGraphMemory
    {
    public:
        IGraphMemory() = default;
        virtual ~IGraphMemory() = default;

        virtual void Load() noexcept = 0;

        friend class CGraphStorage;
    };

    class CGraphStorage final
    {
    public:
        CGraphStorage();
        ~CGraphStorage();

        [[nodiscard]] bool IsExistingDBLoaded() const noexcept;
        void Initialise(std::string file_path) noexcept;
        void UnInitialise() noexcept;

    private:
        //~ Bool to check if a current database is loaded.
        bool m_existing_db_loaded;
        //~ Instance of the on-disk loader for the graph db.
        std::unique_ptr<graphquery::database::storage::IGraphDiskStorage> m_graph_loader;
        //~ Instance of the in-memory graph representation for the graph db.
        std::unique_ptr<graphquery::database::storage::IGraphMemory> m_graph_memory;
    };
}
