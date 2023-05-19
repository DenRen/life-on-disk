#pragma once

#include <vector>
#include <cmath>

#include "../common/common_type.h"
#include "../common/help_func.h"
#include "compr_num_buf.hpp"

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

class alignas(8) BitVector {
public:
    using size_t = uint32_t;

    BitVector(size_t size)
        : m_size{size}
        , m_sblk_bit_size{CalcSblkBitSize(size)}
        , m_blk_bit_size{CalcBlkBitSize(size)}
        , m_sblk_size(size / m_sblk_bit_size) {
        auto sblk_size = size / m_sblk_bit_size * sizeof(size_t);
        auto buf_size = DivUp(size, 8);

        m_sblk_pos = AlignPos(sizeof(BitVector) + buf_size);
        m_blk_pos = AlignPos(m_sblk_pos + sblk_size);

        new (&BlkBuf()) CompressedNumberBuf{(u8)Log2Up(m_sblk_bit_size), size / Log2Up(size)};
    }

    static size_t CalcOccupiedSize(size_t size) noexcept {
        const auto sblk_bit_size = CalcSblkBitSize(size);
        const auto blk_bit_size = CalcBlkBitSize(size);

        auto buf_size = DivUp(size, 8);
        auto sblk_size = size / sblk_bit_size * sizeof(size_t);
        auto blk_size =
            CompressedNumberBuf::CalcOccupiedSize((u8)Log2Up(sblk_bit_size), size / Log2Up(size));

        auto sblk_pos = AlignPos(sizeof(BitVector) + buf_size);
        auto blk_pos = AlignPos(sblk_pos + sblk_size);

        return AlignPos(blk_pos + blk_size);
    }

    void Reinit() noexcept {
        size_t* sblk = SblkBuf();
        CompressedNumberBuf& blk = BlkBuf();

        const auto num_sblk = m_sblk_size;
        const auto num_blk = blk.Size();

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

            blk.Set(i_blk, sblk_bit_ctr);

            if (++i_blk_in_sblk == num_blk_in_sblk) {
                i_blk_in_sblk = 0;

                full_num_bits += sblk_bit_ctr;
                sblk[i_sb++] = full_num_bits;
                sblk_bit_ctr = 0;
            }
        }
    }

    size_t Size() const noexcept {
        return m_size;
    }

    bool Get(size_t pos) const noexcept {
        u8 byte = Buf()[pos / 8];
        u8 mask = 0b1000'0000 >> (pos % 8);
        return byte & mask;
    }
    void Set(size_t pos, bool value) noexcept {
        auto byte_pos = pos / 8;
        auto bit_pos = pos % 8;

        u8* buf = Buf();
        u8 byte = buf[byte_pos];
        u8 clear_mask = 0b1000'0000 >> bit_pos;
        u8 set_mask = u8{value} << (7u - bit_pos);

        byte &= ~clear_mask;
        byte |= set_mask;

        buf[byte_pos] = byte;
    }

    size_t GetRank(size_t pos) const noexcept {
        size_t i_sblk = pos / m_sblk_bit_size;
        size_t in_sblk_pos = pos % m_sblk_bit_size;
        size_t i_blk = pos / m_blk_bit_size;

        size_t rank = 0;

        if (pos >= m_sblk_bit_size) {
            rank += SblkBuf()[i_sblk - 1];
        }

        if (in_sblk_pos >= m_blk_bit_size) {
            rank += BlkBuf().Get(i_blk - 1);
        }

        // TODO: oprimize to popcount
        size_t j_begin = i_blk * m_blk_bit_size;
        size_t j_end = pos;
        for (size_t j = j_begin; j < j_end; ++j) {
            rank += Get(j);
        }

        return rank;
    }

    void Dump() const {
        // TODO: impl use iostream
        printf("sblk bit size: %u\n", m_sblk_bit_size);
        printf("blk bit size: %u\n", m_blk_bit_size);
        printf("size: %u\n", m_size);
        for (size_t i_sblk = 0; i_sblk < m_sblk_size; ++i_sblk) {
            printf("sblk[%u]: %u\n", i_sblk, SblkBuf()[i_sblk]);
        }

        const auto& blk = BlkBuf();
        for (size_t i_blk = 0; i_blk < blk.Size(); ++i_blk) {
            printf("blk[%u]: %u\n", i_blk, blk.Get(i_blk));
        }
    }

private:
    static size_t CalcSblkBitSize(size_t size) noexcept {
        return Log2Up(size) * Log2Up(size);
    }

    static size_t CalcBlkBitSize(size_t size) noexcept {
        return Log2Up(size);
    }

    u8* Buf() noexcept {
        return (u8*)(this + 1);
    }
    const u8* Buf() const noexcept {
        return (u8*)(this + 1);
    }

    size_t* SblkBuf() noexcept {
        return (size_t*)((u8*)this + m_sblk_pos);
    }
    const size_t* SblkBuf() const noexcept {
        return (const size_t*)((const u8*)this + m_sblk_pos);
    }

    CompressedNumberBuf& BlkBuf() noexcept {
        return *(CompressedNumberBuf*)((u8*)this + m_blk_pos);
    }
    const CompressedNumberBuf& BlkBuf() const noexcept {
        return *(const CompressedNumberBuf*)((const u8*)this + m_blk_pos);
    }

private:
    size_t m_size;
    size_t m_sblk_bit_size;  // Num bits in 1 super block
    size_t m_blk_bit_size;   // Num bits in 1 block

    size_t m_sblk_pos;
    size_t m_sblk_size;

    size_t m_blk_pos;
};

class BitVectorBuffer {
public:
    BitVectorBuffer(std::size_t size)
        : m_buf(BitVector::CalcOccupiedSize(size))
        , m_size{size} {}

    u8* Data() noexcept {
        return m_buf.data();
    }

    std::size_t Size() const noexcept {
        return m_size;
    }

private:
    std::vector<u8> m_buf;
    std::size_t m_size;
};
