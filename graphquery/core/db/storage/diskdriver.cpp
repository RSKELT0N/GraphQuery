#include "db/storage/diskdriver.h"

#include "db/system.h"
#include "fmt/format.h"

#include <sys/mman.h>
#include <fcntl.h>
#include <cassert>
#include <unistd.h>

graphquery::database::storage::CDiskDriver::~CDiskDriver()
{
    if(this->m_initialised)
    {
        Close();
        this->m_initialised = false;
    }
}

bool
graphquery::database::storage::CDiskDriver::CheckIfFileExists(std::string_view file_path) const noexcept
{
    if(access(file_path.cbegin(), F_OK) == -1)
    {
        return false;
    }
    return true;
}

bool
graphquery::database::storage::CDiskDriver::CheckIfInitialised() const noexcept
{
    return this->m_initialised;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::Create(std::string_view file_path, size_t file_size)
{
    if(CheckIfFileExists(file_path))
    {
        _log_system->Warning("Cannot create file that already exists");
        return SRet_t::ERROR;
    }

    this->m_file_descriptor = open(file_path.cbegin(), O_CREAT | O_TRUNC | PROT_WRITE, S_IWRITE | S_IWUSR | S_IRUSR);

    if(this->m_file_descriptor == -1)
    {
        _log_system->Error(fmt::format("File descriptor ({}) could not be created to create file", file_path));
        return SRet_t::ERROR;
    }

    if(ftruncate(this->m_file_descriptor, file_size) == -1)
    {
        _log_system->Warning(fmt::format("Issue truncating the file to the specified size {}", errno));
        close(this->m_file_descriptor);
        return SRet_t::ERROR;
    }

    close(this->m_file_descriptor);
    return SRet_t::VALID;

}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::Open(std::string_view file_path, int file_mode, int map_mode_prot, int map_mode_flags)
{
    if(!CheckIfFileExists(file_path))
    {
        _log_system->Warning("Could not open a file that doesn't exist");
        return SRet_t::ERROR;
    }

    this->m_file_descriptor = open(file_path.cbegin(), file_mode);

    if(this->m_file_descriptor == -1)
    {
        _log_system->Error(fmt::format("File descriptor ({}) could not be created to open file", errno));
        return SRet_t::ERROR;
    }

    if(fstat(this->m_file_descriptor, &this->m_fd_info) == -1)
    {
        _log_system->Error(fmt::format("Issue getting file descriptor ({}) info", file_path));
        return SRet_t::ERROR;
    }

    this->m_memory_mapped_file = static_cast<char*>(mmap(nullptr, this->m_fd_info.st_size, map_mode_prot, map_mode_flags, this->m_file_descriptor, 0));
    if (this->m_memory_mapped_file == MAP_FAILED) {
        _log_system->Error(fmt::format("Error mapping file to memory"));
        return SRet_t::ERROR;
    }
    this->m_initialised = true;

    return SRet_t::VALID;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::Close()
{
    if(this->m_initialised)
    {
        if (munmap(this->m_memory_mapped_file, this->m_fd_info.st_size) == -1) {
            _log_system->Error(fmt::format("Error unmapping file from memory"));
            return SRet_t::ERROR;
        }
        close(m_file_descriptor);
        this->m_initialised = false;
    } else _log_system->Warning("File has not been initialised");

    return SRet_t::VALID;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::Read(void *ptr, size_t size, uint32_t amt) const
{
    if(this->m_initialised)
    {
        memcpy(ptr, &this->m_memory_mapped_file[this->m_seek_offset], size * amt);
    } else _log_system->Warning("File has not been initialised");
    return SRet_t::VALID;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::Write(const void *ptr, size_t size, uint32_t amt)
{
    if(this->m_initialised)
    {
        memcpy(&this->m_memory_mapped_file[this->m_seek_offset], ptr, size * amt);
    } else _log_system->Warning("File has not been initialised");
    return SRet_t::VALID;
}

char
graphquery::database::storage::CDiskDriver::operator[](int64_t idx) const
{
    char ret = {};
    if(this->m_initialised)
    {
        assert(idx >= 0L && idx <= this->m_fd_info.st_size);
        ret = this->m_memory_mapped_file[idx];
    } else _log_system->Warning("File has not been initialised");

    return ret;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::Seek(int64_t offset)
{
    if(this->m_initialised)
    {
        assert(offset >= 0L && offset <= this->m_fd_info.st_size);
        this->m_seek_offset = offset;
    } else _log_system->Warning("File has not been initialised");

    return SRet_t::VALID;
}
