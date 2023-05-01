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
    enum class Type : u8 { Inner, Leaf };
    Type type;

    NodeBase(Type type)
        : type{type} {}

    bool IsLeaf() const noexcept {
        return type == Type::Leaf;
    }
    bool IsInner() const noexcept {
        return type == Type::Inner;
    }
};

template <bool IsLeafV>
PACKED_STRUCT Node : public NodeBase {
    Node() noexcept
        : NodeBase{IsLeafV ? Type::Leaf : Type::Inner} {}

    constexpr static uint num_leaves = CalcPTNumLeaf<IsLeafV>(g_block_size - sizeof(NodeBase));
    std::array<uint8_t, NodeSectionsSize<IsLeafV, num_leaves>::PT> PT;
    std::array<uint8_t, NodeSectionsSize<IsLeafV, num_leaves>::Ext> Ext;

    using ExtItemT = ExtItem<IsLeafV>;

    ExtItemT* ExtBegin() noexcept {
        return (ExtItemT*)&Ext;
    }

    const ExtItemT* ExtBegin() const noexcept {
        return (const ExtItemT*)&Ext;
    }

    in_blk_pos_t GetExtPosBegin() const noexcept {
        return (const u8*)&Ext - (const u8*)&PT;
    }

    PT::Wrapper GetPT() const noexcept {
        return {(const u8*)&PT, GetExtPosBegin()};
    }
};

static_assert(sizeof(Node<true>) <= g_block_size);
static_assert(sizeof(Node<false>) <= g_block_size);

using InnerNode = Node<false>;
using LeafNode = Node<true>;

PT::Wrapper GetPT(const NodeBase* node_base) noexcept;

void DumpExt(const NodeBase* node_base);

class StringBTree {
public:
    StringBTree(std::string path_sbt, std::string path_text);
    ~StringBTree() {}

    static StringBTree Build(std::string sbt_dest_path, std::string path_text,
                             const std::vector<str_len_t>& suff_arr);

    std::string_view Search(std::string_view pattern);

    void Dump();

private:
    void DumpImpl(const NodeBase* node_base, int depth);
    const NodeBase* GetNodeBase(blk_pos_t blk_pos) const noexcept;

private:
    FileMapperRead m_btree;
    FileMapperRead m_text;
    const u8* m_root;

    // Cache
    str_pos_t m_leftmost_str;
    str_pos_t m_rightmost_str;
};

}  // namespace SBT
