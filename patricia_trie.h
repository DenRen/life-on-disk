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
using blk_pos_t = uint32_t;
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
    ExternalNode(len_t len, blk_pos_t pos_ext) noexcept
        : Node{len, Node::Type::External}
        , pos_ext{pos_ext} {}

    blk_pos_t pos_ext;
};

class InnerNodeWrapper {
public:
    InnerNodeWrapper(Node& node) noexcept
        : m_node{static_cast<InnerNode&>(node)} {
        assert(node.IsInnerNode());
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

    void Insert(const char* str, uint64_t pos) {
        // TODO
    }

    std::string DrawTrie() const;

private:
    void DrawTrieImpl(const Node& node, std::string& init_nodes, std::string& conns) const;

public:  // Temporary
         // private:
    void InsertUniqExt(InnerNodeWrapper& node, char_t symb, const ExternalNode& ext_node) noexcept {
        Branch* insert_pos = node.LowerBound(symb);

        const uint insert_size = sizeof(Branch) + sizeof(ExternalNode);
        u8* src = (u8*)node.end();
        memmove(src + insert_size, src, m_size - (src - (u8*)this));
        m_size += insert_size;

        ExternalNode* new_ext_node = (ExternalNode*)(src + sizeof(Branch));
        *new_ext_node = ext_node;

        node.InsertUniq(insert_pos, symb, (Node*)new_ext_node, insert_size);

        Node* insert_node_base = (Node*)&node.Base();
        Node* cur_base_node = &m_root;
        while (cur_base_node != insert_node_base) {
            if (cur_base_node->IsInnerNode()) {
                InnerNodeWrapper inn_node{*(InnerNode*)cur_base_node};

                node_pos_t node_pos_shifted = (u8*)src - (u8*)cur_base_node;
                for (auto& branch : inn_node) {
                    if (branch.node_pos >= node_pos_shifted) {
                        branch.node_pos += insert_size;
                    }
                }

                cur_base_node = (Node*)((u8*)cur_base_node + inn_node.GetFullSize());
            } else {
                cur_base_node = (Node*)((u8*)cur_base_node + sizeof(ExternalNode));
            }
        }
    }

    InnerNode& InsertUniqInnExt(InnerNodeWrapper& node, char_t symb, len_t inn_node_len,
                                char_t ext_symb, const ExternalNode& ext_node) noexcept {
        Branch* const insert_pos = node.LowerBound(symb);
        assert(insert_pos != node.end() && insert_pos->symb == symb);

        const uint insert_size = 2 * sizeof(Branch) + sizeof(InnerNode) + sizeof(ExternalNode);
        u8* src = (u8*)node.end();
        memmove(src + insert_size, src, m_size - (src - (u8*)this));
        m_size += insert_size;

        InnerNode& inn_node = *(InnerNode*)src;
        inn_node = {inn_node_len, 2};

        Branch* new_branches = (Branch*)(&inn_node + 1);

        ExternalNode* new_ext_node = (ExternalNode*)(new_branches + 2);
        *new_ext_node = ext_node;

        u8* node_base = (u8*)&node.Base();
        u8* shifted_child = node_base + insert_pos->node_pos + insert_size;
        node_pos_t pos_shifted_child = (node_pos_t)(shifted_child - (u8*)&inn_node);
        node_pos_t pos_new_child = (u8*)new_ext_node - (u8*)&inn_node;
        if (ext_symb > symb) {
            new_branches[0] = Branch{.symb = symb, .node_pos = pos_shifted_child};
            new_branches[1] = Branch{.symb = ext_symb, .node_pos = pos_new_child};
        } else {
            new_branches[1] = Branch{.symb = symb, .node_pos = pos_shifted_child};
            new_branches[0] = Branch{.symb = ext_symb, .node_pos = pos_new_child};
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
        Node* cur_base_node = &m_root;
        while (cur_base_node != insert_node_base) {
            if (cur_base_node->IsInnerNode()) {
                InnerNodeWrapper inn_node{*(InnerNode*)cur_base_node};

                node_pos_t node_pos_shifted = (u8*)src - (u8*)cur_base_node;
                for (auto& branch : inn_node) {
                    if (branch.node_pos >= node_pos_shifted) {
                        branch.node_pos += insert_size;
                    }
                }

                cur_base_node = (Node*)((u8*)cur_base_node + inn_node.GetFullSize());
            } else {
                cur_base_node = (Node*)((u8*)cur_base_node + sizeof(ExternalNode));
            }
        }

        return inn_node;
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