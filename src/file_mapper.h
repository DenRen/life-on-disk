#pragma once

#include "common.h"

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

    const uint8_t* begin() const noexcept {
        return (const uint8_t*)m_data;
    }

    const uint8_t* end() const noexcept {
        return (const uint8_t*)m_data + m_size;
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

std::vector<str_len_t> BuildSortedSuff(std::string_view str);
