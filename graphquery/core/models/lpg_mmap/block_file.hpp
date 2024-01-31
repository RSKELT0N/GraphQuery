/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file block_file.hpp
 * \brief Block file for storing a payload with appending towards
 *        end of file. Helper class for lpg mmap memory model.
 ************************************************************/

#pragma once

#include "db/storage/diskdriver/diskdriver.h"

#include <cstdint>
#include <bitset>
#include <atomic>
#include <utility>

namespace graphquery::database::storage
{
    /****************************************************************
     * \enum EIndexValue_t
     * \brief Declares the possible index states for an index entry.
     *
     * \param MARKED_INDEX      - data block marked for deletion (not considered)
     * \param END_INDEX         - last of the linked data blocks
     * \param UNALLOCATED_INDEX - data block that has not been allocated
     ***************************************************************/
    enum EIndexValue_t : uint32_t
    {
        MARKED_DELETED = 0xFFFFFFFF,
        END_INDEX      = 0xFFFFFFF0,
    };

    /****************************************************************
     * \struct SDataBlock_t
     * \brief Structure of a data block holding a generic payload,
     *        entry to a datablock file.
     *
     * \param idx_state uint64_t - index of the data block
     * \param next uint64_t      - state of the next linked block.
     * \param payload std::array<T, N> - stored payload contained in data block
     ***************************************************************/
    template<typename T, uint8_t N>
        requires(N > 0)
    struct SDataBlock_t
    {
        uint32_t idx                  = {};
        std::bitset<N> state          = {};
        std::atomic<uint32_t> next    = END_INDEX;
        std::atomic<uint32_t> version = END_INDEX;
        std::array<T, N> payload      = {};
    };

    /****************************************************************
     * \struct SDataBlock_t
     * \brief Specialization for N = 1.
     *
     * \param idx_state uint64_t - index of the data block
     * \param next uint64_t      - state of the next linked block.
     * \param payload T          - stored payload contained in data block
     ***************************************************************/
    template<typename T>
    struct SDataBlock_t<T, 1>
    {
        uint32_t idx                  = {};
        std::bitset<1> state          = {};
        std::atomic<uint32_t> next    = END_INDEX;
        std::atomic<uint32_t> version = END_INDEX;
        T payload                     = {};
    };

    template<typename T, uint8_t N = 1>
        requires(N > 0)
    class CDatablockFile
    {
      public:
        /****************************************************************
         * \struct SBlockFileMetadata_t
         * \brief Describes the metadata for a generic data block file,
         *        holding neccessary information to access the index file
         *        correctly.
         *
         * \param data_block_c uint64_t       - amount of currently stored data blocks
         * \param data_blocks_offset uint64_t - start addr of data block entries
         * \param data_block_size uint32_t    - size of one data block
         * \param free_list uint32_t          - linked list of free data blocks
         ***************************************************************/
        struct SBlockFileMetadata_t
        {
            std::atomic<uint64_t> data_blocks_start_addr = {};
            std::atomic<uint32_t> data_block_c           = {};
            std::atomic<uint32_t> data_block_size        = {};
            std::atomic<uint32_t> free_list              = END_INDEX;
        };

        using STypeDataBlock = SDataBlock_t<T, N>;

        ~CDatablockFile();
        CDatablockFile()                                       = default;
        CDatablockFile(const CDatablockFile &)                 = delete;
        CDatablockFile(CDatablockFile &&) noexcept             = delete;
        CDatablockFile & operator=(const CDatablockFile &)     = default;
        CDatablockFile & operator=(CDatablockFile &&) noexcept = default;

        CDiskDriver & get_file() noexcept;
        CDiskDriver::SRet_t close() noexcept;
        inline void store_metadata() noexcept;
        inline SRef_t<SBlockFileMetadata_t> read_metadata() noexcept;
        inline SRef_t<SDataBlock_t<T, N>> read_entry(uint32_t offset) noexcept;
        void open(std::filesystem::path path, std::string_view file_name) noexcept;

        void append_free_data_block(uint32_t block_offset) noexcept;
        uint32_t create_base_entry(uint32_t next_ref) noexcept;
        [[nodiscard]] SRef_t<SDataBlock_t<T, N>> attain_data_block(uint32_t next_ref = END_INDEX) noexcept;
        [[nodiscard]] std::optional<SRef_t<SDataBlock_t<T, N>>> attain_free_data_block() noexcept;

      private:
        CDiskDriver m_file;
        static constexpr uint64_t METADATA_START_ADDR = 0x00000000;
    };
} // namespace graphquery::database::storage

