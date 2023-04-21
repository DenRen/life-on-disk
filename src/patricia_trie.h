#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <string>

#include "file_mapper.h"

namespace PT {

PACKED_STRUCT InnerNode final {
    InnerNode(str_len_t len = 0, alph_size_t size = 0) noexcept
        : len{len}
        , num_branch(size) {}

    str_len_t len;
    alph_size_t num_branch;
};

/*
    In PT not external node beacuse pre leaf node of PT
    will be point to obect of SBT, whose position will
    greater then some number, but less 2^sizeof(node_pos_t)
*/

PACKED_STRUCT Branch final {  // Between inner nodes
    char_t symb;
    in_blk_pos_t node_pos;
};

constexpr uint CalcMaxSize(uint num_leafs) noexcept {
    // Higher trie
    uint num_inner_nodes = num_leafs - 1;
    uint branch_per_node = 2;
    return num_inner_nodes * (sizeof(InnerNode) + branch_per_node * sizeof(Branch));
}

void BuildAmdEmplacePT(const std::vector<std::pair<std::string_view, in_blk_pos_t>>& strs, u8* dest,
                       size_t size);

}  // namespace PT
