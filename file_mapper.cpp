#include "file_mapper.h"

#include <algorithm>
#include <stdexcept>

FileMapper::FileMapper(std::string_view path) {
    int fd = open(path.data(), O_RDONLY);
    if (fd == -1) {
        std::runtime_error("Failed to open file\n");
    }

    loff_t data_len = lseek64(fd, 0, SEEK_END);
    if (data_len == -1 || lseek64(fd, 0, SEEK_SET) == -1) {
        close(fd);
        std::runtime_error("Failed to get file size");
    }
    m_size = data_len + 1;  // For zero terminated

    m_data = (const char*)mmap(NULL, m_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (m_data == NULL) {
        std::runtime_error("Failed to map file");
    }
}

FileMapper::~FileMapper() {
    munmap((void*)m_data, m_size);
}

std::vector<uint64_t> BuildSortedSuff(std::string_view str) {
    std::vector<uint64_t> suff;
    suff.resize(str.size());
    for (uint64_t pos = 0; pos < suff.size(); ++pos) {
        suff[pos] = pos;
    }

    std::sort(std::begin(suff), std::end(suff), [&str](auto lhs, auto rhs) noexcept {
        std::string_view lhs_sv{str.data() + lhs, str.size() - lhs};
        std::string_view rhs_sv{str.data() + rhs, str.size() - rhs};
        return lhs_sv < rhs_sv;
    });

    return suff;
}
