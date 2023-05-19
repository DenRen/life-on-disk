#include <gtest/gtest.h>
#include <random>
#include <chrono>

#include "bitvector.hpp"

TEST(BV, MANUAL) {
    auto test = [](auto& bv) {
        ASSERT_TRUE(bv.Size() >= 4);

        for (std::size_t i = 0; i < bv.Size(); ++i) {
            ASSERT_EQ(bv.Get(i), false);
            ASSERT_EQ(bv.GetRank(i), 0);
        }

        bv.Set(0, true);
        bv.Reinit();
        ASSERT_EQ(bv.Get(0), true);
        ASSERT_EQ(bv.GetRank(0), 0);
        ASSERT_EQ(bv.GetRank(1), 1);
        ASSERT_EQ(bv.GetRank(2), 1);
        ASSERT_EQ(bv.GetRank(3), 1);
        ASSERT_EQ(bv.GetRank(4), 1);

        bv.Set(2, true);
        bv.Reinit();
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
    auto& bv = *new (bv_buf.Data()) BitVector{bv_size};
    test(bv);
}

TEST(BV, RANDOM) {
    const unsigned seed = 0xEDA + 0xDED * 32;
    std::mt19937_64 gen{seed};

    const std::size_t bv_size_min = 1;
    const std::size_t bv_size_max = 2000;
    std::size_t num_repeats = 100;

    std::uniform_int_distribution<BitVector::size_t> bv_size_distrib{bv_size_min, bv_size_max};
    while (num_repeats--) {
        const auto bv_size = bv_size_distrib(gen);

        std::uniform_int_distribution<std::size_t> num_set_distrib{0, 2 * bv_size - 1};
        std::uniform_int_distribution<std::size_t> pos_distrib{0, bv_size - 1};
        std::uniform_int_distribution<u8> set_distrib{0, 1};

        BitVectorNaive bv_naive{bv_size};

        BitVectorBuffer bv_buf{bv_size};
        auto& bv = *new (bv_buf.Data()) BitVector{bv_size};
        auto num_set = num_set_distrib(gen);
        while (num_set--) {
            const auto pos = pos_distrib(gen);
            const bool value = set_distrib(gen);

            bv_naive.Set(pos, value);
            bv.Set(pos, value);
        }

        bv_naive.Reinit();
        bv.Reinit();

        for (std::size_t i = 0; i < bv_size; ++i) {
            ASSERT_EQ(bv.Get(i), bv_naive.Get(i));
            ASSERT_EQ(bv.GetRank(i + 1), bv_naive.GetRank(i + 1)) << "i: " << i;
        }
    }
}

TEST(BV, GET_RANK_SPEED) {
    auto now = [] {
        return std::chrono::high_resolution_clock::now();
    };

    auto delta_us = [](auto time_start, auto time_finish) {
        auto delta = time_finish - time_start;
        return std::chrono::duration_cast<std::chrono::microseconds>(delta).count();
    };

    auto exec_time = [&](auto& res, auto&& func, auto&&... args) {
        auto time_begin = now();
        res = std::forward<decltype(func)>(func)(std::forward<decltype(args)>(args)...);
        auto time_end = now();
        return delta_us(time_begin, time_end);
    };

    const std::size_t bv_size = 16 * 1024;
    BitVectorNaive bv_naive{bv_size};

    BitVectorBuffer bv_buf{bv_size};
    auto& bv = *new (bv_buf.Data()) BitVector{bv_size};

    const std::size_t seed = 0xDED;
    std::mt19937_64 gen{seed};
    std::uniform_int_distribution<std::size_t> pos_distrib{0, bv_size - 1};

    for (std::size_t i = 0; i < bv_size; ++i) {
        const auto pos = pos_distrib(gen);
        bv_naive.Set(pos, true);
        bv.Set(pos, true);
    }

    bv_naive.Reinit();
    bv.Reinit();

    auto rank_linear = [](auto& bv) {
        auto size = bv.Size();

        std::size_t sum_rank = 0;
        while (size--) {
            sum_rank += bv.GetRank(size);
        }

        return sum_rank;
    };

    std::size_t bv_naive_time_us = 0, bv_time_us = 0;
    std::size_t num_repeats = 2;
    for (int i = 0; i < num_repeats; ++i) {
        std::size_t res_naive = 0, res = 0;
        bv_naive_time_us += exec_time(res_naive, rank_linear, bv_naive);
        bv_time_us += exec_time(res, rank_linear, bv);

        ASSERT_EQ(res, res_naive);
    }
    bv_naive_time_us /= num_repeats;
    bv_time_us /= num_repeats;

    ASSERT_LT(100 * bv_time_us, bv_naive_time_us);

    // std::cout << "bv_naive_time_us: " << bv_naive_time_us << std::endl;
    // std::cout << "bv_time_us: " << bv_time_us << std::endl;
}

TEST(LOG2_UP, MANUAL) {
    ASSERT_EQ(Log2Up(15u), 4u);
    ASSERT_EQ(Log2Up(16u), 5u);
    ASSERT_EQ(Log2Up(17u), 5u);
}
