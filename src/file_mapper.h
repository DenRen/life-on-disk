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

private:
    u8* m_data;
    uint64_t m_size;
};

std::vector<str_len_t> BuildSortedSuff(std::string_view str);
