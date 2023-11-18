/************************************************************
* \author Ryan Skelton
* \date 18/09/2023
* \file diskdriver.h
* \brief Header of the disk driver which interacts with
*        creating and mapping actioned files to memory,
<<<<<<< HEAD
*        which allows easy access to write and read
*        efficiently to the host filesystem.
=======
*        which allows easy access to write and read efficiently
*        to the host filesystem.
>>>>>>> 4158259 (Add graph table.)
************************************************************/

#pragma once

#include <cstdint>
#include <iostream>
#include <sys/stat.h>

namespace graphquery::database::storage
{
    class CDiskDriver
    {
    protected:
        enum class SRet_t : uint16_t
        {
            ERROR = 0xFFFF,
            VALID = 0X0000
        };

    public:
        CDiskDriver(int file_mode, int map_mode_prot, int map_mode_flags);
        ~CDiskDriver();

        CDiskDriver(CDiskDriver&&) = delete;
        CDiskDriver(const CDiskDriver&) = delete;

        [[maybe_unused]] SRet_t Close();
        [[maybe_unused]] SRet_t Open();
        [[maybe_unused]] SRet_t Seek(int64_t offset);
        [[maybe_unused]] SRet_t Create(int64_t file_size);
        [[maybe_unused]] SRet_t Read(void* ptr, int64_t size, uint32_t amt) const;
        [[maybe_unused]] SRet_t Write(const void* ptr, int64_t size, uint32_t amt);
        [[maybe_unused]] char operator[](int64_t idx) const;

        void SetFilePath(std::string file_path) noexcept;
        [[nodiscard]] const std::string & GetFilePath() const noexcept;
        [[nodiscard]] bool CheckIfInitialised() const noexcept;
        [[nodiscard]] bool CheckIfFileExists(std::string_view file_path) const noexcept;

    private:
        void Resize(int64_t file_size) noexcept;
        [[maybe_unused]] SRet_t Unmap() noexcept;
        [[maybe_unused]] SRet_t OpenFD() noexcept;
        [[maybe_unused]] SRet_t CloseFD() noexcept;
        [[maybe_unused]] SRet_t Map() noexcept;
        [[maybe_unused]] SRet_t Truncate(int64_t ) noexcept;
        [[maybe_unused]] SRet_t CreateFile(int64_t file_size) noexcept;

        bool m_initialised = {};                //~ Wether the fd descriptor is opened.
        struct stat m_fd_info = {};             //~ Structure info on the currently opened file.
        int m_file_descriptor = {};             //~ integer of the pointed file.
        char * m_memory_mapped_file = {};       //~ buffer address of the memory mapped file.
        int64_t m_seek_offset = {};             //~ Current offset within the memory map.
        std::string m_file_path;                //~ Set file path of the file that is opened/closed.

        int m_file_mode = {};                   //~ Set file mode of the descriptor when opened.
        int m_map_mode_prot = {};               //~ Set map mode (protection) of the file when mapped.
        int m_map_mode_flags = {};              //~ Set map mode (flags) of the file when mapped.
    };
}
