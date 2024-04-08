/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file index_file.hpp
 * \brief Block file for storing a payload with appending towards
 *        end of file. Helper class for lpg mmap memory model.
 ************************************************************/

#pragma once

#include "db/storage/diskdriver/diskdriver.h"
#include "db/utils/atomic_intrinsics.h"
#include "db/storage/graph_model.h"

#include <cstdint>

namespace graphquery::database::storage
{
    class CIndexFile
    {
    public:
        /****************************************************************
         * \struct SIndexMetadata_t
         * \brief Describes the metadata for the index table, holding neccessary
         *        information to access the index file correctly.
         *
         * \param index_c uint32_t               - amount of indices stored
         * \param index_list_start_addr uint32_t - start addr of index list
         * \param index_size uint32_t            - size of one index
         ***************************************************************/
        struct SIndexMetadata_t
        {
            int64_t index_list_start_addr = {};
            int64_t index_size            = {};
            Id_t index_c                  = {};
        };

        /****************************************************************
         * \struct SIndexEntry_t
         * \brief Structure of an index entry to the index table.
         *
         * \param id uint32_t       - generic identifier for the payload
         * \param offset uint32_t   - respective offset for the payload
         * \param label_id uint16_t - generic label id for the index container
         ***************************************************************/
        struct SIndexEntry_t
        {
            Id_t offset = END_INDEX;
            uint8_t set = {};
        };

        ~CIndexFile();
        CIndexFile();
        CIndexFile(const CIndexFile &)                 = delete;
        CIndexFile(CIndexFile &&) noexcept             = delete;
        CIndexFile & operator=(const CIndexFile &)     = delete;
        CIndexFile & operator=(CIndexFile &&) noexcept = delete;

        void reset() noexcept;
        CDiskDriver & get_file() noexcept;
        inline void store_metadata() noexcept;
        void open(std::filesystem::path path, std::string_view file_name, bool create) noexcept;
        bool store_entry(Id_t id, int64_t offset) noexcept;

        template<bool write = false>
        inline SRef_t<SIndexMetadata_t, write> read_metadata() noexcept;

        template<bool write = false>
        inline SRef_t<SIndexEntry_t, write> read_entry(int64_t offset) noexcept;

    private:
        CDiskDriver m_file;
        static constexpr int64_t METADATA_START_ADDR = 0x00000000;
    };
} // namespace graphquery::database::storage

inline graphquery::database::storage::CIndexFile::CIndexFile(): m_file(LPG_MAP_MODE)
{
}

inline
graphquery::database::storage::CIndexFile::~
CIndexFile()
{
    (void) m_file.close();
}

inline void
graphquery::database::storage::CIndexFile::store_metadata() noexcept
{
    auto metadata                   = read_metadata();
    metadata->index_c               = 0;
    metadata->index_size            = sizeof(SIndexEntry_t);
    metadata->index_list_start_addr = sizeof(SIndexMetadata_t);
}

template<bool write>
inline graphquery::database::storage::SRef_t<graphquery::database::storage::CIndexFile::SIndexEntry_t, write>
graphquery::database::storage::CIndexFile::read_entry(const int64_t offset) noexcept
{
    static const auto base_addr  = utils::atomic_load(&read_metadata()->index_list_start_addr);
    static const auto index_size = utils::atomic_load(&read_metadata()->index_size);
    const auto effective_addr    = base_addr + index_size * offset;
    return m_file.ref<SIndexEntry_t, write>(effective_addr);
}

inline bool
graphquery::database::storage::CIndexFile::store_entry(const Id_t id, const int64_t offset) noexcept
{
    uint8_t expected   = 0;
    uint8_t new_value  = 1;
    const auto index_c = utils::atomic_load(&read_metadata()->index_c);
    auto index_ptr     = read_entry(id);

    if (!utils::atomic_fetch_cas(&index_ptr->set, expected, new_value, true))
        return false;

    utils::atomic_store(&index_ptr->offset, offset);

    if (id >= index_c)
        utils::atomic_fetch_inc(&read_metadata()->index_c);
    return true;
}

template<bool write>
inline graphquery::database::storage::SRef_t<graphquery::database::storage::CIndexFile::SIndexMetadata_t, write>
graphquery::database::storage::CIndexFile::read_metadata() noexcept
{
    return m_file.ref<SIndexMetadata_t, write>(METADATA_START_ADDR);
}

inline void
graphquery::database::storage::CIndexFile::open(std::filesystem::path path, const std::string_view file_name, const bool create) noexcept
{
    if (create)
        CDiskDriver::create_file(path, file_name);

    m_file.set_path(std::move(path));
    m_file.open(file_name);
}

inline graphquery::database::storage::CDiskDriver &
graphquery::database::storage::CIndexFile::get_file() noexcept
{
    return m_file;
}

inline void
graphquery::database::storage::CIndexFile::reset() noexcept
{
    m_file.resize_override(CDiskDriver::DEFAULT_FILE_SIZE);
    m_file.clear_contents();
    store_metadata();
    (void) m_file.sync();
}