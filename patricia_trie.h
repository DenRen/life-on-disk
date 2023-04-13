#pragma once

#include <string.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <string>

#define PACKED_STRUCT struct __attribute__((__packed__))

using u8 = uint8_t;
using uint = unsigned;

using char_t = char;
using alph_size_t = u8;

constexpr uint g_page_size = 4096;

namespace PT {

using len_t = uint32_t;
using node_pos_t = uint16_t;

PACKED_STRUCT InnerNode final {
    InnerNode(len_t len = 0, alph_size_t size = 0) noexcept
        : len{len}
        , size(size) {}

    len_t len;
    alph_size_t size;
};

/*
    In PT not external node beacuse pre leaf node of PT
    will be point to obect of SBT, whose position will
    greater then some number, but less 2^sizeof(node_pos_t)
*/

PACKED_STRUCT Branch final {  // Between inner nodes
    char_t symb;
    node_pos_t node_pos;
};

constexpr uint CalcMaxInnerSize(uint num_leafs) noexcept {
    uint num_inner_nodes = num_leafs - 1;
    uint branch_per_node = 2;
    return num_inner_nodes * branch_per_node * sizeof(Branch);
}

void BuildAmdEmplacePT(const std::vector<std::pair<std::string_view, node_pos_t>>& strs);

}  // namespace PT

namespace SBT {

using ext_pos_t = uint64_t;
using blk_pos_t = uint32_t;

using node_pos_t = uint16_t;

template <bool IsLeaf>
struct ExtItem;

template <>
PACKED_STRUCT ExtItem<true> {
    ext_pos_t str;
};

template <>
PACKED_STRUCT ExtItem<false> {
    ext_pos_t left_str;
    ext_pos_t right_str;
    blk_pos_t child;
};

template <bool IsLeaf, uint NumLeaf>  // one pair of L and R is 2 leaves
struct SectionsSize {
    constexpr static uint PT = PT::CalcMaxInnerSize(NumLeaf);
    constexpr static uint Ext = NumLeaf / 2 * sizeof(ExtItem<IsLeaf>);
    constexpr static uint common = PT + Ext;
};

template <bool IsLeaf>
constexpr uint CalcPTNumLeaf() noexcept {
    return 2 * g_page_size / (SectionsSize<IsLeaf, 4>::common - SectionsSize<IsLeaf, 2>::common);
}

PACKED_STRUCT NodeBase {
    enum class NodeType : u8 { Inner, Leaf };

    NodeType type;
};

template <bool IsLeaf>
struct Node {
    constexpr static uint PT_num_leaf = CalcPTNumLeaf<IsLeaf>();
    std::array<uint8_t, SectionsSize<IsLeaf, PT_num_leaf>::PT> PT;
    std::array<uint8_t, SectionsSize<IsLeaf, PT_num_leaf>::Ext> Ext;
} __attribute__((aligned(g_page_size)));

static_assert(sizeof(Node<true>) == g_page_size);
static_assert(sizeof(Node<false>) == g_page_size);

using InnerNode = Node<false>;
using LeafNode = Node<true>;

}  // namespace SBT