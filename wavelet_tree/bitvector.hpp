#pragma once

#include <vector>

class BitVectorNaive {
public:
    BitVectorNaive(std::size_t size)
        : m_buf(size) {}

    std::size_t Size() const noexcept {
        return m_buf.size();
    }

    bool Get(std::size_t pos) const noexcept {
        return m_buf[pos];
    }
    void Set(std::size_t pos, bool value) noexcept {
        m_buf[pos] = value;
    }

    std::size_t GetRank(std::size_t pos) const noexcept {
        std::size_t rank = 0;
        for (std::size_t i = 0; i < pos; ++i) {
            rank += Get(i);
        }
        return rank;
    }

private:
    std::vector<bool> m_buf;
};

class BitVector {
public:
    BitVector(std::size_t size, uint8_t* buf)
        : m_size(size)
        , m_buf{buf} {}

    std::size_t Size() const noexcept {
        return m_size;
    }

    bool Get(std::size_t pos) const noexcept {
        uint8_t byte = m_buf[pos / 8];
        uint8_t mask = 0b1000'0000 >> (pos % 8);
        return byte & mask;
    }
    void Set(std::size_t pos, bool value) noexcept {
        auto byte_pos = pos / 8;
        auto bit_pos = pos % 8;

        uint8_t byte = m_buf[byte_pos];
        uint8_t clear_mask = 0b1000'0000 >> bit_pos;
        uint8_t set_mask = uint8_t{value} << (7 - bit_pos);

        byte &= ~clear_mask;
        byte |= set_mask;

        m_buf[byte_pos] = byte;
    }

    std::size_t GetRank(std::size_t pos) const noexcept {
        std::size_t rank = 0;
        for (std::size_t i = 0; i < pos; ++i) {
            rank += Get(i);
        }
        return rank;
    }

private:
    uint8_t* m_buf;
    std::size_t m_size;
};

class BitVectorBuffer {
public:
    BitVectorBuffer(std::size_t size)
        : m_buf(size / 8 + !!(size % 8))
        , m_size{size} {}

    uint8_t* Data() noexcept {
        return m_buf.data();
    }

    std::size_t Size() const noexcept {
        return m_size;
    }

private:
    std::vector<uint8_t> m_buf;
    std::size_t m_size;
};
