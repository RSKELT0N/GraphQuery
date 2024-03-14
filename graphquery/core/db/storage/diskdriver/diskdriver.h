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

#include "memory_ref.h"
#include "log/logsystem/logsystem.h"
#include "db/storage/config.h"

#include <filesystem>
#include <functional>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <condition_variable>
#include <unistd.h>
#include <bit>
#include <cassert>

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

        static constexpr auto PAGE_SIZE  = KB(4);
        CDiskDriver(CDiskDriver &&)      = delete;
        CDiskDriver(const CDiskDriver &) = delete;

        template<typename T, bool write = false>
        inline SRef_t<T, write> ref(const int64_t seek = -1)
        {
            auto reference = std::bit_cast<T *>(ref<write>(seek, sizeof(T)));
            return SRef_t<T, write>(reference, &m_readers, &m_writer);
        }

        template<typename T, bool write = false>
        inline SRef_t<T, write> ref_update()
        {
            auto reference = std::bit_cast<T *>(ref_update<write>(sizeof(T)));
            return SRef_t<T, write>(reference, &m_readers, &m_writer);
        }

      private:
        template<bool write>
        void * ref(int64_t seek, const int64_t size) noexcept
        {
            static char * ptr = nullptr;
            seek              = seek == -1 ? m_seek_offset : seek;
            if (this->m_initialised)
            {
                if (m_fd_info.st_size <= seek + size)
                    resize((seek + size) * 2);

                if constexpr (write)
                {
                    while (utils::atomic_load(&m_readers) != 0 || utils::atomic_load(&m_writer) != 0) {}
                    utils::atomic_fetch_inc(&m_writer);
                }

                if constexpr (!write)
                {
                    while (utils::atomic_load(&m_writer) != 0) {}
                    utils::atomic_fetch_inc(&m_readers);
                }

                ptr = &this->m_memory_mapped_file[seek];
            }

            return ptr;
        }

        template<bool write>
        void * ref_update(const int64_t size) noexcept
        {
            static char * ptr = nullptr;
            if (this->m_initialised)
            {
                if constexpr (write)
                {
                    while (utils::atomic_load(&m_readers) != 0 || utils::atomic_load(&m_writer) != 0) {}
                    utils::atomic_fetch_inc(&m_writer);
                }

                if constexpr (!write)
                {
                    while (utils::atomic_load(&m_writer) != 0) {}
                    utils::atomic_fetch_inc(&m_readers);
                }

                ptr = &this->m_memory_mapped_file[m_seek_offset];
                m_seek_offset += size;
            }

            return ptr;
        }

      public:
        void clear_contents() noexcept;
        [[nodiscard]] size_t get_filesize() const noexcept;
        [[maybe_unused]] SRet_t close();
        [[maybe_unused]] SRet_t seek(int64_t offset);
        [[nodiscard]] uint64_t get_seek_offset() const noexcept;
        [[maybe_unused]] SRet_t open(std::string_view file_path);
        [[maybe_unused]] SRet_t read(void * ptr, int64_t size, uint32_t amt, bool update = true);
        [[maybe_unused]] SRet_t write(const void * ptr, int64_t size, uint32_t amt, bool update = true);

        void resize(int64_t file_size) noexcept;
        void set_path(std::filesystem::path file_path) noexcept;
        [[nodiscard]] SRet_t sync() const noexcept;
        [[nodiscard]] SRet_t async() const noexcept;
        [[nodiscard]] char operator[](int64_t idx) const noexcept;
        [[nodiscard]] bool check_if_initialised() const noexcept;
        [[nodiscard]] std::filesystem::path get_path() const noexcept;
        [[nodiscard]] static bool check_if_file_exists(std::string_view file_path) noexcept;
        [[nodiscard]] static bool check_if_folder_exists(std::string_view file_path) noexcept;
        [[nodiscard]] static bool check_if_file_exists(std::string_view path, std::string_view file_name) noexcept;
        [[maybe_unused]] static SRet_t create_folder(const std::filesystem::path & path, std::string_view folder_name);
        [[maybe_unused]] static SRet_t create_file(const std::filesystem::path & path, std::string_view file_name, int64_t file_size = PAGE_SIZE);

        static constexpr auto DEFAULT_FILE_SIZE = PAGE_SIZE;

      private:
        [[nodiscard]] SRet_t unmap() const noexcept;
        [[maybe_unused]] SRet_t open_fd() noexcept;
        [[nodiscard]] SRet_t close_fd() const noexcept;
        [[maybe_unused]] SRet_t map() noexcept;
        [[maybe_unused]] SRet_t remap(int64_t old_size) noexcept;
        [[maybe_unused]] SRet_t truncate(int64_t) noexcept;

        inline static int64_t resize_to_pagesize(int64_t size) noexcept;

        uint8_t m_writer;
        uint32_t m_readers;
        std::mutex m_resize_lock;
        static std::shared_ptr<logger::CLogSystem> m_log_system;

        int m_file_mode      = {}; //~ Set file mode of the descriptor when opened.
        int m_map_mode_prot  = {}; //~ Set map mode (protection) of the file when mapped.
        int m_map_mode_flags = {}; //~ Set map mode (flags) of the file when mapped.

        bool m_initialised           = {}; //~ Wether the fd descriptor is opened.
        struct stat m_fd_info        = {}; //~ Structure info on the currently opened file.
        int m_file_descriptor        = {}; //~ integer of the pointed file.
        int64_t m_seek_offset        = {}; //~ Current offset within the memory map.
        char * m_memory_mapped_file  = {}; //~ buffer address of the memory mapped file.
        std::filesystem::path m_path = {}; //~ Set path of the current context.
    };
} // namespace graphquery::database::storage