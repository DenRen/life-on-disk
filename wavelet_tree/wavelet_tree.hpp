#pragma once

#include "bitvector.hpp"

// 110 -> 011
template <typename U>
U reverse(U val, std::size_t bit_len) {
    size_t rev = 0;
    for (size_t i = 0; i < bit_len; ++i) {
        rev <<= 1;
        rev |= val & 1u;
        val >>= 1;
    }
    return rev;
}

class WaveletTreeNaive {
public:
    using size_t = uint32_t;

    template <typename NumberAccesstorT>
    WaveletTreeNaive(const NumberAccesstorT& text) {
        const auto text_size = text.Size();
        m_buf.resize(text_size);
        for (size_t i = 0; i < text_size; ++i) {
            m_buf[i] = text[i];
        }
    }

    size_t GetRank(size_t val, size_t pos) const {
        if (pos > m_buf.size()) {
            throw std::invalid_argument{"WT_Naive: pos is too large"};
        }

        size_t rank = 0;
        for (size_t i = 0; i < pos; ++i) {
            rank += m_buf[i] == val;
        }
        return rank;
    }

private:
    std::vector<size_t> m_buf;
};

class alignas(8) WaveletTree {
public:
    using size_t = uint32_t;

    class BuildInfo {
    public:
        BuildInfo(std::vector<size_t> symb_freq, std::vector<size_t> bv_sizes, size_t bv_pos_begin)
            : m_symb_freq{std::move(symb_freq)}
            , m_bv_sizes{std::move(bv_sizes)}
            , m_bv_pos_begin{bv_pos_begin} {}

        size_t CalcOccupiedSize() const noexcept {
            size_t occ_size = m_bv_pos_begin;

            for (auto bv_size : m_bv_sizes) {
                occ_size += BitVector::CalcOccupiedSize(bv_size);
            }

            return occ_size;
        }

        const std::vector<size_t>& GetSymbFreq() const noexcept {
            return m_symb_freq;
        }

        const std::vector<size_t>& GetBitVectorSizes() const noexcept {
            return m_bv_sizes;
        }

        size_t GetBitVectorPosBegin() const noexcept {
            return m_bv_pos_begin;
        }

    private:
        std::vector<size_t> m_symb_freq;
        std::vector<size_t> m_bv_sizes;
        size_t m_bv_pos_begin;
    };

    template <typename NumberAccesstorT>
    static BuildInfo PrepareBuild(const NumberAccesstorT& text, size_t alph_size) {
        const auto size = text.Size();

        const auto num_levels = Log2Up(alph_size - 1);
        alph_size = 1u << num_levels;
        std::vector<size_t> symb_freq(alph_size);
        for (size_t i = 0; i < size; ++i) {
            auto value = text[i];
            assert(value < alph_size);

            ++symb_freq[value];
        }

        std::vector<size_t> bv_sizes((1u << num_levels) - 1);
        size_t i_begin_last_lvl = (1u << (num_levels - 1)) - 1;
        for (size_t i = 0; i < alph_size; i += 2) {
            size_t bv_size = symb_freq[i] + symb_freq[i + 1];
            bv_sizes[i / 2 + i_begin_last_lvl] = bv_size;
        }

        for (long i = (long)i_begin_last_lvl - 1; i >= 0; --i) {
            auto left_child_pos = GetChildPos(i, false);
            bv_sizes[i] = bv_sizes[left_child_pos] + bv_sizes[left_child_pos + 1];
        }

        size_t bv_pos_begin = sizeof(WaveletTree) + bv_sizes.size() * sizeof(bv_sizes[0]);
        bv_pos_begin = AlignPos(bv_pos_begin);

        return {std::move(symb_freq), std::move(bv_sizes), bv_pos_begin};
    }

    template <typename NumberAccesstorT>
    WaveletTree(const NumberAccesstorT& text, size_t alph_size, const BuildInfo& build_info) {
        const auto& bv_sizes = build_info.GetBitVectorSizes();
        const auto num_bv = bv_sizes.size();

        // Init Decard tree with bit vectors positions
        size_t* bv_poss = GetBitVectorPoss();
        bv_poss[0] = build_info.GetBitVectorPosBegin();
        for (size_t i = 1; i < num_bv; ++i) {
            auto prev_bv_size = BitVector::CalcOccupiedSize(bv_sizes[i - 1]);
            bv_poss[i] = bv_poss[i - 1] + prev_bv_size;
        }

        // Construct bit vectors
        for (size_t i = 0; i < num_bv; ++i) {
            assert(!((uint64_t)GetBitVectorPtr(i) & 0b111u));
            new (GetBitVectorPtr(i)) BitVector{bv_sizes[i]};
        }

        // Fill bit vectors
        std::vector<size_t> in_bv_pos(num_bv);

        const auto num_levels = Log2Up(alph_size - 1);
        m_num_levels = num_levels;
        const auto text_size = text.Size();
        for (size_t i = 0; i < text_size; ++i) {
            auto val = text[i];
            size_t bit_pos = 1u << (m_num_levels - 1);

            size_t i_bv = 0;
            for (size_t i_lvl = 0; i_lvl < num_levels; ++i_lvl) {
                const bool bit = !!(val & bit_pos);
                bit_pos >>= 1;

                auto& bv = GetBitVector(i_bv);
                bv.Set(in_bv_pos[i_bv]++, bit);

                i_bv = GetChildPos(i_bv, bit);
            }
        }

        for (size_t i = 0; i < num_bv; ++i) {
            assert(in_bv_pos[i] == bv_sizes[i]);
        }

        for (size_t i_bv = 0; i_bv < num_bv; ++i_bv) {
            auto& bv = GetBitVector(i_bv);
            bv.Reinit();
        }
    }

