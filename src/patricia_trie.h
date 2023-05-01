#pragma once

#include "file_mapper.h"
#include "misalign.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <string>

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

class Wrapper {
public:
    Wrapper(const InnerNode* root, in_blk_pos_t ext_pos_begin) noexcept
        : m_root{root}
        , m_ext_pos_begin{ext_pos_begin} {}

    Wrapper(const u8* root, in_blk_pos_t ext_pos_begin) noexcept
        : Wrapper{(const InnerNode*)root, ext_pos_begin} {}

    const InnerNode* GetNode(in_blk_pos_t node_pos) const noexcept {
        return (const InnerNode*)((const u8*)m_root + node_pos);
    };

    in_blk_pos_t GetNodePos(const InnerNode* node) const noexcept {
        return (const u8*)node - (const u8*)m_root;
    }

    in_blk_pos_t GetExtPos() const noexcept {
        return m_ext_pos_begin;
    }
    const InnerNode* GetRoot() const noexcept {
        return m_root;
    }

    in_blk_pos_t GetLeftmostExt(const Branch* branch) const noexcept {
        const InnerNode* node = nullptr;
        const Branch* branchs_begin = branch;
        while (branchs_begin[0].node_pos < m_ext_pos_begin) {
            node = GetNode(branchs_begin[0].node_pos);
            branchs_begin = node->GetBranchs();
        }
        return branchs_begin[0].node_pos;
    }

    in_blk_pos_t GetLeftmostExt(const InnerNode* node) const noexcept {
        return GetLeftmostExt(node->GetBranchs());
    }

    in_blk_pos_t GetRightmostExt(const InnerNode* node) const noexcept {
        const Branch* branchs_begin = node->GetBranchs();
        while (branchs_begin[node->num_branch - 1].node_pos < m_ext_pos_begin) {
            node = GetNode(branchs_begin[node->num_branch - 1].node_pos);
            branchs_begin = node->GetBranchs();
        }
        return branchs_begin[node->num_branch - 1].node_pos;
    }

    str_pos_t GetStr(in_blk_pos_t ext_pos) const noexcept {
        return misalign_load<str_pos_t>((const u8*)m_root + ext_pos);
    }

    str_pos_t GetLeftmostStr() const noexcept {
        return GetStr(m_ext_pos_begin);
    }

    str_pos_t GetRightmostStr() const noexcept {
        return GetStr(GetRightmostExt(m_root));
    }

private:
    const InnerNode* m_root;
    in_blk_pos_t m_ext_pos_begin;
};

}  // namespace PT
