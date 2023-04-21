#pragma once

#include "patricia_trie.h"

namespace SBT {

template <bool IsLeaf>
struct ExtItem;

template <>
PACKED_STRUCT ExtItem<true> {
    str_pos_t str_pos;

    constexpr static inline uint num_str = 1;
};

template <>
PACKED_STRUCT ExtItem<false> {
    str_pos_t left_str_pos;
    str_pos_t right_str_pos;
    blk_pos_t child;

    constexpr static inline uint num_str = 2;
};

template <bool IsLeaf, uint NumLeaf>  // one pair of L and R is 2 leaves
struct NodeSectionsSize {
    static_assert(NumLeaf >= 2);

    constexpr static uint PT = PT::CalcMaxSize(NumLeaf);
    constexpr static uint Ext = NumLeaf / ExtItem<IsLeaf>::num_str * sizeof(ExtItem<IsLeaf>);
    constexpr static uint common = PT + Ext;
};

template <bool IsLeaf>
constexpr uint CalcPTNumLeaf(int pt_ext_size) noexcept {
    return 2 * pt_ext_size /
           (NodeSectionsSize<IsLeaf, 4>::common - NodeSectionsSize<IsLeaf, 2>::common);
}

PACKED_STRUCT NodeBase {
    enum class NodeType : u8 { Inner, Leaf };

    NodeType type;
};

template <bool IsLeaf>
PACKED_STRUCT Node {
    enum class Type : u8 { Inner, Leaf };
    Type type = IsLeaf ? Type::Leaf : Type::Inner;

    constexpr static uint num_leaves = CalcPTNumLeaf<IsLeaf>(g_block_size - sizeof(type));
    std::array<uint8_t, NodeSectionsSize<IsLeaf, num_leaves>::PT> PT;
    std::array<uint8_t, NodeSectionsSize<IsLeaf, num_leaves>::Ext> Ext;

    ExtItem<IsLeaf>* ExtBegin() noexcept {
        return (ExtItem<IsLeaf>*)&Ext;
    }
}
__attribute__((aligned(g_block_size)));

static_assert(sizeof(Node<true>) == g_block_size);
static_assert(sizeof(Node<false>) == g_block_size);

using InnerNode = Node<false>;
using LeafNode = Node<true>;

class StringBTree {
public:
    StringBTree(std::string path_sbt, std::string path_text);
    ~StringBTree() {}

    static StringBTree Build(std::string sbt_dest_path, std::string path_text,
                             const std::vector<str_len_t>& suff_arr);

private:
    FileMapperRead btree;
    FileMapperRead text;
    const u8* root;
};

}  // namespace SBT