template<typename T, uint8_t N>
    requires(N > 0)
graphquery::database::storage::CDatablockFile<T, N>::~CDatablockFile()
{
    (void) m_file.close();
}

template<typename T, uint8_t N>
    requires(N > 0)
void
graphquery::database::storage::CDatablockFile<T, N>::store_metadata() noexcept
{
    auto metadata                    = read_metadata();
    metadata->data_block_c           = 0;
    metadata->data_block_size        = sizeof(STypeDataBlock);
    metadata->data_blocks_start_addr = sizeof(SBlockFileMetadata_t);
    metadata->free_list              = END_INDEX;
}

template<typename T, uint8_t N>
    requires(N > 0)
graphquery::database::storage::SRef_t<typename graphquery::database::storage::CDatablockFile<T, N>::SBlockFileMetadata_t>
graphquery::database::storage::CDatablockFile<T, N>::read_metadata() noexcept
{
    return m_file.ref<SBlockFileMetadata_t>(METADATA_START_ADDR);
}

template<typename T, uint8_t N>
    requires(N > 0)
graphquery::database::storage::SRef_t<graphquery::database::storage::SDataBlock_t<T, N>>
graphquery::database::storage::CDatablockFile<T, N>::read_entry(uint32_t offset) noexcept
{
    static const auto base_addr      = read_metadata()->data_blocks_start_addr.load();
    static const auto datablock_size = read_metadata()->data_block_size.load();
    const auto effective_addr        = base_addr + (datablock_size * offset);
    return m_file.ref<STypeDataBlock>(effective_addr);
}

template<typename T, uint8_t N>
    requires(N > 0)
graphquery::database::storage::SRef_t<graphquery::database::storage::SDataBlock_t<T, N>>
graphquery::database::storage::CDatablockFile<T, N>::attain_data_block(const uint32_t next_ref) noexcept
{
    if (next_ref != END_INDEX)
    {
        auto data_block_ptr = read_entry(next_ref);

        if (!data_block_ptr->state.all())
            return data_block_ptr;
    }

    auto head_free_block_opt = attain_free_data_block();

    if (!head_free_block_opt.has_value())
    {
        const auto entry_offset = create_base_entry(next_ref);
        return read_entry(entry_offset);
    }

    return head_free_block_opt.value();
}

template<typename T, uint8_t N>
    requires(N > 0)
std::optional<graphquery::database::storage::SRef_t<graphquery::database::storage::SDataBlock_t<T, N>>>
graphquery::database::storage::CDatablockFile<T, N>::attain_free_data_block() noexcept
{
    const auto head = read_metadata()->free_list.load();

    if (head == END_INDEX)
        return std::nullopt;

    auto data_block_ptr = read_entry(head);

    read_metadata()->free_list = data_block_ptr->next.load();
    data_block_ptr->next       = END_INDEX;

    return data_block_ptr;
}

template<typename T, uint8_t N>
    requires(N > 0)
void
graphquery::database::storage::CDatablockFile<T, N>::append_free_data_block(uint32_t block_offset) noexcept
{
    const auto head = read_metadata()->free_list.load();

    SDataBlock_t<T, N> * data_block_ptr = read_entry(block_offset);
    data_block_ptr->idx                 = block_offset;
    data_block_ptr->state               = {};
    data_block_ptr->next                = head;
    data_block_ptr->version             = END_INDEX;
    data_block_ptr->payload             = {};

    read_metadata()->free_list = block_offset;
}

template<typename T, uint8_t N>
    requires(N > 0)
uint32_t
graphquery::database::storage::CDatablockFile<T, N>::create_base_entry(uint32_t next_ref) noexcept
{
    const uint32_t entry_offset = read_metadata()->data_block_c++;
    auto data_block_ptr         = read_entry(entry_offset);

    data_block_ptr->idx     = entry_offset;
    data_block_ptr->state   = {};
    data_block_ptr->next    = next_ref;
    data_block_ptr->version = END_INDEX;

    return entry_offset;
}

template<typename T, uint8_t N>
    requires(N > 0)
void
graphquery::database::storage::CDatablockFile<T, N>::open(std::filesystem::path path, std::string_view file_name) noexcept
{
    m_file.set_path(std::move(path));
    m_file.open(file_name);
    store_metadata();
}

template<typename T, uint8_t N>
    requires(N > 0)
graphquery::database::storage::CDiskDriver &
graphquery::database::storage::CDatablockFile<T, N>::get_file() noexcept
{
    return m_file;
}
