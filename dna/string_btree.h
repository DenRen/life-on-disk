#pragma once

#include "patricia_trie.h"

#include <limits>

constexpr inline bool SBT_BUILD_LOG = true;

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

PACKED_STRUCT NodeBase {
    enum class Type : u8 { Inner, Leaf };
    Type type;

    str_pos_t suff_arr_left_size;

    NodeBase(Type type)
        : type{type} {}

    bool IsLeaf() const noexcept {
        return type == Type::Leaf;
    }
    bool IsInner() const noexcept {
        return type == Type::Inner;
    }
};

template <typename CharT>
class StringBTree {
    using PT_T = DNA_PT::PT<CharT>;
    using PtWrapperT = typename PT_T::Wrapper;

    template <bool IsLeafV, uint NumLeaf>  // one pair of L and R is 2 leaves
    struct NodeSectionsSize {
        static_assert(NumLeaf >= 2);

        constexpr static uint PT = PT_T::CalcMaxSize(NumLeaf);
        constexpr static uint Ext = NumLeaf / ExtItem<IsLeafV>::num_str * sizeof(ExtItem<IsLeafV>);
        constexpr static uint common = PT + Ext;
    };

    template <bool IsLeafV>
    constexpr static uint CalcPTNumLeaf(int pt_ext_size) noexcept {
        return 2 * pt_ext_size /
               (NodeSectionsSize<IsLeafV, 6>::common - NodeSectionsSize<IsLeafV, 4>::common);
    }
public:
    template <bool IsLeafV>
    PACKED_STRUCT Node : public NodeBase {
        Node() noexcept
            : NodeBase{IsLeafV ? Type::Leaf : Type::Inner} {}

        constexpr static uint num_leaves =
            CalcPTNumLeaf<IsLeafV>(g_block_size - sizeof(NodeBase) - 30);
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

        typename PT_T::Wrapper GetPT() const noexcept {
            return {(const u8*)&PT, GetExtPosBegin()};
        }
    };

    using InnerNode = Node<false>;
    using LeafNode = Node<true>;

    static_assert(sizeof(InnerNode) <= g_block_size);
    static_assert(sizeof(LeafNode) <= g_block_size);

    typename PT_T::Wrapper GetPT(const NodeBase* node_base) noexcept {
        return node_base->IsInner() ? ((const InnerNode*)node_base)->GetPT()
                                    : ((const LeafNode*)node_base)->GetPT();
    }

    void DumpExt(const NodeBase* node_base);

public:
    StringBTree(std::string sbt_path);

    template <typename AccessorT>
    static StringBTree Build(std::string sbt_dest_path, const AccessorT& dna_data,
                             std::string dna_data_sa_path);

    // dna pos, left sa pos, right sa pos, is find
    template <typename AccessorT>
    std::tuple<str_pos_t, str_pos_t, str_pos_t, str_len_t> Search(const AccessorT& pattern,
                                                                  const AccessorT& dna_data);

    void Dump() {
        DumpImpl((const NodeBase*)m_root, 0);
    }

private:
    static blk_pos_t CalcCommonNumBlock(str_len_t num_string);

    void DumpImpl(const NodeBase* node_base, int depth);
    const NodeBase* GetNodeBase(blk_pos_t blk_pos) const noexcept {
        return (const NodeBase*)((const u8*)m_btree.GetData().data() + blk_pos * g_block_size);
    }
    str_pos_t GetSAPos(const NodeBase* node, in_blk_pos_t ext_pos);

    const NodeBase* GetLeftmost(const NodeBase* sbt_node_base) {
        using ExtItemT = typename InnerNode::ExtItemT;

        while (sbt_node_base->IsInner()) {
            PtWrapperT pt = GetPT(sbt_node_base);
            const auto* pt_root = pt.GetRoot();

            const auto ext_begin = pt.GetExtPos();
            auto ext_item = misalign_load<ExtItemT>((const u8*)pt_root + ext_begin);

            sbt_node_base = GetNodeBase(ext_item.child);
        }

        return sbt_node_base;
    }

private:
    FileMapperRead m_btree;
    const u8* m_root;

    // Cache
    str_pos_t m_leftmost_str;
    str_pos_t m_rightmost_str;
    const LeafNode* m_leftmost_leaf;
};

