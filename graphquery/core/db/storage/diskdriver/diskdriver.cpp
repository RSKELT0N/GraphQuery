#include "diskdriver.h"

#include <atomic>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cassert>
#include <cstdint>

namespace
{
    const auto wait_on_resizing = [](const uint8_t * resizing) -> bool { return *resizing == 0; };
    const auto wait_on_refs     = [](const uint32_t * refs) -> bool { return *refs == 0; };
} // namespace

//~ static symbol link
std::shared_ptr<graphquery::logger::CLogSystem> graphquery::database::storage::CDiskDriver::m_log_system;

graphquery::database::storage::CDiskDriver::
CDiskDriver(const int file_mode, const int map_mode_prot, const int map_mode_flags)
{
    m_log_system           = logger::CLogSystem::get_instance();
    this->m_file_mode      = file_mode;
    this->m_map_mode_prot  = map_mode_prot;
    this->m_map_mode_flags = map_mode_flags;
    this->m_resizing       = 0;
    this->m_ref_counter    = 0;
    this->m_unq_lock       = std::unique_lock(m_resize_lock);
}

graphquery::database::storage::CDiskDriver::~
CDiskDriver()
{
    if (this->m_initialised)
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
graphquery::database::storage::CDiskDriver::close_fd() noexcept
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
    m_cv_lock.wait(m_unq_lock, wait_on_refs(&m_ref_counter));
    m_resizing = 1;

    assert(unmap() == SRet_t::VALID);
    truncate(file_size);
    map();

    m_resizing = 0;
    m_unq_lock.unlock();
    m_cv_lock.notify_all();
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
        m_log_system->warning(fmt::format("Issue truncating the file to the specified size {}", errno));
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
    this->m_memory_mapped_file = static_cast<char *>(mmap(nullptr, static_cast<size_t>(m_fd_info.st_size), m_map_mode_prot, m_map_mode_flags, m_file_descriptor, 0));
    if (this->m_memory_mapped_file == MAP_FAILED)
    {
        m_log_system->error(fmt::format("Error mapping file to memory"));
        return SRet_t::ERROR;
    }

    return SRet_t::VALID;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::unmap() const noexcept
{
    if (munmap(this->m_memory_mapped_file, static_cast<size_t>(this->m_fd_info.st_size)) == -1)
    {
        m_log_system->error(fmt::format("Error unmapping file from memory"));
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

    int fd = ::open(file_path.c_str(), O_CREAT | O_TRUNC | PROT_WRITE, S_IWRITE | S_IWUSR | S_IRUSR);

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
graphquery::database::storage::CDiskDriver::open(std::string_view file_name)
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
    if (this->m_initialised)
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
graphquery::database::storage::CDiskDriver::close()
{
    if (this->m_initialised)
    {
        assert(unmap() == SRet_t::VALID);
        close_fd();
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
        if (m_fd_info.st_size < static_cast<int64_t>(size * amt + m_seek_offset))
            resize(static_cast<int64_t>(size * amt + m_seek_offset));

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
        if (m_fd_info.st_size < static_cast<int64_t>(size * amt + m_seek_offset))
            resize(static_cast<int64_t>(size * amt + m_seek_offset) * 2);

        memcpy(&this->m_memory_mapped_file[this->m_seek_offset], ptr, size * amt);
        if (update)
            this->m_seek_offset += size * amt;
    }
    else
        m_log_system->warning("File has not been initialised");

    return SRet_t::VALID;
}

void *
graphquery::database::storage::CDiskDriver::ref(const uint64_t seek) noexcept
{
    static char * ptr = nullptr;
    if (this->m_initialised)
    {
        if (m_fd_info.st_size < static_cast<int64_t>(seek))
            resize(static_cast<int64_t>(seek) * 2);

        m_cv_lock.wait(m_unq_lock, wait_on_resizing(&m_resizing));
        ptr = &this->m_memory_mapped_file[seek];
    }

    return ptr;
}

void *
graphquery::database::storage::CDiskDriver::ref_update(const uint64_t size) noexcept
{
    static char * ptr = nullptr;
    if (this->m_initialised)
    {
        if (m_fd_info.st_size < static_cast<int64_t>(m_seek_offset))
            resize(static_cast<int64_t>(m_seek_offset) * 2);

        m_cv_lock.wait(m_unq_lock, wait_on_resizing(&m_resizing));
        ptr = &this->m_memory_mapped_file[m_seek_offset];
        m_seek_offset += size;
    }

    return ptr;
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
graphquery::database::storage::CDiskDriver::seek(const uint64_t offset)
{
    if (this->m_initialised)
    {
        this->m_seek_offset = offset;
    }
    else
        m_log_system->warning("File has not been initialised");

    return SRet_t::VALID;
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
