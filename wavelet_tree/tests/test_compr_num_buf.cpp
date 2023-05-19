#include <gtest/gtest.h>

#include "../compr_num_buf.hpp"

TEST(COMPR_NUM_BUF, MANUAL) {
    u8 bit_len = 10;
    std::size_t size = 500;

    const auto occup_size = CompressedNumberBuf::CalcOccupiedSize(bit_len, size);
    std::vector<u8> place(occup_size);

    auto* buf_ptr = new (place.data()) CompressedNumberBuf{bit_len, size};
    auto& buf = *buf_ptr;

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