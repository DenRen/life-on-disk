#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <string.h>
#include <cassert>
#include <fstream>

#include "patricia_trie.h"

std::pair<char*, size_t> CreateFilledExternalStrings() {
    std::vector<std::string> strs = {"abaaaba", "abaaabb", "abac",    "bcbcaba",
                                     "bcbcabb", "bcbcbba", "bcbcbbba"};

    uint ext_buf_len = strs.size();
    for (const auto& str : strs) ext_buf_len += str.size();

    char* const ext_buf_strs = new char[ext_buf_len];

    char* ptr = ext_buf_strs;
    for (const auto& str : strs) {
        ptr = std::copy(begin(str), end(str), ptr);
        *ptr++ = '\0';
    }

    // for (uint i = 0; i < ext_buf_len; ++i) {
    //     putchar(ext_buf_strs[i] != '\0' ? ext_buf_strs[i] : ' ');
    // }
    // putchar('\n');

    return {ext_buf_strs, ext_buf_len};
}

using std::cout;
using std::endl;

using namespace PT;

int main() {
    // const auto [ext_buf_strs, ext_buf_len] = CreateFilledExternalStrings();
    // delete[] ext_buf_strs;

    PatriciaTrie pt;
    InnerNodeWrapper root{pt.GetRoot()};

    pt.InsertUniqExt(root, 'a', ExternalNode{30, 170});
    pt.InsertUniqExt(root, 'c', ExternalNode{30, 190});
    pt.InsertUniqExt(root, 'H', ExternalNode{30, 190});
    pt.InsertUniqExt(root, 'e', ExternalNode{30, 100});
    pt.InsertUniqExt(root, 'l', ExternalNode{30, 102});
    pt.InsertUniqExt(root, 'o', ExternalNode{30, 107});
    pt.InsertUniqExt(root, 'w', ExternalNode{30, 100});
    pt.InsertUniqExt(root, 'r', ExternalNode{30, 100});

    {
        InnerNode& inn_node = pt.InsertUniqInnExt(root, 'l', 30, '_', ExternalNode{30, 185});
        InnerNodeWrapper node{inn_node};
        pt.InsertUniqExt(node, 'b', ExternalNode{30, 199});
        pt.InsertUniqInnExt(node, 'b', 30, 'U', ExternalNode{30, 24});
    }

    pt.InsertUniqExt(root, 'd', ExternalNode{30, 100});
    pt.InsertUniqExt(root, 'b', ExternalNode{30, 102});
    pt.InsertUniqExt(root, 'A', ExternalNode{30, 170});

    {
        InnerNode& inn_node = pt.InsertUniqInnExt(root, 'e', 30, 'P', ExternalNode{30, 15});
        InnerNodeWrapper node{inn_node};
        pt.InsertUniqExt(node, 'b', ExternalNode{30, 1909});
        pt.InsertUniqInnExt(node, 'b', 30, 'U', ExternalNode{30, 240});
    }

    std::ofstream dot_file{"dump.dot"};
    dot_file << pt.DrawTrie();
}
