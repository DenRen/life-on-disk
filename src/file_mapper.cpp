#include "file_mapper.h"

#include <algorithm>
#include <stdexcept>
#include <iostream>

FileMapperRead::FileMapperRead(std::string_view path) {
    int fd = open(path.data(), O_RDONLY);
    if (fd == -1) {
        throw std::runtime_error("Failed to open file\n");
    }

    loff_t data_len = lseek64(fd, 0, SEEK_END);
    if (data_len == -1 || lseek64(fd, 0, SEEK_SET) == -1) {
        close(fd);
        throw std::runtime_error("Failed to get file size");
    }
    m_size = data_len;

    m_data = (const char*)mmap(NULL, m_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (m_data == MAP_FAILED) {
        perror("mmap");
        throw std::runtime_error("Failed to map file");
    }
}

FileMapperRead::~FileMapperRead() {
    munmap((void*)m_data, m_size);
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

FileMapperWrite::FileMapperWrite(std::string_view path, std::size_t size)
    : m_size{size} {
    int fd = open(path.data(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        throw std::runtime_error("Failed to open file\n");
    }

    if (ftruncate(fd, m_size) == -1) {
        close(fd);
        perror("ftruncate");
        throw std::runtime_error("Failed to truncate file\n");
    }

    m_data = (u8*)mmap(NULL, m_size, PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (m_data == MAP_FAILED) {
        perror("mmap");
        throw std::runtime_error("Failed to map file");
    }
}

FileMapperWrite::~FileMapperWrite() {
    munmap((void*)m_data, m_size);
}