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

        bool m_initialised = {};
        struct stat m_fd_info = {};
        int m_file_descriptor = {};
        char * m_memory_mapped_file = {};
        int64_t m_seek_offset = {};
        std::string m_file_path;

        int m_file_mode = {};
        int m_map_mode_prot = {};
        int m_map_mode_flags = {};
    };
}
