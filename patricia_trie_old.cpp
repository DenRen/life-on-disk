#include "patricia_trie_old.h"

#include <stdexcept>

namespace PT {

void InnerNodeWrapper::Dump() const {
    printf("Number branching chars: %hhu", m_node.size);
    for (auto it = cbegin(); it != cend(); ++it) {
        printf(", [%c, %hu]", it->symb, it->node_pos);
    }
    printf("\n");
}

std::string PatriciaTrie::DrawTrie(const char_t* ext_begin) const {
    std::string dot =
        "digraph {\n"
        "\tgraph [rankdir = \"TB\"];\n"
        "\tnode [shape = record];";

    std::string init_nodes, conns;
    DrawTrieImpl(ext_begin, GetRoot(), init_nodes, conns);

    dot += "\n";
    dot += init_nodes;
    dot += "\n\n";
    dot += conns;

    dot += "\n}";
    return dot;
}

void PatriciaTrie::DrawTrieImpl(const char_t* ext_begin, const Node& node, std::string& init_nodes,
                                std::string& conns) const {
    const uint8_t* root = (const uint8_t*)&GetRoot();

    auto get_node_name = [&](const uint8_t* node) {
        return "node" + std::to_string(node - root);
    };

    std::string node_name = get_node_name((const uint8_t*)&node);
    if (node.IsInnerNode()) {
        InnerNodeWrapper inn{const_cast<Node&>(node)};

        init_nodes += "\n\t" + node_name + "[label=\"";

        bool is_first = true;
        for (const auto& branch : inn) {
            if (!is_first) {
                init_nodes += " | ";
            }
            is_first = false;

            std::string symb{branch.symb};
            init_nodes += "<" + symb + "> " + symb;

            const uint8_t* child = (const uint8_t*)&node + branch.node_pos;
            std::string child_name = get_node_name(child);
            conns += "\t\"" + node_name + "\":<" + symb + ">->\"";
            conns += child_name + '\"';
            conns += "[label=\"" + std::to_string(((Node*)child)->len) + "\"]";
        }
        init_nodes += "\"];";

        for (const auto& branch : inn) {
            const Node& child = *(const Node*)((const uint8_t*)&node + branch.node_pos);
            DrawTrieImpl(ext_begin, child, init_nodes, conns);
        }
    } else if (node.IsLeafNode()) {
        const ExternalNode& ext_node = (const ExternalNode&)node;

        init_nodes += "\n\t" + node_name + "[label=\"";
        const char_t* str = ext_begin + ext_node.pos_ext;
        init_nodes += std::string(str) + "\" shape=egg ];";
    } else {
        throw std::runtime_error("Invalid node type");
    }
}
}  // namespace PT
