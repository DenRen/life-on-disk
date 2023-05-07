#include "file_mapper.h"

#include <algorithm>
#include <stdexcept>
#include <iostream>

class FileDescGuard {
public:
    FileDescGuard(int fd) noexcept
        : m_fd{fd} {}

    ~FileDescGuard() {
        if (m_fd >= 0) {
            close(m_fd);
        }
    }

    int Get() const noexcept {
        return m_fd;
    }

    void Release() noexcept {
        m_fd = -1;
    }

private:
    int m_fd;
};

static FileDescGuard OpenFile(std::string_view path, int flags) {
    int fd = open64(path.data(), flags);
    if (fd == -1) {
        throw std::runtime_error("Failed to open file\n");
    }
    return fd;
}

static FileDescGuard OpenFile(std::string_view path, int flags, int opts) {
    int fd = open64(path.data(), flags, opts);
    if (fd == -1) {
        throw std::runtime_error("Failed to open file\n");
    }
    return fd;
}

static void* MapFile(int fd, size_t size, int prot, int flags) {
    void* data = mmap64(NULL, size, prot, flags, fd, 0);
    if (data == MAP_FAILED) {
        perror("mmap");
        throw std::runtime_error("Failed to map file");
    }
    return data;
}

static uint64_t GetFileSizeAndSetToBegin(int fd) {
    loff_t file_size = lseek64(fd, 0, SEEK_END);
    if (file_size == -1 || lseek64(fd, 0, SEEK_SET) == -1) {
        throw std::runtime_error("Failed to get file size");
    }
    return (uint64_t)file_size;
}

static void TruncateFile(int fd, uint64_t new_size) {
    if (ftruncate64(fd, new_size) == -1) {
        perror("ftruncate");
        throw std::runtime_error("Failed to truncate file\n");
    }
}

FileMapperRead::FileMapperRead(std::string_view path) {
    FileDescGuard fd_guard = OpenFile(path, O_RDONLY);
    m_size = GetFileSizeAndSetToBegin(fd_guard.Get());
    m_data = (const char*)MapFile(fd_guard.Get(), m_size, PROT_READ, MAP_PRIVATE);
    fd_guard.Release();
}

FileMapperRead::~FileMapperRead() {
    munmap((void*)m_data, m_size);
}

FileMapperWrite::FileMapperWrite(std::string_view path, std::size_t size)
    : m_size{size} {
    FileDescGuard fd_guard = OpenFile(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    m_fd = fd_guard.Get();

    TruncateFile(m_fd, m_size);
    m_data = (u8*)MapFile(m_fd, m_size, PROT_WRITE, MAP_SHARED);

    fd_guard.Release();
}

FileMapperWrite::~FileMapperWrite() {
    close(m_fd);
    if (m_data != nullptr) {
        munmap((void*)m_data, m_size);
    }
}

void FileMapperWrite::Truncate(uint64_t new_size) {
    munmap((void*)m_data, m_size);
    m_data = nullptr;

    TruncateFile(m_fd, new_size);

    m_size = new_size;
    m_data = (u8*)MapFile(m_fd, m_size, PROT_WRITE, MAP_SHARED);
}

std::vector<str_pos_t> BuildSortedSuff(std::string_view str) {
    std::vector<str_pos_t> suff;
    suff.resize(str.size());
    for (str_pos_t pos = 0; pos < suff.size(); ++pos) {
        suff[pos] = pos;
    }

    std::sort(std::begin(suff), std::end(suff), [&str](auto lhs, auto rhs) noexcept {
        std::string_view lhs_sv{str.data() + lhs, str.size() - lhs};
        std::string_view rhs_sv{str.data() + rhs, str.size() - rhs};
        return lhs_sv < rhs_sv;
    });

    return suff;
}
