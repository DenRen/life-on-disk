#include "string_btree.h"
#include "misalign.h"

#include <limits>
#include <stdexcept>
#include <iostream>

namespace SBT {

template <typename T, typename U>
auto DivUp(T value, U dvider) noexcept {
    return value / dvider + !!(value % dvider);
}

blk_pos_t CalcCommonNumBlock(str_len_t num_string) {
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

StringBTree StringBTree::Build(std::string sbt_dest_path, std::string path_text,
                               const std::vector<str_pos_t>& suff_arr) {
    FileMapperRead text_mapper{path_text};
    if (text_mapper.GetData().size() != suff_arr.size()) {
        throw std::runtime_error("Invalid size text and suff");
    }
    std::string_view text = text_mapper.GetData();

    std::cout << "src ptr: " << (const void*)text.begin() << std::endl;
    std::cout << "src ptr: " << (const void*)text.end() << std::endl;

    const blk_pos_t num_blocks = CalcCommonNumBlock((str_len_t)suff_arr.size());
    FileMapperWrite sbt_file(sbt_dest_path, num_blocks * g_block_size);
    auto [dest_data, dest_size] = sbt_file.GetData();
    std::cout << "SBT file size: " << dest_size << std::endl;

    // Build leaves level
    const str_len_t num_strings = suff_arr.size();

    u8* cur_dest = dest_data;
    const blk_pos_t num_leaf_node = DivUp(num_strings, LeafNode::num_leaves);

    // Split lay on: [left] [left] [left] ... [right] [right], right >= left
    str_len_t node_num_str_left = num_strings / num_leaf_node;
    str_len_t node_num_str_right = node_num_str_left + 1;
    str_len_t num_right = num_strings - node_num_str_left * num_leaf_node;

    str_pos_t i_str_begin = 0;
    std::vector<InnerNode::ExtItemT> exts(num_leaf_node);
    for (blk_pos_t i_node_leaf = 0; i_node_leaf < num_leaf_node; ++i_node_leaf) {
        LeafNode* node = new (cur_dest) LeafNode;
        cur_dest = (u8*)node + g_block_size;

        str_len_t node_num_str =
            i_node_leaf < (num_leaf_node - num_right) ? node_num_str_left : node_num_str_right;

        std::vector<std::pair<std::string_view, in_blk_pos_t>> strs(node_num_str);

        LeafNode::ExtItemT* ext_poss = node->ExtBegin();
        u8* addr_begin = (u8*)&node->PT;

        str_pos_t i_str_end = i_str_begin + node_num_str;
        for (str_len_t i_str = i_str_begin; i_str < i_str_end; ++i_str) {
            str_pos_t i = i_str - i_str_begin;
            ext_poss[i].str_pos = suff_arr[i_str];
            strs[i] = {text.substr(ext_poss[i].str_pos), (u8*)&ext_poss[i].str_pos - addr_begin};
        }
        i_str_begin += node_num_str;

        PT::BuildAndEmplacePT(strs, addr_begin, sizeof(node->PT));

        if (num_leaf_node > 1) {
            exts[i_node_leaf] = {ext_poss[0].str_pos, ext_poss[node_num_str - 1].str_pos,
                                 i_node_leaf};
        }
    }

    if (num_leaf_node == 1) {
        return {sbt_dest_path, path_text};
    }

    str_pos_t prev_layer_num_node = num_leaf_node;

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

            std::vector<std::pair<std::string_view, in_blk_pos_t>> strs(2 * num_child);

            InnerNode::ExtItemT* ext_poss = node->ExtBegin();
            u8* addr_begin = (u8*)&node->PT;

            for (str_len_t i_child = 0; i_child < num_child; ++i_child) {
                auto& ext_pos = ext_poss[i_child] = *ext_it++;

                strs[2 * i_child + 0] = {text.substr(ext_pos.left_str_pos),
                                         (u8*)&ext_pos.left_str_pos - addr_begin};
                strs[2 * i_child + 1] = {text.substr(ext_pos.right_str_pos),
                                         (u8*)&ext_pos.right_str_pos - addr_begin};
            }

            PT::BuildAndEmplacePT(strs, addr_begin, sizeof(node->PT));

            if (layer_num_node > 1) {  // Is not root
                exts[i_node] = {ext_poss[0].left_str_pos, ext_poss[num_child - 1].right_str_pos,
                                layer_blk_pos_begin + i_node};
            }
        }
        std::cout << "R: " << text.substr(exts.back().right_str_pos, 20) << std::endl;
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

    return {sbt_dest_path, path_text};
}

StringBTree::StringBTree(std::string path_sbt, std::string path_text)
    : m_btree{path_sbt}
    , m_text{path_text} {
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
    PT::Wrapper pt = GetPT(root_node_base);

    m_leftmost_str = pt.GetLeftmostStr();
    m_rightmost_str = pt.GetRightmostStr();
}

static void print_shift(int depth) {
    for (int i = 0; i < depth; i++) {
        std::cout << "  ";
    }
}

static void print_node_type(const NodeBase* node_base) {
    if (node_base->type == NodeBase::Type::Leaf) {
        std::cout << "leaf\n";
    } else if (node_base->type == NodeBase::Type::Inner) {
        std::cout << "inner\n";
    } else {
        throw std::runtime_error{"Incorrect node type"};
    }
}

void StringBTree::DumpImpl(const NodeBase* node_base, int depth) {
    print_shift(depth);
    print_node_type(node_base);

    if (node_base->type == NodeBase::Type::Inner) {
        const auto* sbt_node = (const InnerNode*)node_base;
        const auto* pt_root = (const PT::InnerNode*)&sbt_node->PT;

        uint num_ext = 0;
        const PT::InnerNode* pt_node = pt_root;
        while (true) {
            const auto* branchs = (const PT::Branch*)(pt_node + 1);
            PT::Branch branch = branchs[pt_node->num_branch - 1];
            if (branch.node_pos >= sbt_node->GetExtPosBegin()) {
                num_ext = branch.node_pos + sizeof(str_pos_t) + sizeof(blk_pos_t) -
                          sbt_node->GetExtPosBegin();

                if (num_ext % sizeof(ExtItem<false>)) {
                    throw std::runtime_error{"Incorrect ext pos"};
                }

                num_ext /= sizeof(ExtItem<false>);
                break;
            }

            pt_node = (const PT::InnerNode*)((const u8*)pt_root + branch.node_pos);
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

void StringBTree::Dump() {
    DumpImpl((const NodeBase*)m_root, 0);
}

const NodeBase* StringBTree::GetNodeBase(blk_pos_t blk_pos) const noexcept {
    return (const NodeBase*)((const u8*)m_btree.GetData().data() + blk_pos * g_block_size);
}

std::string_view StringBTree::Search(std::string_view pattern) {
    std::string_view text = m_text.GetData();

    std::string_view leftmost_str = text.substr(m_leftmost_str);
    if (pattern <= leftmost_str) {
        return leftmost_str;
    }
    std::string_view rightmost_str = text.substr(m_rightmost_str);
    if (pattern > rightmost_str) {
        return rightmost_str;
    }

    str_pos_t str_pos = 0;
    str_len_t cur_lcp = 0;
    const NodeBase* sbt_node_base = (const NodeBase*)m_root;
    while (true) {
        PT::Wrapper pt = GetPT(sbt_node_base);

        const auto* pt_root = pt.GetRoot();
        const auto ext_begin = pt.GetExtPos();

        auto [ext_pos, lcp] = PT::Search(pattern, pt_root, cur_lcp, ext_begin, text);
        cur_lcp = lcp;

        if (sbt_node_base->IsInner()) {
            // Correct ext_pos
            using ExtItemT = InnerNode::ExtItemT;
            in_blk_pos_t local_ext_pos = (ext_pos - ext_begin) % sizeof(ExtItemT);
            if (local_ext_pos > sizeof(str_pos_t)) {
                ext_pos += sizeof(ExtItemT) - local_ext_pos;
                local_ext_pos = 0;
            }

            auto ext_item = misalign_load<ExtItemT>((const u8*)pt_root + ext_pos - local_ext_pos);
            if (local_ext_pos == 0) {
                // L
                str_pos = pt.GetStr(ext_pos);
                break;
            } else if (local_ext_pos == sizeof(str_pos_t)) {
                // R
                sbt_node_base = GetNodeBase(ext_item.child);
            } else {
                throw std::runtime_error{""};
            }
        } else {
            str_pos = pt.GetStr(ext_pos);
            break;
        }
    }

    return cur_lcp >= pattern.size() ? text.substr(str_pos) : std::string_view{};
}

void DumpExt(const NodeBase* node_base) {
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

PT::Wrapper GetPT(const NodeBase* node_base) noexcept {
    return node_base->IsInner() ? ((const InnerNode*)node_base)->GetPT()
                                : ((const LeafNode*)node_base)->GetPT();
}

}  // namespace SBT
