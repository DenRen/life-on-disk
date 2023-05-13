#pragma once

#include "../common/common_type.h"
#include "../common/help_func.h"
#include "dna.h"

#include <array>
#include <cassert>
#include <string.h>

enum class DnaSymb : uint8_t { TERM = 0, A, C, T, G };
constexpr u8 DnaSymbBitSize = 3;

void InsertDnaSymb(uint8_t* begin, uint64_t dna_pos, DnaSymb dna_symb);
DnaSymb ReadDnaSymb(const uint8_t* begin, uint64_t dna_pos);

template <u8 N>
PACKED_CLASS DnaSymbSeq {
public:
    static_assert(N > 1 && DnaSymbBitSize * N <= 8 * sizeof(uint32_t));

    DnaSymbSeq() = default;

    DnaSymb operator[](u8 pos) const noexcept {
        assert(pos < N);
        return ReadDnaSymb(m_buf.data(), pos);
    }

    void Set(DnaSymb dna_symb, u8 dna_pos) noexcept {
        assert(dna_pos < N);
        InsertDnaSymb(m_buf.data(), dna_pos, dna_symb);
    }

    void Reverse() noexcept {
        auto& seq = *this;
        for (u8 i = 0; i < N / 2; ++i) {
            auto lhs = seq[i];
            auto rhs = seq[N - 1 - i];
            Set(lhs, N - 1 - i);
            Set(rhs, i);
        }
    }

    bool operator<(const DnaSymbSeq<N>& rhs) const noexcept {
        return memcmp(m_buf.data(), rhs.m_buf.data(), m_buf.size()) < 0;
    }

    bool operator>(const DnaSymbSeq<N>& rhs) const noexcept {
        return memcmp(m_buf.data(), rhs.m_buf.data(), m_buf.size()) > 0;
    }

    bool operator==(const DnaSymbSeq<N>& rhs) const noexcept {
        return memcmp(m_buf.data(), rhs.m_buf.data(), m_buf.size()) == 0;
    }

    bool operator!=(const DnaSymbSeq<N>& rhs) const noexcept {
        return memcmp(m_buf.data(), rhs.m_buf.data(), m_buf.size()) != 0;
    }

private:
    constexpr static uint c_bit_size = DnaSymbBitSize * N;
    constexpr static uint c_byte_size = DivUp(c_bit_size, 8u);
    std::array<u8, c_byte_size> m_buf = {};
};

using char_t = DnaSymb;
using alph_size_t = u8;

using str_len_t = uint32_t;

using in_blk_pos_t = uint16_t;
using blk_pos_t = uint32_t;
using str_pos_t = str_len_t;
