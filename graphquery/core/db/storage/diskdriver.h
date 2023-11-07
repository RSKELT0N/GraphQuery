#pragma once

#include <iostream>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

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
        CDiskDriver() = default;
        ~CDiskDriver();

        CDiskDriver(const CDiskDriver&) = delete;
        CDiskDriver(CDiskDriver&&) = delete;

        [[maybe_unused]] SRet_t Close();
        [[maybe_unused]] SRet_t Seek(int64_t offset);
        [[maybe_unused]] SRet_t Create(std::string_view file_path, size_t file_size);
        [[maybe_unused]] SRet_t Open(std::string_view file_path, int file_mode, int map_mode_prot, int map_mode_flags);
        [[maybe_unused]] SRet_t Read(void* ptr, size_t size, uint32_t amt) const;
        [[maybe_unused]] SRet_t Write(const void* ptr, size_t size, uint32_t amt);
        [[maybe_unused]] char operator[](int64_t idx) const;

    private:
        bool m_initialised = {};
        struct stat m_fd_info = {};
        int m_file_descriptor = {};
        char * m_memory_mapped_file = {};
        size_t m_seek_offset = {};
    };
}
