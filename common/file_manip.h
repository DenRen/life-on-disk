#pragma once

#include "common_type.h"

#include <string_view>
#include <vector>

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

class FileDescGuard {
public:
    explicit FileDescGuard(int fd) noexcept
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

FileDescGuard OpenFile(std::string_view path, int flags);
FileDescGuard OpenFile(std::string_view path, int flags, int opts);
void* MapFile(const FileDescGuard& fd_guard, size_t size, int prot, int flags);
uint64_t GetFileSizeAndSetToBegin(const FileDescGuard& fd_guard);
uint64_t Lseek64(const FileDescGuard& fd_guard, uint64_t offset, int whence);
void TruncateFile(const FileDescGuard& fd_guard, uint64_t new_size);
void MakeZeroTerminated(std::string_view text_path);

class FileMapperRead {
public:
    FileMapperRead(std::string_view path);
    ~FileMapperRead();

    std::string_view GetData() const noexcept {
        return {m_data, m_size};
    }

    const u8* begin() const noexcept {
        return (const u8*)m_data;
    }

    const u8* end() const noexcept {
        return (const u8*)m_data + m_size;
    }

    uint64_t Size() const noexcept {
        return m_size;
    }

private:
    const char* m_data;
    uint64_t m_size;
};

class FileMapperWrite {
public:
    FileMapperWrite(std::string_view path, std::size_t size);
    ~FileMapperWrite();

    std::pair<u8*, std::size_t> GetData() const noexcept {
        return {m_data, m_size};
    }

    u8* begin() noexcept {
        return m_data;
    }

    void Truncate(uint64_t new_size);

private:
    u8* m_data;
    uint64_t m_size;
    FileDescGuard m_fd;
};
