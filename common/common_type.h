#pragma once

#include <cstdint>

#define PACKED_STRUCT struct __attribute__((__packed__))

using u8 = uint8_t;
using uint = unsigned;

constexpr inline uint g_block_size = 4096;
