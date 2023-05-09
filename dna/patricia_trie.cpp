#include "patricia_trie.h"

#include <variant>
#include <map>
#include <stack>
#include <stdexcept>
#include <fstream>
#include <iostream>
#include <limits>

namespace DNA_PT {

namespace detail {

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

        std::map<char_t, Node*> childs;
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
    ~PatriciaTrieNaive();

    void Insert(DnaDataAccessor dna, str_pos_t dna_pos, in_blk_pos_t ext_node_pos);
    std::string DrawTrie() const;
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
    std::tuple<InnerNode*, str_len_t, char_t> SearchInsertNode(DnaDataAccessor dna,
                                                               str_pos_t dna_pos);
    static LeafNode* FindLeftmostLeaf(Node* node);

private:
    InnerNode* m_root = new InnerNode{0};
};

void PatriciaTrieNaive::DestructInnerNode(InnerNode* node) noexcept {
    for (auto [branch_symb, child] : node->childs) {
        if (child->IsLeaf()) {
            delete (LeafNode*)child;
        } else {
            DestructInnerNode((InnerNode*)child);
        }
    }

    delete node;
}

PatriciaTrieNaive::~PatriciaTrieNaive() {
    DestructInnerNode(m_root);
}

std::string PatriciaTrieNaive::DrawTrie(const InnerNode* root) {
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

std::string PatriciaTrieNaive::DrawTrie() const {
    return DrawTrie(m_root);
}

void PatriciaTrieNaive::DrawTrieImpl(const Node* node, std::string& init_nodes,
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

void PatriciaTrieNaive::Insert(DnaDataAccessor dna, str_pos_t dna_pos, in_blk_pos_t ext_node_pos) {
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

std::tuple<PatriciaTrieNaive::InnerNode*, str_len_t, char_t> PatriciaTrieNaive::SearchInsertNode(
    DnaDataAccessor dna, str_pos_t dna_pos) {
    Node* node = m_root;
    while (true) {
        assert(node->IsInner());
        InnerNode* inn_node = (InnerNode*)node;

        auto& branchs = inn_node->childs;
        auto it = branchs.find(dna[dna_pos + inn_node->len]);
        if (it == branchs.end()) {
            return {inn_node, inn_node->len, char_t::TERM};
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

PatriciaTrieNaive::LeafNode* PatriciaTrieNaive::FindLeftmostLeaf(Node* node) {
    while (!node->IsLeaf()) {
        assert(node->IsInner());

        InnerNode* inn_node = (InnerNode*)node;
        assert(!inn_node->childs.empty());

        node = inn_node->childs.begin()->second;
    }

    return (LeafNode*)node;
}

u8* PatriciaTrieNaive::EmplaceInnerNode(u8* dest, in_blk_pos_t& dest_size, u8* dest_begin,
                                        const InnerNode* node_src, uint& n_node) {
    auto* node_dest = (DNA_PT::InnerNode*)dest;

    auto* branch = (Branch*)(node_dest + 1);
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

void BuildAndEmplacePT(DnaDataAccessor dna,
                       const std::vector<std::pair<str_pos_t, in_blk_pos_t>>& strs, u8* dest,
                       size_t dest_size) {
    // Build PT in-memory and convert one to on-disk format
    detail::PatriciaTrieNaive pt;
    for (auto [dna_pos, ext_node_pos] : strs) {
        pt.Insert(dna, dna_pos, ext_node_pos);
        // std::cout << "ins: \"" << str << "\"\n";
    }

    // static int ctr = 0;
    // std::ofstream{"Native" + std::to_string(ctr++) + ".dot"} << pt.DrawTrie();

    pt.EmplaceOn(dest, dest_size);
}

const Branch* LowerBound(const Branch* begin, const Branch* end, char_t symb) {
    return std::lower_bound(begin, end, symb, [](const Branch& lhs, char_t cur_symb) {
        return lhs.symb < cur_symb;
    });
}

SearchResult Search(DnaDataAccessor pattern, const InnerNode* root, str_len_t last_lcp,
                    in_blk_pos_t ext_pos_begin, DnaDataAccessor dna) {
    const InnerNode* node = root;
    in_blk_pos_t ext_pos = 0;

    DNA_PT::Wrapper pt{root, ext_pos_begin};

    // First phase
    while (true) {
        char_t cur_symb = pattern.Size() > node->len ? pattern[node->len] : char_t::TERM;

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

        char_t cur_symb = pattern.Size() > node->len ? pattern[node->len] : char_t::TERM;

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
        char_t pat_symb = lcp < pattern.Size() ? pattern[lcp] : char_t::TERM;
        char_t text_symb = dna[str_pos + lcp];
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
            char_t pat_symb = pattern[lcp];
            char_t dna_symb = dna[str_pos + lcp];
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

}  // namespace DNA_PT
