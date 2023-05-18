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

    void Reinit() noexcept {}
    void Dump() const {}

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

    // void Init(u8 number_bit_len, std::size_t size) {

    // }

    std::size_t Size() const noexcept {
        return m_size;
    }
    u8 NumBitLen() const noexcept {
        return m_bit_len;
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
    static chank_t Revert(chank_t word) noexcept {
        u8* bytes = (u8*)&word;
        for (u8 i = 0; i < sizeof(word) / 2; ++i) {
            std::swap(bytes[i], bytes[sizeof(word) - 1 - i]);
        }
        return word;
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
    BitVector(std::size_t size, uint8_t* buf)
        : m_size(size)
        , m_buf{buf}
        , m_sblk_bit_size{Log2Up(size) * Log2Up(size)}
        , m_blk_bit_size{Log2Up(size)}
        , m_sblk(size / m_sblk_bit_size)
        , m_blk{(u8)Log2Up(m_sblk_bit_size), size / Log2Up(size)} {
        Reinit();
    }

    void Reinit() noexcept {
        const auto num_sblk = m_sblk.size();
        const auto num_blk = m_blk.Size();

        assert(m_sblk_bit_size % m_blk_bit_size == 0);
        assert(m_sblk_bit_size / m_blk_bit_size == Log2Up(m_size));

        const auto num_blk_in_sblk = Log2Up(m_size);

        std::size_t full_num_bits = 0;
        std::size_t i_sb = 0, i_blk_in_sblk = 0, sblk_bit_ctr = 0;
        for (std::size_t i_blk = 0; i_blk < num_blk; ++i_blk) {
            for (std::size_t j = 0; j < m_blk_bit_size; ++j) {
                std::size_t bit_pos = j + i_blk * m_blk_bit_size;
                sblk_bit_ctr += Get(bit_pos);
            }

            m_blk.Set(i_blk, sblk_bit_ctr);

            if (++i_blk_in_sblk == num_blk_in_sblk) {
                i_blk_in_sblk = 0;

                full_num_bits += sblk_bit_ctr;
                m_sblk[i_sb++] = full_num_bits;
                sblk_bit_ctr = 0;
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
        std::size_t i_sblk = pos / m_sblk_bit_size;
        std::size_t in_sblk_pos = pos % m_sblk_bit_size;
        std::size_t i_blk = pos / m_blk_bit_size;

        std::size_t rank = 0;

        if (pos >= m_sblk_bit_size) {
            rank += m_sblk[i_sblk - 1];
        }

        if (in_sblk_pos >= m_blk_bit_size) {
            rank += m_blk.Get(i_blk - 1);
        }

        std::size_t j_begin = i_blk * m_blk_bit_size;
        std::size_t j_end = pos;
        for (std::size_t j = j_begin; j < j_end; ++j) {
            rank += Get(j);
        }

        return rank;
    }

    void Dump() const {
        printf("sblk bit size: %zu\n", m_sblk_bit_size);
        printf("blk bit size: %zu\n", m_blk_bit_size);
        printf("size: %zu\n", m_size);
        for (std::size_t i_sblk = 0; i_sblk < m_sblk.size(); ++i_sblk) {
            printf("sblk[%zu]: %zu\n", i_sblk, m_sblk[i_sblk]);
        }

        for (std::size_t i_blk = 0; i_blk < m_blk.Size(); ++i_blk) {
            printf("blk[%zu]: %u\n", i_blk, m_blk.Get(i_blk));
        }
    }

private:
    uint8_t* m_buf;
    std::size_t m_size;

    std::size_t m_sblk_bit_size;  // Num bits in 1 super block
    std::size_t m_blk_bit_size;   // Num bits in 1 block
    std::vector<std::size_t> m_sblk;
    CompressedNumberBuf m_blk;
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
