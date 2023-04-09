#pragma once

#include <string.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <string>

#define PACKED_STRUCT struct __attribute__((__packed__))

namespace PT {

using u8 = uint8_t;

using char_t = char;
using alph_size_t = u8;
using len_t = uint32_t;
using ext_pos_t = uint64_t;
using uint = unsigned;

using node_pos_t = uint16_t;

PACKED_STRUCT Node {
    enum class Type : u8 { Inner, External };

    len_t len;
    Type type;

    bool IsInnerNode() const noexcept {
        return type == Type::Inner;
    }
    bool IsLeafNode() const noexcept {
        return type == Type::External;
    }
};

PACKED_STRUCT InnerNode : public Node {
    InnerNode(len_t len = 0, alph_size_t size = 0) noexcept
        : Node{len, Type::Inner}
        , size(size) {}

    alph_size_t size;
};

PACKED_STRUCT Branch {
    char_t symb;
    node_pos_t node_pos;  // Relative of begin Node
};

PACKED_STRUCT ExternalNode : public Node {  // Leaf node of PT
    ExternalNode(len_t len, ext_pos_t pos_ext) noexcept
        : Node{len, Node::Type::External}
        , pos_ext{pos_ext} {}

    ext_pos_t pos_ext;
};

class InnerNodeWrapper {
public:
    InnerNodeWrapper(InnerNode& node) noexcept
        : m_node{node} {}

    InnerNodeWrapper(Node& node) noexcept
        : m_node{static_cast<InnerNode&>(node)} {
        assert(m_node.IsInnerNode());
    }

    InnerNodeWrapper(Node* node) noexcept
        : m_node{static_cast<InnerNode&>(*node)} {
        assert(m_node.IsInnerNode());
    }

    Branch* UppperBound(char symb) noexcept {
        return std::upper_bound(begin(), end(), symb, [](char symb, const Branch& rhs) {
            return symb < rhs.symb;
        });
    }
    Branch* LowerBound(char symb) noexcept {
        return std::lower_bound(begin(), end(), symb, [](const Branch& lhs, char symb) {
            return lhs.symb < symb;
        });
    }

    auto size() const noexcept {
        return m_node.size;
    }
    const Branch* cbegin() const noexcept {
        return GetBranchs();
    }
    const Branch* cend() const noexcept {
        return cbegin() + size();
    }
    Branch* begin() noexcept {
        return GetBranches();
    }
    Branch* end() noexcept {
        return begin() + size();
    }

    const InnerNode& Base() const noexcept {
        return m_node;
    }
    InnerNode& Base() noexcept {
        return m_node;
    }

    uint GetFullSize() const noexcept {
        return (u8*)cend() - (u8*)&m_node;
    }

    void InsertUniq(Branch* insert_pos, char_t symb, Node* child,
                    node_pos_t add_shift_size) noexcept {
        // Check on unique insertion
        assert((insert_pos - GetBranches()) > 0 ? (insert_pos - 1)->symb != symb : true);

        Branch* const cur_end = end();
        uint shift_size = (cur_end - insert_pos) * sizeof(Branch);
        memmove(insert_pos + 1, insert_pos, shift_size);

        insert_pos->symb = symb;
        insert_pos->node_pos = (u8*)child - (u8*)&m_node;

        const uint bound_pos = (u8*)cur_end - (u8*)&m_node;
        ++m_node.size;

        Branch* const last_end = end();
        for (Branch* branch = begin(); branch < last_end; ++branch) {
            if (branch == insert_pos) {
                continue;
            }

            if (branch->node_pos >= bound_pos) {
                branch->node_pos += add_shift_size;
            }
        }
    }

    Node& GetChild(const Branch* branch) noexcept {
        return *(Node*)((u8*)&m_node + branch->node_pos);
    }

    const Node& GetChild(const Branch* branch) const noexcept {
        return *(const Node*)((const u8*)&m_node + branch->node_pos);
    }

    Node& GetChild(uint index) noexcept {
        return GetChild(GetBranches() + index);
    }

