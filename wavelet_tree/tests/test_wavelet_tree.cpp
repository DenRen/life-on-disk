#include <gtest/gtest.h>
#include <random>

#include "wavelet_tree.hpp"

class TestText : public std::vector<unsigned> {
public:
    std::size_t Size() const noexcept {
        return size();
    }
};

TEST(WAVELET_TREE, MANUAL) {
    //             0  1  2  3   4  5  6  7
    TestText text{{1, 2, 4, 4, 10, 3, 3, 2}};

    auto build_info = WaveletTree::PrepareBuild(text, 16);
    std::vector<u8> mapped_buf(build_info.CalcOccupiedSize());
    auto& wt = *new (mapped_buf.data()) WaveletTree{text, 10, build_info};

    WaveletTreeNaive wt_naive{text};

    auto test = [](const auto& wt) {
        ASSERT_EQ(wt.GetRank(0, 1), 0);

        ASSERT_EQ(wt.GetRank(1, 1), 1);
        ASSERT_EQ(wt.GetRank(1, 2), 1);

        ASSERT_EQ(wt.GetRank(2, 1), 0);
        ASSERT_EQ(wt.GetRank(2, 2), 1);
        ASSERT_EQ(wt.GetRank(2, 7), 1);
        ASSERT_EQ(wt.GetRank(2, 8), 2);

        ASSERT_EQ(wt.GetRank(10, 4), 0);
        ASSERT_EQ(wt.GetRank(10, 5), 1);
        ASSERT_EQ(wt.GetRank(10, 6), 1);

        ASSERT_EQ(wt.GetRank(12, 4), 0);
        ASSERT_EQ(wt.GetRank(12, 5), 0);
        ASSERT_EQ(wt.GetRank(12, 6), 0);
    };

    test(wt_naive);
    test(wt);
}

TEST(WAVELET_TREE, RANDOM) {
    using size_t = WaveletTree::size_t;

    const unsigned seed = 0xEDA + 0xDED * 64;
    std::mt19937_64 gen{seed};

    const size_t text_size_min = 1;
    const size_t text_size_max = 1000;

    const size_t alph_size_min = 2;
    const size_t alph_size_max = 1000;

    using UniformDist = std::uniform_int_distribution<size_t>;

    UniformDist text_size_distrib{text_size_min, text_size_max};
    UniformDist alph_size_distrib{alph_size_min, alph_size_max};

    size_t num_repeats = 10;
    while (num_repeats--) {
        const auto text_size = text_size_distrib(gen);
        const auto alph_size = alph_size_distrib(gen);

        UniformDist symb_distrib{0, alph_size - 1};

        TestText text;
        text.resize(text_size);
        for (size_t i = 0; i < text_size; ++i) {
            text[i] = symb_distrib(gen);
        }

        auto build_info = WaveletTree::PrepareBuild(text, alph_size);
        std::vector<u8> mapped_buf(build_info.CalcOccupiedSize());
        auto& wt = *new (mapped_buf.data()) WaveletTree{text, alph_size, build_info};

        WaveletTreeNaive wt_naive{text};

        std::size_t dt = 0, dt_naive = 0;
        for (size_t i_pos = 0; i_pos < text_size; ++i_pos) {
            for (size_t symb = 0; symb < alph_size; ++symb) {
                const auto res = wt.GetRank(symb, i_pos);
                const auto ref = wt_naive.GetRank(symb, i_pos);

                ASSERT_EQ(res, ref);
            }
        }
    }
}
