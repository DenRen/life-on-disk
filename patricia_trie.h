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

using char_t = char;
using alph_size_t = uint8_t;
using len_t = uint32_t;
using blk_pos_t = uint32_t;
using uint = unsigned;

using node_pos_t = uint16_t;

PACKED_STRUCT Node {
    enum class Type : uint8_t { Inner, External };

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
        return (uint8_t*)cend() - (uint8_t*)&m_node;
    }

    void InsertUniq(char_t symb, Node* child, Branch* insert_pos) noexcept {
        // Check on unique insertion
        assert((insert_pos - GetBranches()) > 0 ? (insert_pos - 1)->symb != symb : true);

        uint shift_size = (end() - insert_pos) * sizeof(Branch);
        memmove(insert_pos + 1, insert_pos, shift_size);

        insert_pos->symb = symb;
        insert_pos->node_pos = (uint8_t*)child - (uint8_t*)&m_node;

        const uint old_num_branch = m_node.size++;
        Branch* branches = GetBranches();
        for (uint i = 0; i < old_num_branch; ++i) {
            if (branches[i].node_pos >= insert_pos->node_pos) {
                branches[i].node_pos += sizeof(Branch);
            }
        }
    }

    void Dump() const;

private:
    const Branch* GetBranchs() const noexcept {
        return (const Branch*)((const uint8_t*)&m_node + sizeof(m_node));
    }
    Branch* GetBranches() noexcept {
        return (Branch*)((uint8_t*)&m_node + sizeof(m_node));
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
        m_size = (uint8_t*)(&m_root + 1) - (uint8_t*)this;

        m_root.len = 0;
        m_root.type = Node::Type::Inner;
        m_root.size = 0;
    }

    void Insert(const char* str, uint64_t pos) {
        // TODO
    }

    std::string DrawTrie() const;

private:
    void DrawTrieImpl(const Node& node, std::string& init_nodes,
                      std::string& conns) const;

public:  // Temporary
         // private:
    void InsertUniqExt(InnerNodeWrapper& node, char_t symb,
                       const ExternalNode& ext_node) noexcept {
        Branch* insert_pos = node.LowerBound(symb);
        assert(insert_pos == node.end());

        const uint insert_size = sizeof(Branch) + sizeof(ExternalNode);
        uint8_t* src = (uint8_t*)node.end();
        memmove(src + insert_size, src, m_size - (src - (uint8_t*)this));

        ExternalNode* new_ext_node = (ExternalNode*)(src + sizeof(Branch));
        *new_ext_node = ext_node;

        node.InsertUniq(symb, (Node*)new_ext_node, insert_pos);

        Node* insert_node_base = (Node*)&node.Base();
        Node* cur_base_node = &m_root;
        while (cur_base_node != insert_node_base) {
            if (cur_base_node->IsInnerNode()) {
                InnerNodeWrapper inn_node{*(InnerNode*)cur_base_node};

                node_pos_t node_pos_shifted =
                    (uint8_t*)new_ext_node - (uint8_t*)cur_base_node;
                for (auto& branch : inn_node) {
                    if (branch.node_pos >= node_pos_shifted) {
                        branch.node_pos += insert_size;
                    }
                }

                cur_base_node = (Node*)((uint8_t*)cur_base_node + inn_node.GetFullSize());
            } else {
                cur_base_node = (Node*)((uint8_t*)cur_base_node + sizeof(ExternalNode));
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