#include "diskdriver.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cassert>
#include <cstdint>

#include "db/utils/lib.h"
#include "db/utils/ring_buffer.hpp"

//~ static symbol link
std::shared_ptr<graphquery::logger::CLogSystem> graphquery::database::storage::CDiskDriver::m_log_system;

graphquery::database::storage::CDiskDriver::CDiskDriver(const int map_mode_flags, const int file_mode, const int map_mode_prot)
{
    m_log_system               = logger::CLogSystem::get_instance();
    this->m_file_mode          = file_mode;
    this->m_map_mode_prot      = map_mode_prot;
    this->m_map_mode_flags     = map_mode_flags;
    this->m_memory_mapped_file = nullptr;
    this->reader_c             = 0;
    this->m_writer_lock.unlock();
}

graphquery::database::storage::CDiskDriver::~
CDiskDriver()
{
    if (this->m_initialised && m_map_mode_flags == MAP_SHARED)
    {
        (void) sync();
        close();
        this->m_initialised = false;
    }
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::open_fd() noexcept
{
    this->m_file_descriptor = ::open(m_path.generic_string().c_str(), m_file_mode);

    if (this->m_file_descriptor == -1)
    {
        m_log_system->error(fmt::format("File descriptor ({}) could not be "
                                        "created to open file (Error: {})",
                                        m_path.generic_string(),
                                        errno));
        return SRet_t::ERROR;
    }

    if (fstat(this->m_file_descriptor, &this->m_fd_info) == -1)
    {
        m_log_system->error(fmt::format("Issue getting file descriptor ({}) info", m_path.generic_string()));
        return SRet_t::ERROR;
    }
    return SRet_t::VALID;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::close_fd() const noexcept
{
    if (::close(this->m_file_descriptor) == -1)
    {
        m_log_system->error(fmt::format("Issue closing file descriptor ({}) (Error: {})", m_path.generic_string(), errno));
        return SRet_t::ERROR;
    }
    return SRet_t::VALID;
}

void
graphquery::database::storage::CDiskDriver::resize(const int64_t file_size) noexcept
{
    m_writer_lock.lock();
    const auto old_size = m_fd_info.st_size;

    if(old_size >= file_size)
    {
        m_writer_lock.unlock();
        return;
    }

    truncate(resize_to_pagesize(file_size));
    remap(old_size);
    m_writer_lock.unlock();
}

void
graphquery::database::storage::CDiskDriver::resize_override(const int64_t file_size) noexcept
{
    const auto old_size = m_fd_info.st_size;
    truncate(resize_to_pagesize(file_size));
    remap(old_size);
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::truncate(const int64_t file_size) noexcept
{
    if (this->m_file_descriptor == -1)
    {
        m_log_system->error(fmt::format("File descriptor ({}) could not be created to create file", m_path.generic_string()));
        return SRet_t::ERROR;
    }

    if (ftruncate(this->m_file_descriptor, file_size) == -1)
    {
        m_log_system->warning(fmt::format("Issue truncating the file to the specified size, error: {} ({})", strerror(errno), errno));
        ::close(this->m_file_descriptor);
        return SRet_t::ERROR;
    }

    if (fstat(this->m_file_descriptor, &this->m_fd_info) == -1)
    {
        m_log_system->error(fmt::format("Issue getting file descriptor ({}) info", m_path.generic_string()));
        return SRet_t::ERROR;
    }

    return SRet_t::VALID;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::map() noexcept
{
    this->m_memory_mapped_file = static_cast<char *>(mmap(nullptr, m_fd_info.st_size, m_map_mode_prot, m_map_mode_flags, m_file_descriptor, 0));

    if (this->m_memory_mapped_file == MAP_FAILED)
    {
        m_log_system->error(fmt::format("Error mapping file to memory"));
        return SRet_t::ERROR;
    }

    return SRet_t::VALID;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::remap([[maybe_unused]] const int64_t old_size) noexcept
{
#if defined(__APPLE__)
    this->m_memory_mapped_file = static_cast<char *>(mmap(m_memory_mapped_file, m_fd_info.st_size, m_map_mode_prot, m_map_mode_flags, m_file_descriptor, 0));
#else
    this->m_memory_mapped_file = static_cast<char *>(mremap(m_memory_mapped_file, old_size, m_fd_info.st_size, MREMAP_MAYMOVE));
#endif

    if (this->m_memory_mapped_file == MAP_FAILED)
    {
        m_log_system->error(fmt::format("Error mapping file to memory {} ({})", strerror(errno), errno));
        return SRet_t::ERROR;
    }

    return SRet_t::VALID;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::unmap() const noexcept
{
    if (munmap(this->m_memory_mapped_file, this->m_fd_info.st_size) == -1)
    {
        m_log_system->error(fmt::format("Error unmapping file from memory {} ({})", strerror(errno), errno));
        return SRet_t::ERROR;
    }

    return SRet_t::VALID;
}

void
graphquery::database::storage::CDiskDriver::set_path(std::filesystem::path path) noexcept
{
    this->m_path = std::move(path);
}

std::filesystem::path
graphquery::database::storage::CDiskDriver::get_path() const noexcept
{
    return m_path.generic_string();
}

bool
graphquery::database::storage::CDiskDriver::check_if_file_exists(const std::string_view file_path) noexcept
{
    if (access(file_path.data(), F_OK) == 0)
        return true;

    return false;
}

bool
graphquery::database::storage::CDiskDriver::check_if_file_exists(const std::string_view path, const std::string_view file_name) noexcept
{
    const auto & file_path = std::filesystem::path(path) / file_name;

    if (access(file_path.string().c_str(), F_OK) == 0)
        return true;

    return false;
}

bool
graphquery::database::storage::CDiskDriver::check_if_folder_exists(std::string_view folder_path) noexcept
{
    struct stat folderStat = {};

    if (stat(folder_path.cbegin(), &folderStat) == 0)
    {
        if (S_ISDIR(folderStat.st_mode))
            return true;
    }

    return false;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::create_file(const std::filesystem::path & path, std::string_view file_name, const int64_t file_size)
{
    std::string file_path = fmt::format("{}/{}", path.string(), file_name);
    if (check_if_file_exists(file_path))
    {
        m_log_system->warning("Cannot create file that already exists");
        return SRet_t::ERROR;
    }

    const int fd = ::open(file_path.c_str(), O_CREAT | O_TRUNC | PROT_WRITE | O_RDWR, S_IWRITE | S_IWUSR | S_IRUSR);

    if (fd == -1)
    {
        m_log_system->error(fmt::format("File descriptor ({}) could not be created to create file", file_path));
        return SRet_t::ERROR;
    }

    if (ftruncate(fd, file_size) == -1)
    {
        m_log_system->warning(fmt::format("Issue truncating the file to the specified size {}", errno));
        ::close(fd);
        return SRet_t::ERROR;
    }

    ::close(fd);
    return SRet_t::VALID;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::create_folder(const std::filesystem::path & path, std::string_view folder_name)
{
    std::string folder_path = fmt::format("{}/{}", path.string(), folder_name);

    if (check_if_folder_exists(folder_path))
    {
        m_log_system->warning("Cannot create folder that already exists");
        return SRet_t::ERROR;
    }

    if (mkdir(folder_path.c_str(), 0777))
        return SRet_t::ERROR;

    return SRet_t::VALID;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::open(const std::string_view file_name)
{
    const std::filesystem::path file_path = m_path / file_name;

    if (!check_if_file_exists(file_path.generic_string()))
    {
        m_log_system->warning("Could not open a file that doesn't exist");
        return SRet_t::ERROR;
    }

    this->m_path = file_path;
    open_fd();
    map();

    this->m_initialised = true;
    return SRet_t::VALID;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::sync() const noexcept
{
    if (this->m_initialised && m_map_mode_flags == MAP_SHARED)
    {
        if (msync(m_memory_mapped_file, static_cast<size_t>(m_fd_info.st_size), MS_SYNC) == -1)
        {
            m_log_system->warning("Issue syncing the buffer");
            return SRet_t::ERROR;
        }

        return SRet_t::VALID;
    }

    m_log_system->warning("File has not been initialised");
    return SRet_t::ERROR;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::async() const noexcept
{
    if (this->m_initialised && m_map_mode_flags == MAP_SHARED)
    {
        if (msync(m_memory_mapped_file, static_cast<size_t>(m_fd_info.st_size), MS_ASYNC) == -1)
        {
            m_log_system->warning("Issue syncing the buffer");
            return SRet_t::ERROR;
        }

        return SRet_t::VALID;
    }

    m_log_system->warning("File has not been initialised");
    return SRet_t::ERROR;
}

void
graphquery::database::storage::CDiskDriver::clear_contents() noexcept
{
    if (m_initialised)
    {
        memset(&m_memory_mapped_file[0], 0, m_fd_info.st_size);
        this->reader_c             = 0;
        this->m_writer_lock.unlock();
    }
}

size_t
graphquery::database::storage::CDiskDriver::get_filesize() const noexcept
{
    if (m_initialised)
    {
        return m_fd_info.st_size;
    }
    return 0;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::close()
{
    if (this->m_initialised)
    {
        if (m_map_mode_flags == MAP_SHARED)
            assert(sync() == SRet_t::VALID);
        assert(unmap() == SRet_t::VALID);
        assert(close_fd() == SRet_t::VALID);
        this->m_initialised = false;
        return SRet_t::VALID;
    }

    m_log_system->warning("File has not been initialised");
    return SRet_t::ERROR;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::read(void * ptr, const int64_t size, const uint32_t amt, bool update)
{
    if (this->m_initialised)
    {
        if (m_fd_info.st_size <= size * amt + m_seek_offset)
            resize((size * amt + m_seek_offset) * 2);

        memcpy(ptr, &this->m_memory_mapped_file[this->m_seek_offset], size * amt);

        if (update)
            this->m_seek_offset += size * amt;
    }
    else
        m_log_system->warning("File has not been initialised");
    return SRet_t::VALID;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::write(const void * ptr, const int64_t size, const uint32_t amt, bool update)
{
    if (this->m_initialised)
    {
        if (m_fd_info.st_size <= size * amt + m_seek_offset)
            resize((size * amt + m_seek_offset) * 2);

        memcpy(&this->m_memory_mapped_file[this->m_seek_offset], ptr, size * amt);
        if (update)
            this->m_seek_offset += size * amt;
    }
    else
        m_log_system->warning("File has not been initialised");

    return SRet_t::VALID;
}

char
graphquery::database::storage::CDiskDriver::operator[](const int64_t idx) const noexcept
{
    char ret = {};
    if (this->m_initialised)
    {
        assert(idx >= 0L && idx <= this->m_fd_info.st_size);

        ret = this->m_memory_mapped_file[idx];
    }
    else
        m_log_system->warning("File has not been initialised");

    return ret;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::seek(const int64_t offset)
{
    if (this->m_initialised)
    {
        this->m_seek_offset = offset;
    }
    else
        m_log_system->warning("File has not been initialised");

    return SRet_t::VALID;
}

int64_t
graphquery::database::storage::CDiskDriver::resize_to_pagesize(const int64_t size) noexcept
{
    const int64_t pages = utils::ceilaferdiv(size, PAGE_SIZE);
    return pages * PAGE_SIZE;
}

uint64_t
graphquery::database::storage::CDiskDriver::get_seek_offset() const noexcept
{
    return this->m_seek_offset;
}

bool
graphquery::database::storage::CDiskDriver::check_if_initialised() const noexcept
{
    return this->m_initialised;
}