template <typename CharT>
blk_pos_t StringBTree<CharT>::CalcCommonNumBlock(str_len_t num_string) {
    // Spec case
    if (num_string <= LeafNode::num_leaves) {
        return 1;  // Need only root node as LeafNode
    }

    uint64_t num_blocks = DivUp(num_string, LeafNode::num_leaves);
    uint64_t num_blocks_last_layer = num_blocks;
    while (num_blocks_last_layer != 1) {
        num_blocks_last_layer = DivUp(num_blocks_last_layer, InnerNode::num_leaves / 2);
        num_blocks += num_blocks_last_layer;
    }

    if (num_blocks > std::numeric_limits<blk_pos_t>::max()) {
        throw std::runtime_error("Number blocks more then MAX_BLK_POS");
    }

    return num_blocks;
}

template <typename CharT>
template <typename AccessorT>
StringBTree<CharT> StringBTree<CharT>::StringBTree::Build(std::string sbt_dest_path,
                                                          const AccessorT& dna_data,
                                                          std::string dna_data_sa_path) {
    ObjectFileHolder suff_arr_holder{dna_data_sa_path};
    const str_pos_t* suff_arr = (const str_pos_t*)suff_arr_holder.cbegin();
    const str_len_t suff_arr_size = suff_arr_holder.Size();

    if (dna_data.Size() != suff_arr_size) {
        throw std::runtime_error("Invalid size text and suffix array");
    }

    const blk_pos_t num_blocks = CalcCommonNumBlock(suff_arr_size);
    FileMapperWrite sbt_file{sbt_dest_path, num_blocks * g_block_size};
    auto [dest_data, dest_size] = sbt_file.GetData();
    std::cout << "SBT file size: " << dest_size << std::endl;

    // Build leaves level
    const str_len_t num_strings = suff_arr_size;

    u8* cur_dest = dest_data;
    const blk_pos_t num_leaf_node = DivUp(num_strings, LeafNode::num_leaves);

    // Split lay on: [left] [left] [left] ... [right] [right], right >= left
    str_len_t node_num_str_left = num_strings / num_leaf_node;
    str_len_t node_num_str_right = node_num_str_left + 1;
    str_len_t num_right = num_strings - node_num_str_left * num_leaf_node;

    str_pos_t i_str_begin = 0;
    std::vector<typename InnerNode::ExtItemT> exts(num_leaf_node);
    str_pos_t suff_arr_left_size_prev = 0;
    for (blk_pos_t i_node_leaf = 0; i_node_leaf < num_leaf_node; ++i_node_leaf) {
        LeafNode* node = new (cur_dest) LeafNode;
        cur_dest = (u8*)node + g_block_size;

        str_len_t node_num_str =
            i_node_leaf < (num_leaf_node - num_right) ? node_num_str_left : node_num_str_right;

        std::vector<std::pair<str_pos_t, in_blk_pos_t>> strs(node_num_str);

        typename LeafNode::ExtItemT* ext_poss = node->ExtBegin();
        u8* addr_begin = (u8*)&node->PT;

        str_pos_t i_str_end = i_str_begin + node_num_str;
        for (str_len_t i_str = i_str_begin; i_str < i_str_end; ++i_str) {
            str_pos_t i = i_str - i_str_begin;
            ext_poss[i].str_pos = suff_arr[i_str];
            strs[i] = {(str_pos_t)ext_poss[i].str_pos, (u8*)&ext_poss[i].str_pos - addr_begin};
        }

        i_str_begin += node_num_str;

        PT_T::BuildAndEmplacePT(dna_data, strs, addr_begin, sizeof(node->PT));

        node->suff_arr_left_size = suff_arr_left_size_prev;
        suff_arr_left_size_prev += node_num_str;

        if (num_leaf_node > 1) {
            exts[i_node_leaf] = {ext_poss[0].str_pos, ext_poss[node_num_str - 1].str_pos,
                                 i_node_leaf};
        }

        if constexpr (SBT_BUILD_LOG) {
            if (i_node_leaf % (10 * num_leaf_node / 100) == 0) {
                double proc = 100 * double(i_node_leaf) / num_leaf_node;
                std::cout << i_node_leaf << " / " << num_leaf_node << " -> " << proc << "%\n";
            }
        }
    }

    if (num_leaf_node == 1) {
        return {sbt_dest_path};
    }

    str_pos_t prev_layer_num_node = num_leaf_node;

    if constexpr (SBT_BUILD_LOG) {
        std::cout << "Start build inner layers\n";
    }

    while (true) {
        // Build inner layer

        str_pos_t layer_num_node = DivUp(prev_layer_num_node, InnerNode::num_leaves / 2);
        str_len_t num_child_left = prev_layer_num_node / layer_num_node;
        str_len_t num_child_right = num_child_left + 1;
        str_len_t num_right = prev_layer_num_node - num_child_left * layer_num_node;

        auto ext_it = exts.cbegin();
        blk_pos_t layer_blk_pos_begin = exts.back().child + 1;
        for (blk_pos_t i_node = 0; i_node < layer_num_node; ++i_node) {
            InnerNode* node = new (cur_dest) InnerNode;
            cur_dest = (u8*)node + g_block_size;

            str_len_t num_child =
                i_node < (layer_num_node - num_right) ? num_child_left : num_child_right;

            std::vector<std::pair<str_pos_t, in_blk_pos_t>> strs(2 * num_child);

            typename InnerNode::ExtItemT* ext_poss = node->ExtBegin();
            u8* addr_begin = (u8*)&node->PT;

            for (str_len_t i_child = 0; i_child < num_child; ++i_child) {
                auto& ext_pos = ext_poss[i_child] = *ext_it++;

                strs[2 * i_child + 0] = {(str_pos_t)ext_pos.left_str_pos,
                                         (u8*)&ext_pos.left_str_pos - addr_begin};
                strs[2 * i_child + 1] = {(str_pos_t)ext_pos.right_str_pos,
                                         (u8*)&ext_pos.right_str_pos - addr_begin};
            }

            PT_T::BuildAndEmplacePT(dna_data, strs, addr_begin, sizeof(node->PT));

            if (layer_num_node > 1) {  // Is not root
                exts[i_node] = {ext_poss[0].left_str_pos, ext_poss[num_child - 1].right_str_pos,
                                layer_blk_pos_begin + i_node};
            }
        }

        if (ext_it != exts.cend()) {
            std::cout << std::distance(exts.cbegin(), ext_it) << std::endl;
            throw std::runtime_error{"Incorrect construct SBT"};
        }

        if (layer_num_node == 1) {  // Is root
            break;
        }

        exts.resize(layer_num_node);
        prev_layer_num_node = layer_num_node;
    }

    return {sbt_dest_path};
}

