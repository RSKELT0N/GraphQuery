/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file block_file.hpp
 * \brief Block file for storing a payload with appending towards
 *        end of file. Helper class for lpg mmap memory model.
 ************************************************************/

#pragma once

#include "db/utils/atomic_intrinsics.h"
#include "db/storage/diskdriver/diskdriver.h"

#include <cstdint>
#include <bitset>
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
     * \param idx_state uint32_t - index of the data block
     * \param next uint32_t      - state of the next linked block.
     * \param payload std::array<T, N> - stored payload contained in data block
     ***************************************************************/
    template<typename T, uint8_t N>
        requires(N > 0)
    struct SDataBlock_t
    {
        uint32_t idx             = {};
        std::bitset<N> state     = {};
        uint32_t next            = END_INDEX;
        uint32_t version         = END_INDEX;
        std::array<T, N> payload = {};
        uint8_t payload_amt      = {};
    };

    /****************************************************************
     * \struct SDataBlock_t
     * \brief Specialization for N = 1.
     *
     * \param idx_state uint32_t - index of the data block
     * \param next uint32_t      - state of the next linked block.
     * \param payload T          - stored payload contained in data block
     ***************************************************************/
    template<typename T>
    struct SDataBlock_t<T, 1>
    {
        uint32_t idx         = END_INDEX;
        std::bitset<1> state = {0};
        uint32_t next        = END_INDEX;
        uint32_t version     = END_INDEX;
        T payload            = {};
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
         * \param data_block_c uint32_t       - amount of currently stored data blocks
         * \param data_blocks_offset uint32_t - start addr of data block entries
         * \param data_block_size uint32_t    - size of one data block
         * \param free_list uint32_t          - linked list of free data blocks
         ***************************************************************/
        struct SBlockFileMetadata_t
        {
            uint32_t data_blocks_start_addr = {};
            uint32_t data_block_c           = {};
            uint32_t data_block_size        = {};
            uint32_t free_list              = END_INDEX;
        };

        using STypeDataBlock = SDataBlock_t<T, N>;

        ~CDatablockFile() { (void) m_file.close(); }
        CDatablockFile()                                       = default;
        CDatablockFile(const CDatablockFile &)                 = delete;
        CDatablockFile(CDatablockFile &&) noexcept             = delete;
        CDatablockFile & operator=(const CDatablockFile &)     = default;
        CDatablockFile & operator=(CDatablockFile &&) noexcept = default;

        CDiskDriver & get_file() noexcept;
        inline void store_metadata() noexcept;
        inline SRef_t<SBlockFileMetadata_t> read_metadata() noexcept;
        inline SRef_t<SDataBlock_t<T, N>> read_entry(int64_t offset) noexcept;
        void open(std::filesystem::path path, std::string_view file_name, bool create) noexcept;

        uint32_t create_entry(uint32_t next_ref = END_INDEX) noexcept;
        void append_free_data_block(uint32_t block_offset) noexcept;
        void foreach_block(const std::function<void(SRef_t<SDataBlock_t<T, N>> &)> &);
        void foreach_block(uint32_t start_block, const std::function<void(SRef_t<SDataBlock_t<T, N>> &)> &);
        [[nodiscard]] SRef_t<SDataBlock_t<T, N>> attain_data_block(uint32_t next_ref = END_INDEX) noexcept;
        [[nodiscard]] std::optional<SRef_t<SDataBlock_t<T, N>>> attain_free_data_block() noexcept;

      private:
        CDiskDriver m_file;
        static constexpr uint32_t METADATA_START_ADDR = 0x00000000;
    };
} // namespace graphquery::database::storage

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
graphquery::database::storage::CDatablockFile<T, N>::read_entry(int64_t offset) noexcept
{
    static const auto base_addr      = utils::atomic_load(&read_metadata()->data_blocks_start_addr);
    static const auto datablock_size = utils::atomic_load(&read_metadata()->data_block_size);
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
        const auto entry_offset = create_entry(next_ref);
        return read_entry(entry_offset);
    }

    return head_free_block_opt.value();
}

template<typename T, uint8_t N>
    requires(N > 0)
std::optional<graphquery::database::storage::SRef_t<graphquery::database::storage::SDataBlock_t<T, N>>>
graphquery::database::storage::CDatablockFile<T, N>::attain_free_data_block() noexcept
{
    auto metadata   = read_metadata();
    const auto head = utils::atomic_load(&metadata->free_list);

    if (head == END_INDEX)
        return std::nullopt;

    auto data_block_ptr = read_entry(head);

    utils::atomic_store(&metadata->free_list, data_block_ptr->next);
    utils::atomic_store(&data_block_ptr->next, static_cast<uint32_t>(END_INDEX));

    return data_block_ptr;
}

template<typename T, uint8_t N>
    requires(N > 0)
void
graphquery::database::storage::CDatablockFile<T, N>::append_free_data_block(uint32_t block_offset) noexcept
{
    auto metadata   = read_metadata();
    const auto head = utils::atomic_load(&metadata->free_list);
    utils::atomic_store(&metadata->free_list, block_offset);

    SRef_t<STypeDataBlock> data_block_ptr = read_entry(block_offset);
    data_block_ptr->idx                   = block_offset;
    data_block_ptr->state                 = 0;
    data_block_ptr->next                  = head;
    data_block_ptr->version               = END_INDEX;
}

template<typename T, uint8_t N>
    requires(N > 0)
uint32_t
graphquery::database::storage::CDatablockFile<T, N>::create_entry(uint32_t next_ref) noexcept
{
    const uint32_t entry_offset = utils::atomic_fetch_inc(&read_metadata()->data_block_c);
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
graphquery::database::storage::CDatablockFile<T, N>::foreach_block(const uint32_t start_block, const std::function<void(SRef_t<SDataBlock_t<T, N>> &)> & apply)
{
    if (start_block >= read_metadata()->data_block_c)
        return;

    auto block_next = start_block;

    while (block_next != END_INDEX)
    {
        auto block_ptr = read_entry(block_next);

        if (unlikely(!block_ptr->state.any()))
            continue;

        apply(block_ptr);
        block_next = block_ptr->next;
    }
}

template<typename T, uint8_t N>
    requires(N > 0)
void
graphquery::database::storage::CDatablockFile<T, N>::foreach_block(const std::function<void(SRef_t<SDataBlock_t<T, N>> &)> & apply)
{
    const uint32_t datablock_c = read_metadata()->data_block_c;
    auto block_ptr             = read_entry(0);

    for (uint32_t i = 0; i < datablock_c; i++, ++block_ptr)
    {
        if (unlikely(block_ptr->state.any()))
            continue;

        apply(block_ptr);
    }
}

template<typename T, uint8_t N>
    requires(N > 0)
void
graphquery::database::storage::CDatablockFile<T, N>::open(std::filesystem::path path, const std::string_view file_name, const bool create) noexcept
{
    if (create)
        CDiskDriver::create_file(path, file_name);

    m_file.set_path(std::move(path));
    m_file.open(file_name);
}

template<typename T, uint8_t N>
    requires(N > 0)
graphquery::database::storage::CDiskDriver &
graphquery::database::storage::CDatablockFile<T, N>::get_file() noexcept
{
    return m_file;
}
