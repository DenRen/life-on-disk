#pragma once

#include "../common/file_manip.h"
#include "../common/help_func.h"
#include "dna.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <map>
#include <fstream>

namespace DNA_PT {

/*
    In PT not external node beacuse pre leaf node of PT
    will be point to obect of SBT, whose position will
    greater then some number, but less 2^sizeof(node_pos_t)
*/

template <typename CharT>
class PT {
public:
    PACKED_STRUCT Branch final {  // Between inner nodes
        CharT symb;
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

    constexpr static uint CalcMaxSize(uint num_leafs) noexcept {
        // Higher trie
        uint num_inner_nodes = num_leafs - 1;
        uint branch_per_node = 2;
        return num_inner_nodes * (sizeof(InnerNode) + branch_per_node * sizeof(Branch));
    }

    struct SearchResult final {
        in_blk_pos_t ext_node_pos;
        str_len_t lcp;
    };

    template <typename AccessorT>
    static SearchResult Search(const AccessorT& pattern, const InnerNode* root, str_len_t last_lcp,
                               in_blk_pos_t ext_pos_begin, const AccessorT& dna_text);

    template <typename AccessorT>
    static std::pair<in_blk_pos_t, bool> RSearch(const AccessorT& pattern, const InnerNode* root,
                                                 str_len_t last_lcp, in_blk_pos_t ext_pos_begin,
                                                 const AccessorT& dna_text);

    static const Branch* LowerBound(const Branch* begin, const Branch* end, CharT symb) noexcept {
        return std::lower_bound(begin, end, symb, [](const Branch& lhs, CharT cur_symb) {
            return lhs.symb < cur_symb;
        });
    }

    template <typename AccessorT>
    static void BuildAndEmplacePT(const AccessorT& dna,
                                  const std::vector<std::pair<str_pos_t, in_blk_pos_t>>& strs,
                                  u8* dest, size_t dest_size);

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
};

namespace detail {

template <typename CharT>
class PatriciaTrieNaive {
    struct Node {
        enum class Type : u8 { Inner, Leaf };

        Node(str_len_t len, Type type)
            : len{len}
            , type{type} {}

        str_len_t len;
        Type type;

        bool IsInner() const noexcept {
            return type == Type::Inner;
        }
        bool IsLeaf() const noexcept {
            return type == Type::Leaf;
        }
    };

    struct InnerNode : public Node {
        InnerNode(str_len_t len)
            : Node{len, Node::Type::Inner} {}

        std::map<CharT, Node*> childs;
    };

    struct LeafNode : public Node {
        LeafNode(str_len_t len, str_pos_t dna_pos, in_blk_pos_t ext_pos)
            : Node{len, Node::Type::Leaf}
            , dna_pos{dna_pos}
            , ext_pos{ext_pos} {}

        str_pos_t dna_pos;
        in_blk_pos_t ext_pos;
    };

public:
    ~PatriciaTrieNaive() {
        DestructInnerNode(m_root);
    }

    template <typename AccessorT>
    void Insert(const AccessorT& dna, str_pos_t dna_pos, in_blk_pos_t ext_node_pos);
    std::string DrawTrie() const {
        return DrawTrie(m_root);
    }
    void EmplaceOn(u8* dest, in_blk_pos_t dest_size) {
        try {
            uint n_node = 0;
            EmplaceInnerNode(dest, dest_size, dest, m_root, n_node);
        } catch (...) {
            std::ofstream{"DestEmpty.dot"} << DrawTrie();
            throw;
        }
    }

private:
    static void DestructInnerNode(InnerNode* node) noexcept;
    static u8* EmplaceInnerNode(u8* dest, in_blk_pos_t& dest_size, u8* dest_begin,
                                const InnerNode* node_src, uint& n_node);

    static std::string DrawTrie(const InnerNode* root);
    static void DrawTrieImpl(const Node* node, std::string& init_nodes, std::string& conns);