template <typename CharT>
StringBTree<CharT>::StringBTree(std::string sbt_path)
    : m_btree{sbt_path} {
    auto sv_btree = m_btree.GetData();
    const auto btree_num_blocks = sv_btree.size() / g_block_size;

    if (sv_btree.size() % g_block_size) {
        throw std::runtime_error{"Size of btree not div on block size!"};
    }

    if (btree_num_blocks > 1) {
        m_root = (const u8*)sv_btree.end() - g_block_size;
    } else if (btree_num_blocks == 1) {
        m_root = (const u8*)sv_btree.data();
    } else {
        throw std::runtime_error{"Size of btree too small"};
    }

    const NodeBase* root_node_base = (const NodeBase*)m_root;
    PtWrapperT pt = GetPT(root_node_base);

    m_leftmost_str = pt.GetLeftmostStr();
    m_rightmost_str = pt.GetRightmostStr();

    // Init leftmost leaf
    const NodeBase* node_base = (const NodeBase*)m_root;
    while (node_base->IsInner()) {
        const InnerNode* inn_node = (const InnerNode*)node_base;
        blk_pos_t leftmost_child_pos = inn_node->ExtBegin()->child;
        node_base = GetNodeBase(leftmost_child_pos);
    }
    m_leftmost_leaf = (const LeafNode*)node_base;
}