    void Dump() const;

private:
    const Branch* GetBranchs() const noexcept {
        return (const Branch*)((const u8*)&m_node + sizeof(m_node));
    }
    Branch* GetBranches() noexcept {
        return (Branch*)((u8*)&m_node + sizeof(m_node));
    }

private:
    InnerNode& m_node;
};

constexpr static uint PatriciaTrieNodeSize(uint max_num_leafs) noexcept {
    uint max_num_inner_node = 2 * (max_num_leafs - 1);
    uint size_inner_node = sizeof(InnerNode) + 2 * sizeof(Branch);
    uint size_inner_nodes = max_num_inner_node * size_inner_node;
    uint size_external_nodes = max_num_leafs * sizeof(ExternalNode);

    return size_inner_nodes + size_external_nodes;
}

constexpr static uint FindMaxNumLeafs(uint size) noexcept {
    uint num_leafs = 1;
    while (PatriciaTrieNodeSize(num_leafs) < size) ++num_leafs;

    return num_leafs - 1;
}

class PatriciaTrie {
public:
    constexpr static uint block_size = 4096;
    constexpr static uint max_num_leafs = FindMaxNumLeafs(block_size);

    PatriciaTrie() noexcept {
        m_size = (u8*)(&m_root + 1) - (u8*)this;

        m_root.len = 0;
        m_root.type = Node::Type::Inner;
        m_root.size = 0;
    }

    void Insert(const char_t* ext_begin, ext_pos_t str_pos, len_t len) {
        assert(ext_begin != nullptr);

        const char_t* str = ext_begin + str_pos;

        InnerNodeWrapper root{m_root};
        Branch* insert_pos = root.LowerBound(str[0]);
        if (insert_pos == root.end()) {
            ExternalNode ext_node{len, str_pos};
            InsertUniqExt(root, insert_pos, str[0], ext_node);
        } else {
            Node& child = root.GetChild(insert_pos);
            InsertInNonRoot(child, m_root, insert_pos, ext_begin, len, str_pos, 1);
        }
    }

    std::string DrawTrie(const char_t* ext_begin) const;

private:
    void DrawTrieImpl(const char_t* ext_begin, const Node& node, std::string& init_nodes,
                      std::string& conns) const;

    static ExternalNode* FindLeftmostNode(Node& node) noexcept {
        Node* leftmost_child = &node;
        while (!leftmost_child->IsLeafNode()) {
            leftmost_child = &InnerNodeWrapper{leftmost_child}.GetChild(0u);
        }
        return (ExternalNode*)leftmost_child;
    }

    void InsertInNonRoot(Node& child, InnerNode& parent, Branch* insert_pos,
                         const char_t* ext_begin, len_t len, ext_pos_t str_pos,
                         len_t lcp) noexcept {
        // Get child sstring for compare min_len characters
        ExternalNode* leftmost_child = FindLeftmostNode(child);
        const char_t* leftmost_node_str = ext_begin + leftmost_child->pos_ext;

        const char_t* str = ext_begin + str_pos;
        len_t min_len = std::min(len, child.len);
        len_t i = lcp;
        for (; i < min_len; ++i) {
            if (str[i] != leftmost_node_str[i]) {
                break;
            }
        }

        if (i != min_len) {
            // Insert new inner node between parent and child
            char_t symb_child = leftmost_node_str[i];
            char_t symb_new_ext = str[i];
            ExternalNode ext_node{len, str_pos};
            InnerNodeWrapper wrapper{parent};
            InsertUniqInnExt(wrapper, insert_pos, i, symb_new_ext, symb_child, ext_node);
        } else {
            InnerNodeWrapper node{child};
            Branch* insert_pos = node.LowerBound(str[i]);
            if (insert_pos == node.end()) {
                ExternalNode ext_node{len, str_pos};
                InsertUniqExt(node, insert_pos, str[i], ext_node);
            } else {
                Node& new_child = node.GetChild(insert_pos);
                InnerNode& new_parent = node.Base();
                InsertInNonRoot(new_child, new_parent, insert_pos, ext_begin, len, str_pos, i);
            }
        }
    }

public:  // Temporary
         // private:
    void InsertUniqExt(InnerNodeWrapper node, Branch* const insert_pos /* LowerBound */,
                       char_t symb, const ExternalNode& ext_node) noexcept {
        const uint insert_size = sizeof(Branch) + sizeof(ExternalNode);
        u8* src = (u8*)node.end();
        memmove(src + insert_size, src, m_size - (src - (u8*)this));
        m_size += insert_size;
        assert(m_size < block_size);

        ExternalNode* new_ext_node = (ExternalNode*)(src + sizeof(Branch));
        *new_ext_node = ext_node;

        node.InsertUniq(insert_pos, symb, (Node*)new_ext_node, insert_size);

        Node* insert_node_base = (Node*)&node.Base();
        ShiftNodes(insert_node_base, src, insert_size);
    }

