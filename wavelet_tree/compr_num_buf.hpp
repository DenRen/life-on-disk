#pragma once

#include <cstdio>
#include <stdexcept>

#include "../common/common_type.h"
#include "../common/help_func.h"

class CompressedNumberBuf {
public:
    using chank_t = uint32_t;

    CompressedNumberBuf(u8 number_bit_len, std::size_t size)
        : m_bit_len{GetAlignedNumberBitLen(number_bit_len)}
        , m_size{size} {}

    std::size_t Size() const noexcept {
        return m_size;
    }
    u8 NumBitLen() const noexcept {
        return m_bit_len;
    }

    chank_t Get(std::size_t pos) const noexcept {
        auto bit_pos = m_bit_len * pos;
        auto byte_pos = bit_pos / 8;
        chank_t word = misalign_load<chank_t>(Data() + byte_pos);
        word = Revert(word);

        auto l_shift = bit_pos % 8;
        auto r_shift = 8 * sizeof(chank_t) - l_shift - m_bit_len;
        word <<= l_shift;
        word >>= (l_shift + r_shift);
        return word;
    }

    void Set(std::size_t pos, chank_t value) noexcept {
        auto bit_pos = m_bit_len * pos;
        auto byte_pos = bit_pos / 8;
        chank_t word = misalign_load<chank_t>(Data() + byte_pos);
        word = Revert(word);

        auto l_shift = bit_pos % 8;
        auto r_shift = 8 * sizeof(chank_t) - l_shift - m_bit_len;

        chank_t mask = (1u << m_bit_len) - 1;
        mask <<= r_shift;

        word &= ~mask;
        word |= (value << r_shift);
        word = Revert(word);

        misalign_store<chank_t>(Data() + byte_pos, word);
    }

    void Dump() const noexcept {
        for (std::size_t i = 0; i < m_size * m_bit_len; ++i) {
            u8 byte = Data()[i / 8];
            u8 bit = !!(byte & (0b1000'0000 >> (i % 8)));
            if (i && !(i % m_bit_len)) {
                putchar('\'');
            }
            printf("%hhu", bit);
        }
        putchar('\n');
    }

    static std::size_t CalcOccupiedSize(u8 number_bit_len, std::size_t size) noexcept {
        const auto buf_size = CalcBufSize(GetAlignedNumberBitLen(number_bit_len), size);
        return sizeof(CompressedNumberBuf) + buf_size;
    }

private:
    static chank_t Revert(chank_t word) noexcept {
        u8* bytes = (u8*)&word;
        for (u8 i = 0; i < sizeof(word) / 2; ++i) {
            std::swap(bytes[i], bytes[sizeof(word) - 1 - i]);
        }
        return word;
    }

    static u8 GetAlignedNumberBitLen(u8 number_bit_len) {
        constexpr u8 min_size = 9;
        constexpr u8 max_size = 15;

        if (number_bit_len > max_size) {
            throw std::invalid_argument{"number_bit_len is too large!"};
        }

        return std::max(min_size, number_bit_len);
    }

    static std::size_t CalcBufSize(u8 aligned_number_bit_len, std::size_t size) {
        return sizeof(chank_t) + DivUp(aligned_number_bit_len * size, 8);
    }

    const u8* Data() const noexcept {
        return (const u8*)(this + 1);
    }

    u8* Data() noexcept {
        return (u8*)(this + 1);
    }

private:
    u8 m_bit_len;
    std::size_t m_size;
};
