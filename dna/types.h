#pragma once

#include "../common/common_type.h"

#include "dna.h"

enum class DnaSymb : uint8_t { TERM = 0, A, C, T, G, N };

using char_t = DnaSymb;
using alph_size_t = u8;

using str_len_t = uint32_t;

using in_blk_pos_t = uint16_t;
using blk_pos_t = uint32_t;
using str_pos_t = str_len_t;
