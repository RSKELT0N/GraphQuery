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

#include <filesystem>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define KB(x) ((size_t) (x * (1 << 10)))
#define MB(x) ((size_t) (x * (1 << 20)))
#define GB(x) ((size_t) (x * (1 << 30)))

namespace graphquery::database::storage
{
    class CDiskDriver
    {
      public:
        enum class SRet_t : uint16_t
        {
            ERROR = 0xFFFF,
            VALID = 0X0000
        };

        explicit CDiskDriver(int file_mode = O_RDWR, int map_mode_prot = PROT_READ | PROT_WRITE, int map_mode_flags = MAP_SHARED);
        ~CDiskDriver();

        CDiskDriver(CDiskDriver &&)      = delete;
        CDiskDriver(const CDiskDriver &) = delete;

        [[maybe_unused]] SRet_t close();
        [[maybe_unused]] SRet_t seek(uint64_t offset);
        [[maybe_unused]] uint64_t get_seek_offset() const noexcept;
        [[maybe_unused]] SRet_t open(std::string_view file_path);
        [[maybe_unused]] SRet_t read(void * ptr, int64_t size, uint32_t amt, bool update = true);
        [[maybe_unused]] SRet_t write(const void * ptr, int64_t size, uint32_t amt, bool update = true);
        [[maybe_unused]] void * ref(uint64_t seek) noexcept;

        void resize(int64_t file_size) noexcept;
        void set_path(std::filesystem::path file_path) noexcept;
        [[nodiscard]] SRet_t sync() const noexcept;
        [[nodiscard]] char operator[](int64_t idx) const noexcept;
        [[nodiscard]] bool check_if_initialised() const noexcept;
        [[nodiscard]] std::filesystem::path get_path() const noexcept;
        [[nodiscard]] static bool check_if_file_exists(std::string_view file_path) noexcept;
        [[nodiscard]] static bool check_if_folder_exists(std::string_view file_path) noexcept;
        [[nodiscard]] static bool check_if_file_exists(std::string_view path, std::string_view file_name) noexcept;
        [[maybe_unused]] static SRet_t create_folder(const std::filesystem::path & path, std::string_view folder_name);
        [[maybe_unused]] static SRet_t create_file(const std::filesystem::path & path, std::string_view file_name, int64_t file_size);

        template<typename T>
        inline T * ref(const uint64_t seek)
        {
            return std::bit_cast<T *>(ref(seek));
        }

      private:
        [[maybe_unused]] SRet_t unmap() const noexcept;
        [[maybe_unused]] SRet_t open_fd() noexcept;
        [[maybe_unused]] SRet_t close_fd() noexcept;
        [[maybe_unused]] SRet_t map() noexcept;
        [[maybe_unused]] SRet_t truncate(int64_t) noexcept;

        static std::shared_ptr<logger::CLogSystem> m_log_system;

        int m_file_mode      = {}; //~ Set file mode of the descriptor when opened.
        int m_map_mode_prot  = {}; //~ Set map mode (protection) of the file when mapped.
        int m_map_mode_flags = {}; //~ Set map mode (flags) of the file when mapped.

        bool m_initialised           = {}; //~ Wether the fd descriptor is opened.
        struct stat m_fd_info        = {}; //~ Structure info on the currently opened file.
        int m_file_descriptor        = {}; //~ integer of the pointed file.
        uint64_t m_seek_offset       = {}; //~ Current offset within the memory map.
        char * m_memory_mapped_file  = {}; //~ buffer address of the memory mapped file.
        std::filesystem::path m_path = {}; //~ Set path of the current context.
    };
} // namespace graphquery::database::storage
