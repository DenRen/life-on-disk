#pragma once

#include <string.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <string>

#define PACKED_STRUCT struct __attribute__((__packed__))

namespace PT {

using u8 = uint8_t;

using char_t = char;
using alph_size_t = u8;
using len_t = uint32_t;
using ext_pos_t = uint64_t;
using uint = unsigned;

using node_pos_t = uint16_t;

}  // namespace PT