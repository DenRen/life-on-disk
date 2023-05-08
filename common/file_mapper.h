#pragma once

#include "common_type.h"

#include <string_view>
#include <vector>

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

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
    int m_fd;
};

