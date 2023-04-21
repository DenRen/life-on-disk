#include "string_btree.h"

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
                               const std::vector<str_len_t>& suff_arr) {
    FileMapperRead text_mapper{path_text};
    if (text_mapper.GetData().size() != suff_arr.size()) {
        throw std::runtime_error("Invalid size text and suff");
    }

    std::string_view text = text_mapper.GetData();

    const blk_pos_t num_blocks = CalcCommonNumBlock((str_len_t)suff_arr.size());
    FileMapperWrite sbt_file(sbt_dest_path, num_blocks * g_block_size);
    auto [dest_data, dest_size] = sbt_file.GetData();
    std::cout << "SBT file size: " << dest_size << std::endl;

    // Build leaves level
    const str_len_t num_strings = suff_arr.size();

    u8* cur_dest = dest_data;
    const blk_pos_t num_leaf_node = DivUp(num_strings, LeafNode::num_leaves);

    // Split lay on: [left] [left] [left] ... [left] [right], right >= left
    str_len_t node_num_str_left = num_strings / DivUp(num_strings, LeafNode::num_leaves);
    str_len_t node_num_str_right =
        node_num_str_left + num_strings % DivUp(num_strings, LeafNode::num_leaves);

    std::vector<ExtItem<false>> exts(num_leaf_node);

    for (blk_pos_t i_node_leaf = 0; i_node_leaf < num_leaf_node; ++i_node_leaf) {
        LeafNode* node = new (cur_dest) LeafNode;
        cur_dest = (u8*)(node + 1);

        str_len_t node_num_str =
            i_node_leaf + 1 < num_leaf_node ? node_num_str_left : node_num_str_right;

        std::vector<std::pair<std::string_view, in_blk_pos_t>> strs(node_num_str);

        ExtItem<true>* ext_poss = node->ExtBegin();
        u8* addr_begin = (u8*)&node->PT;

        str_pos_t i_str_begin = i_node_leaf * node_num_str_left;
        str_pos_t i_str_end = i_str_begin + node_num_str;
        for (str_len_t i_str = i_str_begin; i_str < i_str_end; ++i_str) {
            str_pos_t i = i_str - i_str_begin;
            ext_poss[i].str_pos = (str_pos_t)suff_arr[i_str];
            strs[i] = {text.substr(ext_poss[i].str_pos), (u8*)&ext_poss[i].str_pos - addr_begin};
        }

        PT::BuildAndEmplacePT(strs, addr_begin, sizeof(node->PT));

        if (num_leaf_node > 1) {
            exts[i_node_leaf] = {ext_poss[0].str_pos, ext_poss[node_num_str - 1].str_pos,
                                 i_node_leaf};
        }
    }

    if (num_leaf_node <= LeafNode::num_leaves) {
        return {sbt_dest_path, path_text};
    }

    str_pos_t prev_layer_num_node = num_leaf_node;
    while (true) {
        // Build inner layer

        str_pos_t layer_num_node = DivUp(prev_layer_num_node, InnerNode::num_leaves / 2);
        str_len_t num_child_left = prev_layer_num_node / layer_num_node;
        str_len_t num_child_right = num_child_left + prev_layer_num_node % layer_num_node;

        auto ext_it = exts.cbegin();
        blk_pos_t layer_blk_pos_begin = exts.back().child + 1;
        for (blk_pos_t i_node = 0; i_node < layer_num_node; ++i_node) {
            InnerNode* node = new (cur_dest) InnerNode;
            cur_dest = (u8*)(node + 1);

            str_len_t num_child = i_node + 1 < num_leaf_node ? num_child_left : num_child_right;

            std::vector<std::pair<std::string_view, in_blk_pos_t>> strs(2 * num_child);

            ExtItem<false>* ext_poss = node->ExtBegin();
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

        if (layer_num_node == 1) {  // Is root
            break;
        }

        exts.resize(layer_num_node);
        prev_layer_num_node = layer_num_node;
    }

    return {sbt_dest_path, path_text};
}

StringBTree::StringBTree(std::string path_sbt, std::string path_text)
    : btree{path_sbt}
    , text{path_text} {
    auto sv_btree = btree.GetData();
    const auto btree_num_blocks = sv_btree.size() / g_block_size;

    if (sv_btree.size() % g_block_size) {
        throw std::runtime_error{"Size of btree not div on block size!"};
    }

    if (btree_num_blocks > 1) {
        root = (const u8*)sv_btree.end() - g_block_size;
    } else if (btree_num_blocks == 1) {
        root = (const u8*)sv_btree.data();
    } else {
        throw std::runtime_error{"Size of btree too small"};
    }
}

}  // namespace SBT
