#include "diskdriver.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cassert>
#include <cstdint>

//~ static symbol link
std::shared_ptr<graphquery::logger::CLogSystem> graphquery::database::storage::CDiskDriver::m_log_system;

graphquery::database::storage::CDiskDriver::CDiskDriver(const int file_mode, int map_mode_prot, int map_mode_flags)
{
    this->m_log_system            = logger::CLogSystem::GetInstance();
    this->m_file_mode             = file_mode;
    this->m_map_mode_prot         = map_mode_prot;
    this->m_map_mode_flags        = map_mode_flags;
    this->m_current_bytes_written = 0;
}

graphquery::database::storage::CDiskDriver::~CDiskDriver()
{
    if (this->m_initialised)
    {
        sync();
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
    assert(unmap() == SRet_t::VALID);

    truncate(file_size);
    map();
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
    this->m_memory_mapped_file = static_cast<char *>(mmap(nullptr, m_fd_info.st_size, m_map_mode_prot, m_map_mode_flags, m_file_descriptor, 0));
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
    if (munmap(this->m_memory_mapped_file, this->m_fd_info.st_size) == -1)
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
graphquery::database::storage::CDiskDriver::check_if_file_exists(std::string_view file_path) noexcept
{
    if (access(file_path.cbegin(), F_OK) == 0)
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
    if (check_if_file_exists(file_name))
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
        if (msync(m_memory_mapped_file, m_fd_info.st_size, MS_SYNC) == -1)
        {
            m_log_system->warning("Issue syncing the buffer");
            return SRet_t::ERROR;
        }

        m_current_bytes_written ^= m_current_bytes_written;
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
        this->m_path.remove_filename();
        return SRet_t::VALID;
    }

    m_log_system->warning("File has not been initialised");
    return SRet_t::ERROR;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::read(void * ptr, const int64_t size, const uint32_t amt) const
{
    if (this->m_initialised)
    {
        memcpy(ptr, &this->m_memory_mapped_file[this->m_seek_offset], size * amt);
    }
    else
        m_log_system->warning("File has not been initialised");
    return SRet_t::VALID;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::write(const void * ptr, const int64_t size, const uint32_t amt)
{
    if (this->m_initialised)
    {
        if (m_current_bytes_written >= MAX_WRITE_SIZE)
            sync();

        if (m_fd_info.st_size < (size * amt + m_seek_offset))
        {
            static constexpr uint8_t scale = 10;
            resize(m_fd_info.st_size + (size * amt * scale));
        }
        memcpy(&this->m_memory_mapped_file[this->m_seek_offset], ptr, size * amt);
        m_current_bytes_written += size * amt;
    }
    else
        m_log_system->warning("File has not been initialised");

    return SRet_t::VALID;
}

char
graphquery::database::storage::CDiskDriver::operator[](const int64_t idx)
{
    char ret = {};
    if (this->m_initialised)
    {
        assert(idx >= 0L && idx <= this->m_fd_info.st_size);

        if (m_current_bytes_written >= MAX_WRITE_SIZE)
            sync();

        ret = this->m_memory_mapped_file[idx];
        m_current_bytes_written += 1;
    }
    else
        m_log_system->warning("File has not been initialised");

    return ret;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::seek(int64_t offset)
{
    if (this->m_initialised)
    {
        assert(offset >= 0L && offset <= this->m_fd_info.st_size);
        this->m_seek_offset = offset;
    }
    else
        m_log_system->warning("File has not been initialised");

    return SRet_t::VALID;
}

bool
graphquery::database::storage::CDiskDriver::check_if_initialised() const noexcept
{
    return this->m_initialised;
}
