#include <gtest/gtest.h>
#include <random>
#include <chrono>

#include "bitvector.hpp"
#include "wavelet_tree.hpp"

TEST(BV, MANUAL) {
    auto test = [](auto& bv) {
        ASSERT_TRUE(bv.Size() >= 4);

        for (std::size_t i = 0; i < bv.Size(); ++i) {
            ASSERT_EQ(bv.Get(i), false);
            ASSERT_EQ(bv.GetRank(i), 0);
        }

        bv.Set(0, true);
        ASSERT_EQ(bv.Get(0), true);
        ASSERT_EQ(bv.GetRank(0), 0);
        ASSERT_EQ(bv.GetRank(1), 1);
        ASSERT_EQ(bv.GetRank(2), 1);
        ASSERT_EQ(bv.GetRank(3), 1);
        ASSERT_EQ(bv.GetRank(4), 1);

        bv.Set(2, true);
        ASSERT_EQ(bv.Get(0), true);
        ASSERT_EQ(bv.GetRank(0), 0);
        ASSERT_EQ(bv.GetRank(1), 1);
        ASSERT_EQ(bv.GetRank(2), 1);
        ASSERT_EQ(bv.GetRank(3), 2);
        ASSERT_EQ(bv.GetRank(4), 2);
    };

    const std::size_t bv_size = 4;

    BitVectorNaive bv_naive{bv_size};
    test(bv_naive);

    BitVectorBuffer bv_buf{bv_size};
    BitVector bv{bv_size, bv_buf.Data()};
    test(bv);
}

TEST(BV, RANDOM) {
    const unsigned seed = 0xEDA + 0xDED + 32;
    std::mt19937_64 gen{seed};

    const std::size_t bv_size = 1235;
    std::size_t num_repeats = 100;

    std::uniform_int_distribution<std::size_t> num_set_distrib{0, 2 * bv_size};
    std::uniform_int_distribution<std::size_t> pos_distrib{0, bv_size};
    std::uniform_int_distribution<uint8_t> set_distrib{0, 1};

    BitVectorNaive bv_naive{bv_size};

    BitVectorBuffer bv_buf{bv_size};
    BitVector bv{bv_size, bv_buf.Data()};

    while (num_repeats--) {
        auto num_set = num_set_distrib(gen);
        while (num_set--) {
            const auto pos = pos_distrib(gen);
            const bool value = set_distrib(gen);

            bv_naive.Set(pos, value);
            bv.Set(pos, value);
        }

        for (std::size_t i = 0; i < bv_size; ++i) {
            ASSERT_EQ(bv.Get(i), bv_naive.Get(i));
            ASSERT_EQ(bv.GetRank(i + 1), bv_naive.GetRank(i + 1));
        }
    }
}

TEST(BV, GET_RANK_SPEED) {
    auto now = [] {
        return std::chrono::high_resolution_clock::now();
    };

    auto delta_ms = [](auto time_start, auto time_finish) {
        auto delta = time_finish - time_start;
        return std::chrono::duration_cast<std::chrono::milliseconds>(delta).count();
    };

    auto exec_time = [&](auto&& func, auto&&... args) {
        auto time_begin = now();
        volatile auto res =
            std::forward<decltype(func)>(func)(std::forward<decltype(args)>(args)...);
        auto time_end = now();
        return delta_ms(time_begin, time_end);
    };

    const std::size_t bv_size = 32 * 1024;
    BitVectorNaive bv_naive{bv_size};

    BitVectorBuffer bv_buf{bv_size};
    BitVector bv{bv_size, bv_buf.Data()};

    const std::size_t seed = 0xDED;
    std::mt19937_64 gen{seed};
    std::uniform_int_distribution<std::size_t> pos_distrib{0, bv_size};

    for (std::size_t i = 0; i < bv_size; ++i) {
        const auto pos = pos_distrib(gen);
        bv_naive.Set(pos, true);
        bv.Set(pos, true);
    }

    auto rank_linear = [](auto& bv) {
        auto size = bv.Size();

        std::size_t sum_rank = 0;
        while (size--) {
            sum_rank += bv.GetRank(size);
        }

        return sum_rank;
    };

    std::size_t bv_naive_time = 0, bv_time = 0;
    std::size_t num_repeats = 10;
    for (int i = 0; i < num_repeats; ++i) {
        bv_naive_time += exec_time(rank_linear, bv_naive);
        bv_time += exec_time(rank_linear, bv);
    }
    bv_naive_time /= num_repeats;
    bv_time /= num_repeats;

    std::cout << "bv_naive_time: " << bv_naive_time << std::endl;
    std::cout << "bv_time: " << bv_time << std::endl;
}

TEST(WAVELET_TREE, MANUAL) {}