    InnerNode& InsertUniqInnExt(InnerNodeWrapper& node, Branch* const insert_pos /* LowerBound */,
                                len_t inn_node_len, char_t symb_new_child, char_t symb_old_child,
                                const ExternalNode& ext_node) noexcept {
        char_t symb = insert_pos->symb;
        assert(insert_pos != node.end() && insert_pos->symb == symb);

        const uint insert_size = 2 * sizeof(Branch) + sizeof(InnerNode) + sizeof(ExternalNode);
        u8* src = (u8*)node.end();
        memmove(src + insert_size, src, m_size - (src - (u8*)this));
        m_size += insert_size;
        assert(m_size < block_size);

        InnerNode& inn_node = *(InnerNode*)src;
        inn_node = {inn_node_len, 2};

        Branch* new_branches = (Branch*)(&inn_node + 1);

        ExternalNode* new_ext_node = (ExternalNode*)(new_branches + 2);
        *new_ext_node = ext_node;

        u8* node_base = (u8*)&node.Base();
        u8* shifted_child = node_base + insert_pos->node_pos + insert_size;
        node_pos_t pos_shifted_child = (node_pos_t)(shifted_child - (u8*)&inn_node);
        node_pos_t pos_new_child = (u8*)new_ext_node - (u8*)&inn_node;
        if (symb_new_child < symb_old_child) {
            new_branches[0] = Branch{.symb = symb_new_child, .node_pos = pos_new_child};
            new_branches[1] = Branch{.symb = symb_old_child, .node_pos = pos_shifted_child};
        } else {
            new_branches[1] = Branch{.symb = symb_new_child, .node_pos = pos_new_child};
            new_branches[0] = Branch{.symb = symb_old_child, .node_pos = pos_shifted_child};
        }

        {
            node_pos_t node_pos_shifted = (src - (u8*)&node.Base());
            for (auto& branch : node) {
                if (&branch == insert_pos) {
                    insert_pos->node_pos = (u8*)&inn_node - (u8*)&node.Base();
                } else {
                    if (branch.node_pos >= node_pos_shifted) {
                        branch.node_pos += insert_size;
                    }
                }
            }
        }

        Node* insert_node_base = (Node*)&node.Base();
        ShiftNodes(insert_node_base, src, insert_size);

        return inn_node;
    }

    void ShiftNodes(Node* end, u8* src_changes, node_pos_t shift_size) noexcept {
        Node* insert_node_base = end;
        Node* cur_base_node = &m_root;
        while (cur_base_node != insert_node_base) {
            if (cur_base_node->IsInnerNode()) {
                InnerNodeWrapper inn_node{*(InnerNode*)cur_base_node};

                node_pos_t node_pos_shifted = src_changes - (u8*)cur_base_node;
                for (auto& branch : inn_node) {
                    if (branch.node_pos >= node_pos_shifted) {
                        branch.node_pos += shift_size;
                    }
                }

                cur_base_node = (Node*)((u8*)cur_base_node + inn_node.GetFullSize());
            } else {
                cur_base_node = (Node*)((u8*)cur_base_node + sizeof(ExternalNode));
            }
        }
    }

    InnerNode& GetRoot() {
        return m_root;
    }
    const InnerNode& GetRoot() const {
        return m_root;
    }
    Branch* GetBranches(InnerNode& inner_node) {
        return (Branch*)(&inner_node + sizeof(inner_node));
    };

private:
    uint16_t m_size;
    InnerNode m_root;
} __attribute__((aligned(block_size)));

static_assert(sizeof(PatriciaTrie) == PatriciaTrie::block_size);

}  // namespace PT