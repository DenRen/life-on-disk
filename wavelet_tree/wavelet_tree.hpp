#pragma once

#include "bitvector.hpp"

class alignas(8) WaveletTree {
public:
    using size_t = uint32_t;

    class BuildInfo {
    public:
        BuildInfo(std::vector<size_t> bv_sizes, size_t bv_pos_begin)
            : m_bv_sizes{std::move(bv_sizes)}
            , m_bv_pos_begin{bv_pos_begin} {}

        size_t CalcOccupiedSize() const noexcept {
            size_t occ_size = m_bv_pos_begin;

            for (auto bv_size : m_bv_sizes) {
                if (bv_size) {
                    occ_size += BitVector::CalcOccupiedSize(bv_size);
                }
            }

            return occ_size;
        }

        const std::vector<size_t>& GetBitVectorSizes() const noexcept {
            return m_bv_sizes;
        }

        size_t GetBitVectorPosBegin() const noexcept {
            return m_bv_pos_begin;
        }

    private:
        std::vector<size_t> m_bv_sizes;
        size_t m_bv_pos_begin;
    };

    template <typename NumberAccesstorT>
    static BuildInfo PrepareBuild(const NumberAccesstorT& text, size_t alph_size) {
        const auto size = text.Size();

        const auto num_levels = Log2Up(alph_size - 1);
        alph_size = 1u << num_levels;
        std::vector<size_t> symb_freq(1u << num_levels);
        for (size_t i = 0; i < size; ++i) {
            auto value = text[i];
            assert(value < alph_size);

            ++symb_freq[value];
        }

        // 110 -> 011
        auto reverse = [num_levels](size_t val) {
            size_t rev = 0;
            for (size_t i = 0; i < num_levels; ++i) {
                rev <<= 1;
                rev |= val & 1u;
                val >>= 1;
            }
            return rev;
        };

        std::vector<size_t> bv_sizes((1u << num_levels) - 1);
        size_t i_begin_last_lvl = (1u << (num_levels - 1)) - 1;
        for (size_t i = 0; i < alph_size; i += 2) {
            size_t bv_size = symb_freq[reverse(i)] + symb_freq[reverse(i + 1)];
            bv_sizes[i / 2 + i_begin_last_lvl] = bv_size;
        }

        for (long i = (long)i_begin_last_lvl - 1; i >= 0; --i) {
            auto left_child_pos = GetChildPos(i, false);
            bv_sizes[i] = bv_sizes[left_child_pos] + bv_sizes[left_child_pos + 1];
        }

        size_t bv_pos_begin = sizeof(WaveletTree) + bv_sizes.size() * sizeof(bv_sizes[0]);
        bv_pos_begin = AlignPos(bv_pos_begin);

        return {std::move(bv_sizes), bv_pos_begin};
    }

    template <typename NumberAccesstorT>
    WaveletTree(const NumberAccesstorT& text, size_t alph_size, const BuildInfo& build_info) {
        const auto& bv_sizes = build_info.GetBitVectorSizes();
        const auto num_bv = bv_sizes.size();

        // Init Decard tree with bit vectors positions
        size_t* bv_poss = GetBitVectorPoss();
        bv_poss[0] = build_info.GetBitVectorPosBegin();
        for (size_t i = 1; i < num_bv; ++i) {
            auto prev_bv_size = bv_sizes[i - 1] ? BitVector::CalcOccupiedSize(bv_sizes[i - 1]) : 0;
            bv_poss[i] = bv_poss[i - 1] + prev_bv_size;
        }

        // Construct bit vectors
        for (size_t i = 0; i < num_bv; ++i) {
            if (bv_sizes[i]) {
                if ((uint64_t)GetBitVectorPtr(i) & 0b111u) {
                    std::cout << (uint64_t)GetBitVectorPtr(i) << std::endl;
                    throw std::runtime_error{"GFFF"};
                }
                new (GetBitVectorPtr(i)) BitVector{bv_sizes[i]};
            }
        }

        // Fill bit vectors
        std::vector<size_t> in_bv_pos(num_bv);

        const auto num_levels = Log2Up(alph_size - 1);
        const auto text_size = text.Size();
        for (size_t i = 0; i < text_size; ++i) {
            auto val = text[i];

            size_t i_bv = 0;
            for (size_t i_lvl = 0; i_lvl < num_levels; ++i_lvl) {
                auto& bv = GetBitVector(i_bv);
                const bool bit = val & 1u;
                bv.Set(in_bv_pos[i_bv]++, bit);

                i_bv = GetChildPos(i_bv, bit);
                val >>= 1;
            }
        }

        for (size_t i = 0; i < num_bv; ++i) {
            if (in_bv_pos[i] != bv_sizes[i]) {
                throw std::runtime_error{"Detected incorrect WT build"};
            }
        }
    }

private:
    static size_t GetChildPos(size_t parent_pos, bool is_right_child) noexcept {
        return 2 * parent_pos + is_right_child + 1;
    }

    size_t* GetBitVectorPoss() noexcept {
        return (size_t*)(this + 1);
    }

    BitVector* GetBitVectorPtr(size_t node_pos) noexcept {
        return (BitVector*)((u8*)this + GetBitVectorPoss()[node_pos]);
    }
    BitVector& GetBitVector(size_t node_pos) noexcept {
        return *GetBitVectorPtr(node_pos);
    }

private:
};
