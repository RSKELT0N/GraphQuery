/************************************************************
* \author Ryan Skelton
* \date 18/09/2023
* \file diskdriver.h
* \brief Header of the disk driver which interacts with
*        creating and mapping actioned files to memory,
*        which allows easy access to write and read
*        efficiently to the host filesystem.
************************************************************/

#pragma once

#include "log/logsystem/logsystem.h"

#include <cstdint>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <cassert>
#include <unistd.h>

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

        [[maybe_unused]] SRet_t close();
        [[maybe_unused]] SRet_t open();
        [[maybe_unused]] SRet_t seek(int64_t offset);
        [[maybe_unused]] SRet_t create(int64_t file_size);
        [[maybe_unused]] SRet_t read(void* ptr, int64_t size, uint32_t amt) const;
        [[maybe_unused]] SRet_t write(const void* ptr, int64_t size, uint32_t amt);
        [[maybe_unused]] char operator[](int64_t idx);

        void set_file_path(std::string file_path) noexcept;
        [[nodiscard]] const std::string & get_file_path() const noexcept;
        [[nodiscard]] bool check_if_initialised() const noexcept;
        [[nodiscard]] bool check_if_file_exists(std::string_view file_path) const noexcept;

    private:
        void resize(int64_t file_size) noexcept;
        [[maybe_unused]] SRet_t unmap() const noexcept;
        [[maybe_unused]] SRet_t open_fd() noexcept;
        [[maybe_unused]] SRet_t close_fd() noexcept;
        [[maybe_unused]] SRet_t map() noexcept;
        [[maybe_unused]] SRet_t sync() noexcept;
        [[maybe_unused]] SRet_t truncate(int64_t ) noexcept;
        [[maybe_unused]] SRet_t create_file(int64_t file_size) noexcept;

        std::shared_ptr<logger::CLogSystem> m_log_system;

        bool m_initialised = {};                            //~ Wether the fd descriptor is opened.
        struct stat m_fd_info = {};                         //~ Structure info on the currently opened file.
        int m_file_descriptor = {};                         //~ integer of the pointed file.
        int64_t m_seek_offset = {};                         //~ Current offset within the memory map.
        std::string m_file_path = {};                       //~ Set file path of the file that is opened/closed.
        char * m_memory_mapped_file = {};                   //~ buffer address of the memory mapped file.

        int m_file_mode = {};                               //~ Set file mode of the descriptor when opened.
        int m_map_mode_prot = {};                           //~ Set map mode (protection) of the file when mapped.
        int m_map_mode_flags = {};                          //~ Set map mode (flags) of the file when mapped.

        mutable uint64_t m_current_bytes_written;           //~ Current amount of bytes written.
        static constexpr uint64_t MAX_WRITE_SIZE = 1<<10;   //~ max size to write to buffer.
    };
}
