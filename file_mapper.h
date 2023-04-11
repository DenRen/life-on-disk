#pragma once

#include <string_view>
#include <vector>

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

class FileMapper {
public:
    FileMapper(std::string_view path);
    ~FileMapper();
    std::string_view GetData() const noexcept {
        return {m_data, m_size};
    }

private:
    const char* m_data;
    uint64_t m_size;
};

std::vector<uint64_t> BuildSortedSuff(std::string_view str);