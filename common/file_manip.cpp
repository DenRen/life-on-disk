#include "file_manip.h"
#include "../common/help_func.h"

#include <algorithm>
#include <stdexcept>
#include <iostream>

FileDescGuard OpenFile(std::string_view path, int flags) {
    int fd = open64(path.data(), flags);
    if (fd == -1) {
        perror("open64");
        throw std::runtime_error("Failed to open file: " + std::string{path});
    }
    return FileDescGuard{fd};
}

FileDescGuard OpenFile(std::string_view path, int flags, int opts) {
    int fd = open64(path.data(), flags, opts);
    if (fd == -1) {
        perror("open64");
        throw std::runtime_error("Failed to open file: " + std::string{path});
    }
    return FileDescGuard{fd};
}

void* MapFile(const FileDescGuard& fd_guard, size_t size, int prot, int flags) {
    void* data = mmap64(NULL, size, prot, flags, fd_guard.Get(), 0);
    if (data == MAP_FAILED) {
        perror("mmap");
        throw std::runtime_error("Failed to map file");
    }
    return data;
}

uint64_t GetFileSizeAndSetToBegin(const FileDescGuard& fd_guard) {
    loff_t file_size = lseek64(fd_guard.Get(), 0, SEEK_END);
    if (file_size == -1 || lseek64(fd_guard.Get(), 0, SEEK_SET) == -1) {
        throw std::runtime_error("Failed to get file size");
    }
    return (uint64_t)file_size;
}

uint64_t Lseek64(const FileDescGuard& fd_guard, uint64_t offset, int whence) {
    loff_t res = lseek64(fd_guard.Get(), offset, whence);
    if (res == -1) {
        perror("lseek64");
        throw std::runtime_error{"Failed to get file size"};
    }

    return (uint64_t)res;
}

void TruncateFile(const FileDescGuard& fd_guard, uint64_t new_size) {
    if (ftruncate64(fd_guard.Get(), new_size) == -1) {
        perror("ftruncate");
        throw std::runtime_error("Failed to truncate file\n");
    }
}

void MakeZeroTerminated(std::string_view text_path) {
    FileDescGuard fd_guard = OpenFile(text_path, O_RDWR);

    uint64_t file_size = GetFileSizeAndSetToBegin(fd_guard);

    char last_char = 0;
    if (read(fd_guard.Get(), &last_char, 1) != 1) {
        throw std::runtime_error{"Read last symb failed"};
    }

    if (last_char != '\0') {
        TruncateFile(fd_guard, file_size + 1);

        last_char = '\0';
        if (write(fd_guard.Get(), &last_char, 1) != 1) {
            throw std::runtime_error{"Failed to write zero terminated byte"};
        }
    }
}

FileMapperRead::FileMapperRead(std::string_view path) {
    FileDescGuard fd_guard = OpenFile(path, O_RDONLY);
    m_size = GetFileSizeAndSetToBegin(fd_guard);
    m_data = (const char*)MapFile(fd_guard, m_size, PROT_READ, MAP_PRIVATE);
    fd_guard.Release();
}

FileMapperRead::~FileMapperRead() {
    munmap((void*)m_data, m_size);
}

FileMapperWrite::FileMapperWrite(std::string_view path, std::size_t size)
    : m_size{size}
    , m_fd{OpenFile(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)} {
    TruncateFile(m_fd, m_size);
    m_data = (u8*)MapFile(m_fd, m_size, PROT_WRITE, MAP_SHARED);
}

FileMapperWrite::~FileMapperWrite() {
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
