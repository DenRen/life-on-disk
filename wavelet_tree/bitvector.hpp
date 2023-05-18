#pragma once

#include <bit>
#include <vector>
#include <cmath>

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
        : m_bit_len{std::max<u8>(9, std::min<u8>(number_bit_len, 15))}
        , m_size{size}
        , m_buf(sizeof(chank_t) + DivUp(m_bit_len * size, 8)) {}

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
        word <<= l_shift;
        word >>= (l_shift + r_shift);
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
        mask <<= r_shift;

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
    u8 m_bit_len;
    std::vector<uint8_t> m_buf;
    std::size_t m_size;
};

constexpr std::size_t Log2Up(std::size_t value) noexcept {
    return 8 * sizeof(value) - std::countl_zero(value);
}

class BitVector {
public:
    static std::size_t CalcSuperblockBitSize(std::size_t bv_size) noexcept {
        return std::pow(Log2Up(bv_size), 2);
    }

    static std::size_t CalcNumSuperblock(std::size_t bv_size) noexcept {
        return bv_size / CalcSuperblockBitSize(bv_size);
    }

    static std::size_t CalcBlockBitSize(std::size_t bv_size) noexcept {
        return Log2Up(bv_size);
    }

    static std::size_t CalcNumBlock(std::size_t bv_size) noexcept {
        return bv_size / Log2Up(bv_size);
    }

    BitVector(std::size_t size, uint8_t* buf)
        : m_size(size)
        , m_buf{buf}
        , m_superblocks(CalcNumSuperblock(size))
        , m_blocks{(u8)Log2Up(CalcSuperblockBitSize(size)), CalcNumBlock(size)} {
        const auto num_sblock = m_superblocks.size();
        const auto num_blocks = m_blocks.Size();

        const auto blk_num_bits = CalcBlockBitSize(m_size);
        const auto sblk_num_bits = CalcSuperblockBitSize(m_size);
        assert(sblk_num_bits % blk_num_bits == 0);

        const auto num_blk_in_sblk = sblk_num_bits / blk_num_bits;

        std::size_t full_num_bits = 0;
        std::size_t i_sb = 0, i_blk_in_sblk = 0;
        for (std::size_t i_blk = 0; i_blk < num_blocks; ++i_blk) {
            std::size_t blk_bit_ctr = 0;
            for (std::size_t j = 0; j < blk_num_bits; ++j) {
                std::size_t bit_pos = j + i_blk * blk_num_bits;
                blk_bit_ctr += Get(bit_pos);
            }

            m_blocks.Set(i_blk, blk_bit_ctr);

            full_num_bits += blk_bit_ctr;
            if (++i_blk_in_sblk == num_blk_in_sblk) {
                i_blk_in_sblk = 0;
                m_superblocks[i_sb++] = full_num_bits;
            }
        }
    }

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

    std::vector<std::size_t> m_superblocks;
    CompressedNumberBuf m_blocks;
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