template <typename CharT>
void StringBTree<CharT>::DumpImpl(const NodeBase* node_base, int depth) {
    for (int i = 0; i < depth; i++) {
        std::cout << "  ";
    }
    if (node_base->type == NodeBase::Type::Leaf) {
        std::cout << "leaf\n";
    } else if (node_base->type == NodeBase::Type::Inner) {
        std::cout << "inner\n";
    } else {
        throw std::runtime_error{"Incorrect node type"};
    }

    if (node_base->type == NodeBase::Type::Inner) {
        const auto* sbt_node = (const InnerNode*)node_base;
        const auto* pt_root = (const typename PT_T::InnerNode*)&sbt_node->PT;

        uint num_ext = 0;
        const typename PT_T::InnerNode* pt_node = pt_root;
        while (true) {
            const auto* branchs = (const typename PT_T::Branch*)(pt_node + 1);
            typename PT_T::Branch branch = branchs[pt_node->num_branch - 1];
            if (branch.node_pos >= sbt_node->GetExtPosBegin()) {
                num_ext = branch.node_pos + sizeof(str_pos_t) + sizeof(blk_pos_t) -
                          sbt_node->GetExtPosBegin();

                if (num_ext % sizeof(ExtItem<false>)) {
                    throw std::runtime_error{"Incorrect ext pos"};
                }

                num_ext /= sizeof(ExtItem<false>);
                break;
            }

            pt_node = (const typename PT_T::InnerNode*)((const u8*)pt_root + branch.node_pos);
        }

        if (num_ext % 2) {
            throw std::runtime_error{"In inner node number leaf not even!"};
        }

        const auto* ext_begin = (const ExtItem<false>*)&sbt_node->Ext;
        for (uint i_ext = 0; i_ext < num_ext; ++i_ext) {
            const auto* ext = (ext_begin + i_ext);
            const u8* data_begin = (const u8*)m_btree.GetData().data();

            const NodeBase* child_base = (const NodeBase*)(data_begin + ext->child * g_block_size);
            DumpImpl(child_base, depth + 1);
        }
    }
}

template <typename CharT>
template <typename AccessorT>
std::tuple<str_pos_t, str_pos_t, str_pos_t, str_len_t> StringBTree<CharT>::Search(
    const AccessorT& pattern, const AccessorT& dna) {
    {
        // pattern <= m_leftmost_str
        str_len_t len = std::min(pattern.Size(), dna.StrSize(m_leftmost_str));
        str_pos_t i = 0;
        for (; i < len; ++i) {
            CharT patt_symb = pattern[i];
            CharT left_symb = dna[m_leftmost_str + i];
            if (patt_symb != left_symb) {
                if (patt_symb < left_symb) {
                    return {m_leftmost_str, 0, 0, true};
                }
                break;
            }
        }
        if (i == pattern.Size()) {
            return {m_leftmost_str, 0, 0, true};
        }
    }

    {
        // pattern > m_rightmost_str
        str_len_t len = std::min(pattern.Size(), dna.StrSize(m_rightmost_str));
        str_pos_t i = 0;
        for (; i < len; ++i) {
            CharT patt_symb = pattern[i];
            CharT right_symb = dna[m_rightmost_str + i];
            if (patt_symb != right_symb) {
                if (patt_symb > right_symb) {
                    str_pos_t sa_pos = dna.Size() - 1;
                    return {m_rightmost_str, sa_pos, sa_pos + 1, true};
                }
                break;
            }
        }
    }

    bool is_right_finded = false;

    auto correct_ext_pos = [](in_blk_pos_t& ext_pos, in_blk_pos_t ext_begin) {
        using ExtItemT = typename InnerNode::ExtItemT;
        in_blk_pos_t local_ext_pos = (ext_pos - ext_begin) % sizeof(ExtItemT);
        if (local_ext_pos > sizeof(str_pos_t)) {
            ext_pos += sizeof(ExtItemT) - local_ext_pos;
            local_ext_pos = 0;
        }

        return local_ext_pos;
    };

    str_pos_t str_pos = 0;
    str_len_t cur_lcp = 0;
    in_blk_pos_t ext_pos_res = 0;
    in_blk_pos_t ext_pos_right_res = 0;
    const NodeBase* sbt_node_base = (const NodeBase*)m_root;
    const NodeBase* sbt_node_base_right = nullptr;
    while (true) {
        PtWrapperT pt = GetPT(sbt_node_base);

        const auto* pt_root = pt.GetRoot();
        const auto ext_begin = pt.GetExtPos();

        auto [ext_pos, lcp] = PT_T::Search(pattern, pt_root, cur_lcp, ext_begin, dna);
        cur_lcp = lcp;

        /*
            /^^\       /======\       /^^\
            & -------- | TODO | -------- &
            \__/       \======/       \__/
             ##                        ##
             ::                        ::
             ''                        ''

            Fix find sa_pos_right when search is unalign ---v
        */

        if (!is_right_finded && lcp + 1 >= pattern.Size()) {
            is_right_finded = lcp == pattern.Size();

            auto [right_ext_pos, is_rightmost] =
                PT_T::RSearch(pattern, pt_root, lcp, ext_begin, dna);
            if (is_rightmost) {
                ext_pos_right_res = 0;  // Spec value
            } else {
                ext_pos_right_res = right_ext_pos;
                sbt_node_base_right = sbt_node_base;
            }
        }

        if (sbt_node_base->IsInner()) {
            in_blk_pos_t local_ext_pos = correct_ext_pos(ext_pos, ext_begin);

            using ExtItemT = typename InnerNode::ExtItemT;
            auto ext_item = misalign_load<ExtItemT>((const u8*)pt_root + ext_pos - local_ext_pos);
            if (local_ext_pos == 0) {
                // L
                str_pos = pt.GetStr(ext_pos);

                sbt_node_base = GetNodeBase(ext_item.child);
                sbt_node_base = GetLeftmost(sbt_node_base);
                ext_pos_res = GetPT(sbt_node_base).GetExtPos();
                break;
            } else if (local_ext_pos == sizeof(str_pos_t)) {
                // R
                sbt_node_base = GetNodeBase(ext_item.child);
            } else {
                throw std::runtime_error{"Internal error"};
            }
        } else {
            str_pos = pt.GetStr(ext_pos);
            ext_pos_res = ext_pos;
            break;
        }
    }

    str_pos_t sa_pos = GetSAPos(sbt_node_base, ext_pos_res);

    str_pos_t sa_pos_right = 0;
    if (ext_pos_right_res) {
        sbt_node_base = sbt_node_base_right;
        in_blk_pos_t ext_pos = ext_pos_right_res;

        if (sbt_node_base->IsInner()) {
            using ExtItemT = typename InnerNode::ExtItemT;

            PtWrapperT pt = GetPT(sbt_node_base);
            const auto* pt_root = pt.GetRoot();

            const auto ext_begin = pt.GetExtPos();
            in_blk_pos_t local_ext_pos = correct_ext_pos(ext_pos, ext_begin);

            auto ext_item = misalign_load<ExtItemT>((const u8*)pt_root + ext_pos - local_ext_pos);
            sbt_node_base = GetNodeBase(ext_item.child);
            sbt_node_base = GetLeftmost(sbt_node_base);

            ext_pos_right_res = GetPT(sbt_node_base).GetExtPos();
        }

        sa_pos_right = GetSAPos(sbt_node_base, ext_pos_right_res);
    } else {
        sa_pos_right = std::min<str_len_t>(sa_pos + 1, dna.Size() - 1);
    }

    return {str_pos, sa_pos, sa_pos_right, cur_lcp};
}

