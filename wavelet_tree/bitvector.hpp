#pragma once

#include <vector>

#include "../common/common_type.h"
#include "../common/help_func.h"

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

class CompressedNumberBuf {
public:
    using chank_t = uint32_t;

    CompressedNumberBuf(u8 number_bit_len, std::size_t size)
        : m_buf(sizeof(chank_t) + DivUp(number_bit_len * size, 8))
        , m_size{size}
        , m_bit_len{number_bit_len} {
        if (!(m_bit_len > 8 && m_bit_len < 16)) {
            throw std::invalid_argument{"Incorrect bit length"};
        }
    }

    std::size_t Size() const noexcept {
        return m_size;
    }

    static chank_t Revert(chank_t word) noexcept {
        u8* bytes = (u8*)&word;
        for (u8 i = 0; i < sizeof(word) / 2; ++i) {
            std::swap(bytes[i], bytes[sizeof(word) - 1 - i]);
        }
        return word;
    }

    chank_t Get(std::size_t pos) const noexcept {
        auto bit_pos = m_bit_len * pos;
        auto byte_pos = bit_pos / 8;
        chank_t word = misalign_load<chank_t>(m_buf.data() + byte_pos);
        word = Revert(word);

        auto l_shift = bit_pos % 8;
        auto r_shift = 8 * sizeof(chank_t) - l_shift - m_bit_len;
        word = word << l_shift;
        word = word >> (l_shift + r_shift);
        return word;
    }

    void Set(std::size_t pos, chank_t value) noexcept {
        auto bit_pos = m_bit_len * pos;
        auto byte_pos = bit_pos / 8;
        chank_t word = misalign_load<chank_t>(m_buf.data() + byte_pos);
        word = Revert(word);

        auto l_shift = bit_pos % 8;
        auto r_shift = 8 * sizeof(chank_t) - l_shift - m_bit_len;

        chank_t mask = (1u << m_bit_len) - 1;
        mask = mask << r_shift;

        word &= ~mask;
        word |= (value << r_shift);
        word = Revert(word);

        misalign_store<chank_t>(m_buf.data() + byte_pos, word);
    }

    void Dump() const noexcept {
        for (std::size_t i = 0; i < m_size * m_bit_len; ++i) {
            u8 byte = m_buf[i / 8];
            u8 bit = !!(byte & (0b1000'0000 >> (i % 8)));
            if (i && !(i % m_bit_len)) {
                putchar('\'');
            }
            printf("%hhu", bit);
        }
        putchar('\n');
    }

private:
    std::vector<uint8_t> m_buf;
    std::size_t m_size;
    u8 m_bit_len;
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

    // std::vector<std::size_t> m_superblock;
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
