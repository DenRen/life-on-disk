#pragma once

#include "../common/common_type.h"

using char_t = char;
using alph_size_t = u8;

using str_len_t = uint32_t;

using in_blk_pos_t = uint16_t;
using blk_pos_t = uint32_t;
using str_pos_t = str_len_t;

auto inline CompareChar = [](const char_t& lhs, const char_t& rhs) noexcept {
    return (u8)lhs < (u8)rhs;
};
