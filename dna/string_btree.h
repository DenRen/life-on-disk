#pragma once

#include "patricia_trie.h"

namespace DNA_SBT {

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

template <bool IsLeafV, uint NumLeaf>  // one pair of L and R is 2 leaves
struct NodeSectionsSize {
    static_assert(NumLeaf >= 2);

    constexpr static uint PT = DNA_PT::CalcMaxSize(NumLeaf);
    constexpr static uint Ext = NumLeaf / ExtItem<IsLeafV>::num_str * sizeof(ExtItem<IsLeafV>);
    constexpr static uint common = PT + Ext;
};

template <bool IsLeafV>
constexpr uint CalcPTNumLeaf(int pt_ext_size) noexcept {
    return 2 * pt_ext_size /
           (NodeSectionsSize<IsLeafV, 52>::common - NodeSectionsSize<IsLeafV, 50>::common);
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

    constexpr static uint num_leaves = CalcPTNumLeaf<IsLeafV>(g_block_size - sizeof(NodeBase) - 30);
    std::array<uint8_t, NodeSectionsSize<IsLeafV, num_leaves + 2>::PT> PT;
    std::array<uint8_t, NodeSectionsSize<IsLeafV, num_leaves + 2>::Ext> Ext;

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

    DNA_PT::Wrapper GetPT() const noexcept {
        return {(const u8*)&PT, GetExtPosBegin()};
    }
};

using InnerNode = Node<false>;
using LeafNode = Node<true>;

static_assert(sizeof(InnerNode) <= g_block_size);
static_assert(sizeof(LeafNode) <= g_block_size);

DNA_PT::Wrapper GetPT(const NodeBase* node_base) noexcept;

void DumpExt(const NodeBase* node_base);

class StringBTree {
public:
    StringBTree(std::string sbt_path, std::string dna_data_path);
    ~StringBTree() {}

    static StringBTree Build(std::string sbt_dest_path, std::string dna_data_path,
                             std::string dna_data_sa_path);

    std::pair<str_pos_t, bool> Search(DnaDataAccessor pattern);

    void Dump();

private:
    void DumpImpl(const NodeBase* node_base, int depth);
    const NodeBase* GetNodeBase(blk_pos_t blk_pos) const noexcept;

private:
    FileMapperRead m_btree;
    const u8* m_root;

    ObjectFileHolder m_dna_data_holder;
    DnaDataAccessor m_dna;

    // Cache
    str_pos_t m_leftmost_str;
    str_pos_t m_rightmost_str;
};

}  // namespace DNA_SBT