    template <typename AccessorT>
    std::tuple<InnerNode*, str_len_t, CharT> SearchInsertNode(const AccessorT& dna,
                                                              str_pos_t dna_pos);
    static LeafNode* FindLeftmostLeaf(Node* node);

private:
    InnerNode* m_root = new InnerNode{0};
};

template <typename CharT>
void PatriciaTrieNaive<CharT>::DestructInnerNode(InnerNode* node) noexcept {
    for (auto [branch_symb, child] : node->childs) {
        if (child->IsLeaf()) {
            delete (LeafNode*)child;
        } else {
            DestructInnerNode((InnerNode*)child);
        }
    }

    delete node;
}

template <typename CharT>
std::string PatriciaTrieNaive<CharT>::DrawTrie(const InnerNode* root) {
    std::string dot =
        "digraph {\n"
        "\tgraph [rankdir = \"TB\"];\n"
        "\tnode [shape = record];";

    std::string init_nodes, conns;
    DrawTrieImpl(root, init_nodes, conns);

    dot += "\n";
    dot += init_nodes;
    dot += "\n\n";
    dot += conns;

    dot += "\n}\n";
    return dot;
}

template <typename CharT>
void PatriciaTrieNaive<CharT>::DrawTrieImpl(const Node* node, std::string& init_nodes,
                                            std::string& conns) {
    auto get_node_name = [&](const Node* node) {
        return "node" + std::to_string((uint64_t)node);
    };

    std::string node_name = get_node_name(node);
    if (node->IsInner()) {
        init_nodes += "\n\t" + node_name + "[label=\"";

        const InnerNode& inn_node = *(const InnerNode*)node;
        bool is_first = true;
        for (const auto [branch_symb, child] : inn_node.childs) {
            if (!is_first) {
                init_nodes += " | ";
            }
            is_first = false;

            std::string symb = DNASymb2String(branch_symb);
            if (symb == "\\0") {
                symb = "$";
            }
            init_nodes += "<" + symb + "> " + symb;

            std::string child_name = get_node_name(child);
            conns += "\t\"" + node_name + "\":<" + symb + ">->\"";
            conns += child_name + '\"';
            conns += "[label=\"" + std::to_string(child->len) + "\"];\n";
        }
        init_nodes += "\"];";

        for (const auto [branch_symb, child] : inn_node.childs) {
            DrawTrieImpl(child, init_nodes, conns);
        }
    } else if (node->IsLeaf()) {
        const LeafNode& ext_node = *(const LeafNode*)node;

        init_nodes += "\n\t" + node_name + "[label=\"";
        init_nodes +=
            std::to_string(ext_node.len) /* + '\n' + std::string(str) */ + "\" shape=egg ];\n";
    } else {
        throw std::runtime_error("Invalid node type");
    }
}

template <typename CharT>
template <typename AccessorT>
void PatriciaTrieNaive<CharT>::Insert(const AccessorT& dna, str_pos_t dna_pos,
                                      in_blk_pos_t ext_node_pos) {
    auto [ins_node, lcp, symb_old] = SearchInsertNode(dna, dna_pos);
    if (lcp == dna.StrSize(dna_pos)) {
        return;
    }

    auto& branchs = ins_node->childs;
    Node* new_leaf = new LeafNode{(str_len_t)dna.StrSize(dna_pos), dna_pos, ext_node_pos};

    auto it = branchs.find(dna[dna_pos + ins_node->len]);
    if (it == branchs.end()) {
        branchs.insert({dna[dna_pos + lcp], new_leaf});
    } else {
        InnerNode* new_inn_node = new InnerNode{lcp};
        new_inn_node->childs.emplace(dna[dna_pos + lcp], new_leaf);
        new_inn_node->childs.emplace(symb_old, it->second);
        it->second = new_inn_node;
    }
}

template <typename CharT>
template <typename AccessorT>
std::tuple<typename PatriciaTrieNaive<CharT>::InnerNode*, str_len_t, CharT>
PatriciaTrieNaive<CharT>::SearchInsertNode(const AccessorT& dna, str_pos_t dna_pos) {
    Node* node = m_root;
    while (true) {
        assert(node->IsInner());
        InnerNode* inn_node = (InnerNode*)node;

        auto& branchs = inn_node->childs;
        auto it = branchs.find(dna[dna_pos + inn_node->len]);
        if (it == branchs.end()) {
            return {inn_node, inn_node->len, {}};
        } else {
            Node* child = it->second;
            LeafNode* leftmost_leaf = FindLeftmostLeaf(child);
            str_pos_t down_dna_pos = leftmost_leaf->dna_pos;

            str_len_t min_len = std::min((str_len_t)dna.Size(), child->len);
            str_len_t lcp = inn_node->len;
            for (; lcp < min_len; ++lcp) {
                if (dna[dna_pos + lcp] != dna[down_dna_pos + lcp]) {
                    break;
                }
            }

            if (lcp < child->len || child->IsLeaf()) {
                return {inn_node, lcp, dna[down_dna_pos + lcp]};
            }

            node = child;
        }
    }
}

template <typename CharT>
typename PatriciaTrieNaive<CharT>::LeafNode* PatriciaTrieNaive<CharT>::FindLeftmostLeaf(
    Node* node) {
    while (!node->IsLeaf()) {
        assert(node->IsInner());

        InnerNode* inn_node = (InnerNode*)node;
        assert(!inn_node->childs.empty());

        node = inn_node->childs.begin()->second;
    }

    return (LeafNode*)node;
}

template <typename CharT>
u8* PatriciaTrieNaive<CharT>::EmplaceInnerNode(u8* dest, in_blk_pos_t& dest_size, u8* dest_begin,
                                               const InnerNode* node_src, uint& n_node) {
    auto* node_dest = (typename PT<CharT>::InnerNode*)dest;

    auto* branch = (typename PT<CharT>::Branch*)(node_dest + 1);
    auto* branch_end = branch + node_src->childs.size();

    const in_blk_pos_t emplace_size = (u8*)branch_end - dest;
    if (emplace_size > dest_size) {
        throw std::runtime_error("Destinantion is empty! n_node: " + std::to_string(n_node));
    }
    dest_size -= emplace_size;

    *node_dest = {node_src->len, (alph_size_t)node_src->childs.size()};
    ++n_node;

    dest = (u8*)branch_end;
    for (auto [branch_symb, child] : node_src->childs) {
        branch->symb = branch_symb;

        if (child->IsInner()) {
            branch->node_pos = dest - dest_begin;
            dest = EmplaceInnerNode(dest, dest_size, dest_begin, (InnerNode*)child, n_node);
        } else if (child->IsLeaf()) {
            const auto* leaf = (const LeafNode*)child;
            branch->node_pos = leaf->ext_pos;
            ++n_node;
        } else {
            throw std::runtime_error("Undefined node type");
        }

        ++branch;
    }

    return dest;
}

}  // namespace detail

template <typename CharT>
template <typename AccessorT>
void PT<CharT>::BuildAndEmplacePT(const AccessorT& dna,
                                  const std::vector<std::pair<str_pos_t, in_blk_pos_t>>& strs,
                                  u8* dest, size_t dest_size) {
    // Build PT in-memory and convert one to on-disk format
    detail::PatriciaTrieNaive<CharT> pt;
    for (auto [dna_pos, ext_node_pos] : strs) {
        pt.Insert(dna, dna_pos, ext_node_pos);
        // std::cout << "ins: \"" << str << "\"\n";
    }

    // static int ctr = 0;
    // std::ofstream{"Native" + std::to_string(ctr++) + ".dot"} << pt.DrawTrie();

    pt.EmplaceOn(dest, dest_size);
}

template <typename CharT>
template <typename AccessorT>
typename PT<CharT>::SearchResult PT<CharT>::Search(const AccessorT& pattern, const InnerNode* root,
                                                   str_len_t last_lcp, in_blk_pos_t ext_pos_begin,
                                                   const AccessorT& dna) {
    const InnerNode* node = root;
    in_blk_pos_t ext_pos = 0;

    PT<CharT>::Wrapper pt{root, ext_pos_begin};

    // First phase
    while (true) {
        CharT cur_symb = pattern.Size() > node->len ? pattern[node->len] : CharT{};

        const Branch* branchs_begin = node->GetBranchs();
        const Branch* branchs_end = branchs_begin + node->num_branch;
        const Branch* branch = LowerBound(branchs_begin, branchs_end, cur_symb);
        if (branch != branchs_end) {
            if (branch->symb == cur_symb) {
                if (branch->node_pos < ext_pos_begin) {
                    node = pt.GetNode(branch->node_pos);
                } else {
                    ext_pos = branch->node_pos;
                    break;
                }
            } else {
                ext_pos = pt.GetLeftmostExt(node);
                break;
            }
        } else {
            ext_pos = pt.GetLeftmostExt(node);
            break;
        }
    }

    str_pos_t str_pos = misalign_load<str_pos_t>((const u8*)root + ext_pos);

    // Find lcp
    str_len_t lcp = last_lcp;
    str_len_t max_lcp = std::min<str_len_t>(pattern.Size(), dna.StrSize(str_pos));
    for (; lcp < max_lcp; ++lcp) {
        if (pattern[lcp] != dna[str_pos + lcp]) {
            break;
        }
    }

    // Find hit node as position in PT
    in_blk_pos_t hit_node_pos = 0;
    node = root;
    while (true) {
        if (node->len >= lcp) {
            hit_node_pos = pt.GetNodePos(node);
            break;
        }

        CharT cur_symb = pattern.Size() > node->len ? pattern[node->len] : CharT{};

        const Branch* branchs_begin = node->GetBranchs();
        const Branch* branchs_end = branchs_begin + node->num_branch;
        const Branch* branch = LowerBound(branchs_begin, branchs_end, cur_symb);
        assert(branch != branchs_end && branch->symb == cur_symb);

        if (branch->node_pos >= ext_pos_begin) {
            hit_node_pos = branch->node_pos;
            break;
        }

        node = pt.GetNode(branch->node_pos);
    }

    // Second phase
    if (hit_node_pos < ext_pos_begin) {  // Hit node is not leaf
        CharT pat_symb = lcp < pattern.Size() ? pattern[lcp] : CharT{};
        CharT text_symb = dna[str_pos + lcp];
        node = pt.GetNode(hit_node_pos);
        const Branch* branchs_begin = node->GetBranchs();
        const Branch* branchs_end = branchs_begin + node->num_branch;

        if (lcp == node->len) {
            if (pat_symb < branchs_begin->symb) {
                ext_pos = pt.GetLeftmostExt(node);
            } else if ((branchs_end - 1)->symb < pat_symb) {
                ext_pos = pt.GetRightmostExt(node) + sizeof(str_pos);
            } else {
                const Branch* branch = LowerBound(branchs_begin, branchs_end, pat_symb);
                ext_pos = pt.GetLeftmostExt(branch);
            }
        } else {  // lcp < node->len
            if (pat_symb < text_symb) {
                ext_pos = pt.GetLeftmostExt(node);
            } else {
                ext_pos = pt.GetRightmostExt(node) + sizeof(str_pos);
            }
        }
    } else {  // Hit node is leaf
        if (lcp == node->len) {
            ext_pos = hit_node_pos;
        } else {
            CharT pat_symb = pattern[lcp];
            CharT dna_symb = dna[str_pos + lcp];
            assert(pat_symb != dna_symb);
            if (pat_symb < dna_symb) {
                ext_pos = hit_node_pos;
            } else {
                // Next ext_pos
                ext_pos = hit_node_pos + sizeof(str_pos);  // Fucking prototyping!!!
            }
        }
    }

    return SearchResult{.ext_node_pos = ext_pos, .lcp = lcp};
}

// Return: ext_pos and is_rightmost for r from [l, r]
template <typename CharT>
template <typename AccessorT>
std::pair<in_blk_pos_t, bool> PT<CharT>::RSearch(const AccessorT& pattern, const InnerNode* root,
                                                 str_len_t last_lcp, in_blk_pos_t ext_pos_begin,
                                                 const AccessorT& dna) {
    const InnerNode* node = root;
    in_blk_pos_t ext_pos = 0;

    PT<CharT>::Wrapper pt{root, ext_pos_begin};

    std::vector<const InnerNode*> path;

    str_len_t cur_len = 0;
    while (true) {
        if (pattern.Size() <= cur_len) {
            node = path.back();
            path.pop_back();

            while (true) {
                const Branch* branchs_begin = node->GetBranchs();
                const Branch* branchs_end = branchs_begin + node->num_branch;
                const Branch* branch = LowerBound(branchs_begin, branchs_end, pattern[node->len]);

                assert(branch != branchs_end);
                if (branch + 1 == branchs_end) {
                    if (path.empty()) {
                        return {0, true};
                    }

                    node = path.back();
                    path.pop_back();
                } else {
                    ext_pos = pt.GetLeftmostExt(branch + 1);
                    return {ext_pos, false};
                }
            }
        }

        path.push_back(node);

        CharT cur_symb = pattern[node->len];

        const Branch* branchs_begin = node->GetBranchs();
        const Branch* branchs_end = branchs_begin + node->num_branch;
        const Branch* branch = LowerBound(branchs_begin, branchs_end, cur_symb);

        if (branch != branchs_end) {
            bool is_inner_node_pos = branch->node_pos < ext_pos_begin;
            if (is_inner_node_pos) {
                node = pt.GetNode(branch->node_pos);
                cur_len = node->len;
            } else {
                str_pos_t str_pos = misalign_load<str_pos_t>((const u8*)root + ext_pos);
                cur_len = dna.StrSize(str_pos);
            }
        } else {
            return {0, true};
        }
    }

    assert(0);
}

}  // namespace DNA_PT