    size_t GetRank(size_t val, size_t pos) const {
        size_t bit_pos = 1u << (m_num_levels - 1);

        size_t i_bv = 0, rank = pos;
        for (size_t i_lvl = 0; i_lvl < m_num_levels; ++i_lvl) {
            if (rank == 0) {
                return 0;
            }

            const bool bit = !!(val & bit_pos);
            bit_pos >>= 1;

            const auto& bv = GetBitVector(i_bv);
            if (bit) {
                rank = bv.GetRank(rank);
            } else {
                const auto y = bv.GetRank(rank);
                rank = rank - bv.GetRank(rank);
            }

            i_bv = GetChildPos(i_bv, bit);
        }

        return rank;
    }

    // m_num_levels = 4
    // input: val = C***, signif_bit_len = 1
    // input: val = CA**, signif_bit_len = 2
    size_t GetFirstRank(size_t val, u8 signif_bit_len, size_t pos) const {
        assert(signif_bit_len <= m_num_levels);

        size_t bit_pos = 1u << (m_num_levels - 1);

        size_t i_bv = 0, rank = pos;
        for (size_t i_lvl = 0; i_lvl < signif_bit_len; ++i_lvl) {
            if (rank == 0) {
                return 0;
            }

            const bool bit = !!(val & bit_pos);
            bit_pos >>= 1;

            const auto& bv = GetBitVector(i_bv);
            if (bit) {
                rank = bv.GetRank(rank);
            } else {
                const auto y = bv.GetRank(rank);
                rank = rank - bv.GetRank(rank);
            }

            i_bv = GetChildPos(i_bv, bit);
        }
        
        if (rank == 0) {
            return 0;
        }

        for (size_t i_lvl = signif_bit_len; i_lvl < m_num_levels; ++i_lvl) {
            const auto& bv = GetBitVector(i_bv);
            size_t zero_rank = rank - bv.GetRank(rank);
            if (zero_rank) {
                rank = zero_rank;
            }
            i_bv = GetChildPos(i_bv, zero_rank == 0);
        }

        return rank;
    }

    void Dump() const {
        size_t i_bv = 0;
        for (size_t i_lvl = 0; i_lvl < m_num_levels; ++i_lvl) {
            const size_t num_lvl_bv = 1u << i_lvl;

            for (size_t i_lvl_bv = 0; i_lvl_bv < num_lvl_bv; ++i_lvl_bv, ++i_bv) {
                const auto& bv = GetBitVector(i_bv);

                for (size_t i_bit = 0; i_bit < bv.Size(); ++i_bit) {
                    putchar(bv.Get(i_bit) ? '1' : '0');

                    if (i_bit + 1 < bv.Size()) {
                        putchar(' ');
                    }
                }
                if (i_lvl_bv + 1 < num_lvl_bv) {
                    putchar('\'');
                }
            }
            putchar('\n');
        }
    }

private:
    static size_t GetChildPos(size_t parent_pos, bool is_right_child) noexcept {
        return 2 * parent_pos + is_right_child + 1;
    }

    size_t* GetBitVectorPoss() noexcept {
        return (size_t*)(this + 1);
    }
    const size_t* GetBitVectorPoss() const noexcept {
        return (size_t*)(this + 1);
    }

    BitVector* GetBitVectorPtr(size_t node_pos) noexcept {
        return (BitVector*)((u8*)this + GetBitVectorPoss()[node_pos]);
    }
    const BitVector* GetBitVectorPtr(size_t node_pos) const noexcept {
        return (BitVector*)((u8*)this + GetBitVectorPoss()[node_pos]);
    }

    BitVector& GetBitVector(size_t node_pos) noexcept {
        return *GetBitVectorPtr(node_pos);
    }
    const BitVector& GetBitVector(size_t node_pos) const noexcept {
        return *GetBitVectorPtr(node_pos);
    }

private:
    size_t m_num_levels;
};