template <typename CharT>
void StringBTree<CharT>::DumpExt(const NodeBase* node_base) {
    if (node_base->type == NodeBase::Type::Inner) {
        const InnerNode* node = (const InnerNode*)node_base;
        const auto* ext_begin = node->ExtBegin();
        const auto* ext_end = (const ExtItem<false>*)(&node->Ext + 1);
        for (auto ext_it = ext_begin; ext_it < ext_end; ++ext_it) {
            if (ext_it != ext_begin) {
                std::cout << ", ";
            }
            std::cout << ext_it->left_str_pos << ", " << ext_it->right_str_pos;
        }
    } else {
        const LeafNode* node = (const LeafNode*)node_base;
        const auto* ext_begin = node->ExtBegin();
        const auto* ext_end = (const ExtItem<true>*)(&node->Ext + 1);
        for (auto ext_it = ext_begin; ext_it < ext_end; ++ext_it) {
            if (ext_it != ext_begin) {
                std::cout << ", ";
            }
            std::cout << ext_it->str_pos;
        }
    }
}

template <typename CharT>
str_pos_t StringBTree<CharT>::GetSAPos(const NodeBase* node, in_blk_pos_t ext_pos) {
    assert(node->IsLeaf());

    const auto global_sa_pos = node->suff_arr_left_size;

    const LeafNode* leaf_node = (const LeafNode*)node;
    const auto local_sa_pos =
        (ext_pos - leaf_node->GetExtPosBegin()) / sizeof(LeafNode::ExtItemT::str_pos);

    return global_sa_pos + local_sa_pos;
}

}  // namespace DNA_SBT
