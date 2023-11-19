#include "diskdriver.h"

graphquery::database::storage::CDiskDriver::CDiskDriver(int file_mode, int map_mode_prot, int map_mode_flags)
{
    this->m_log_system = logger::CLogSystem::GetInstance();
    this->m_file_mode = file_mode;
    this->m_map_mode_prot = map_mode_prot;
    this->m_map_mode_flags = map_mode_flags;
}

graphquery::database::storage::CDiskDriver::~CDiskDriver()
{
    if(this->m_initialised)
    {
        Close();
        this->m_initialised = false;
    }
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::OpenFD() noexcept
{
    this->m_file_descriptor = open(m_file_path.c_str(), m_file_mode);

    if(this->m_file_descriptor == -1)
    {
        m_log_system->Error(fmt::format("File descriptor ({}) could not be created to open file (Error: {})", m_file_path, errno));
        return SRet_t::ERROR;
    }

    if(fstat(this->m_file_descriptor, &this->m_fd_info) == -1)
    {
        m_log_system->Error(fmt::format("Issue getting file descriptor ({}) info", m_file_path));
        return SRet_t::ERROR;
    }
    return SRet_t::VALID;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::CloseFD() noexcept
{
    if(close(this->m_file_descriptor) == -1)
    {
        m_log_system->Error(fmt::format("Issue closing file descriptor ({}) (Error: {})", m_file_path, errno));
        return SRet_t::ERROR;
    }
    return SRet_t::VALID;
}

void
graphquery::database::storage::CDiskDriver::Resize(int64_t new_file_size) noexcept
{
    Unmap();
    Truncate(new_file_size);
    Map();
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::Truncate(int64_t file_size) noexcept
{
    if(this->m_file_descriptor == -1)
    {
        m_log_system->Error(fmt::format("File descriptor ({}) could not be created to create file", m_file_path));
        return SRet_t::ERROR;
    }

    if(ftruncate(this->m_file_descriptor, file_size) == -1)
    {
        m_log_system->Warning(fmt::format("Issue truncating the file to the specified size {}", errno));
        close(this->m_file_descriptor);
        return SRet_t::ERROR;
    }

    if(fstat(this->m_file_descriptor, &this->m_fd_info) == -1)
    {
        m_log_system->Error(fmt::format("Issue getting file descriptor ({}) info", m_file_path));
        return SRet_t::ERROR;
    }

    return SRet_t::VALID;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::Map() noexcept
{
    this->m_memory_mapped_file = static_cast<char*>(mmap(nullptr, m_fd_info.st_size, m_map_mode_prot, m_map_mode_flags, m_file_descriptor, 0));
    if (this->m_memory_mapped_file == MAP_FAILED) {
        m_log_system->Error(fmt::format("Error mapping file to memory"));
        return SRet_t::ERROR;
    }

    return SRet_t::VALID;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::Unmap() noexcept
{
    if (munmap(this->m_memory_mapped_file, this->m_fd_info.st_size) == -1) {
        m_log_system->Error(fmt::format("Error unmapping file from memory"));
        return SRet_t::ERROR;
    }

    return SRet_t::VALID;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::CreateFile(int64_t file_size) noexcept
{
    this->m_file_descriptor = open(m_file_path.c_str(), O_CREAT | O_TRUNC | PROT_WRITE, S_IWRITE | S_IWUSR | S_IRUSR);

    if(this->m_file_descriptor == -1)
    {
        m_log_system->Error(fmt::format("File descriptor ({}) could not be created to create file", m_file_path));
        return SRet_t::ERROR;
    }

    if(ftruncate(this->m_file_descriptor, file_size) == -1)
    {
        m_log_system->Warning(fmt::format("Issue truncating the file to the specified size {}", errno));
        close(this->m_file_descriptor);
        return SRet_t::ERROR;
    }

    close(this->m_file_descriptor);
    return SRet_t::VALID;
}


void
graphquery::database::storage::CDiskDriver::SetFilePath(std::string file_path) noexcept
{
    this->m_file_path = std::move(file_path);
}

const std::string &
graphquery::database::storage::CDiskDriver::GetFilePath() const noexcept
{
    return m_file_path;
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
graphquery::database::storage::CDiskDriver::Create(int64_t file_size)
{
    if(CheckIfFileExists(m_file_path))
    {
        m_log_system->Warning("Cannot create file that already exists");
        return SRet_t::ERROR;
    }

    CreateFile(file_size);
    return SRet_t::VALID;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::Open()
{
    if(!CheckIfFileExists(m_file_path))
    {
        m_log_system->Warning("Could not open a file that doesn't exist");
        return SRet_t::ERROR;
    }

    OpenFD();
    Map();
    this->m_initialised = true;

    return SRet_t::VALID;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::Close()
{
    if(this->m_initialised)
    {
        Unmap();
        CloseFD();
        this->m_initialised = false;
    } else m_log_system->Warning("File has not been initialised");

    return SRet_t::VALID;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::Read(void *ptr, const int64_t size, const uint32_t amt) const
{
    if(this->m_initialised)
    {
        memcpy(ptr, &this->m_memory_mapped_file[this->m_seek_offset], size * amt);
    } else m_log_system->Warning("File has not been initialised");
    return SRet_t::VALID;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::Write(const void *ptr, const int64_t size, const uint32_t amt)
{
    if(this->m_initialised)
    {
        if(m_fd_info.st_size < (size * amt + m_seek_offset))
        {
            static constexpr uint8_t scale = 10;
            Resize(m_fd_info.st_size + (size * amt * scale));
        }
        memcpy(&this->m_memory_mapped_file[this->m_seek_offset], ptr, size * amt);
    } else m_log_system->Warning("File has not been initialised");

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
    } else m_log_system->Warning("File has not been initialised");

    return ret;
}

graphquery::database::storage::CDiskDriver::SRet_t
graphquery::database::storage::CDiskDriver::Seek(int64_t offset)
{
    if(this->m_initialised)
    {
        assert(offset >= 0L && offset <= this->m_fd_info.st_size);
        this->m_seek_offset = offset;
    } else m_log_system->Warning("File has not been initialised");

    return SRet_t::VALID;
}
