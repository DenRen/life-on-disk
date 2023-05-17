#include <gtest/gtest.h>

#include "bitvector.hpp"

TEST(COMPR_NUM_BUF, MANUAL) {
    u8 bit_len = 10;
    CompressedNumberBuf buf{bit_len, 500};

    for (std::size_t i = 0; i < buf.Size(); ++i) {
        ASSERT_EQ(buf.Get(i), 0);
    }

    buf.Set(0, 15);
    ASSERT_EQ(buf.Get(0), 15);
    ASSERT_EQ(buf.Get(1), 0);

    buf.Set(0, (1u << bit_len) - 1);
    ASSERT_EQ(buf.Get(0), (1u << bit_len) - 1);
    ASSERT_EQ(buf.Get(1), 0);

    buf.Set(0, 0);
    ASSERT_EQ(buf.Get(0), 0);
    ASSERT_EQ(buf.Get(1), 0);

    for (std::size_t i = 0; i < buf.Size(); ++i) {
        buf.Set(i, i);
        for (std::size_t j = 0; j < buf.Size(); ++j) {
            ASSERT_EQ(buf.Get(j), j <= i ? j : 0) << i << " " << j;
        }
    }
}