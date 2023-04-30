#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <string>

#include "file_mapper.h"

namespace PT {

/*
    In PT not external node beacuse pre leaf node of PT
    will be point to obect of SBT, whose position will
    greater then some number, but less 2^sizeof(node_pos_t)
*/

PACKED_STRUCT Branch final {  // Between inner nodes
    char_t symb;
    in_blk_pos_t node_pos;
};

PACKED_STRUCT InnerNode final {
    InnerNode(str_len_t len = 0, alph_size_t size = 0) noexcept
        : len{len}
        , num_branch(size) {}

    str_len_t len;
    alph_size_t num_branch;

    const Branch* GetBranchs() const noexcept {
        return (const Branch*)(this + 1);
    }
};

struct SearchResult final {
    in_blk_pos_t ext_node_pos;
    str_len_t lcp;
};

SearchResult Search(std::string_view pattern, const InnerNode* root, str_len_t last_lcp,
                    in_blk_pos_t ext_pos_begin, std::string_view text);

constexpr uint CalcMaxSize(uint num_leafs) noexcept {
    // Higher trie
    uint num_inner_nodes = num_leafs - 1;
    uint branch_per_node = 2;
    return num_inner_nodes * (sizeof(InnerNode) + branch_per_node * sizeof(Branch));
}

void BuildAndEmplacePT(const std::vector<std::pair<std::string_view, in_blk_pos_t>>& strs, u8* dest,
                       size_t size);

}  // namespace PT
