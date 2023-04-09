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

PACKED_STRUCT BranchTest {
    InnerNode node = {{.len = 0, .type = Node::Type::Inner}, .size = 5};
    std::array<Branch, 10> branchs = {Branch{'1', 0},
                                      Branch{'3', 0},
                                      Branch{'5', 0},
                                      Branch{'7', 0},
                                      Branch{'8', 0},
                                      {},
                                      {},
                                      {},
                                      {},
                                      {}};
};

int main() {
    // const auto [ext_buf_strs, ext_buf_len] = CreateFilledExternalStrings();
    // delete[] ext_buf_strs;

    // BranchTest branch_test;
    // InnerNodeWrapper inn{ branch_test.node };

    // inn.Dump();
    // inn.InsertUniq({'2', 0}, inn.LowerBound('2'));
    // inn.Dump();
    // inn.InsertUniq({'4', 0}, inn.LowerBound('4'));
    // inn.Dump();
    // inn.InsertUniq({'6', 0}, inn.LowerBound('6'));

    // inn.Dump();

    PatriciaTrie pt;
    InnerNodeWrapper root{pt.GetRoot()};

    pt.InsertUniqExt(root, 'a', ExternalNode{30, 170});
    pt.InsertUniqExt(root, 'c', ExternalNode{30, 190});
    pt.InsertUniqExt(root, 'd', ExternalNode{30, 100});
    pt.InsertUniqExt(root, 'b', ExternalNode{30, 102});
    pt.InsertUniqExt(root, 'A', ExternalNode{30, 170});
    pt.InsertUniqExt(root, 'C', ExternalNode{30, 190});
    pt.InsertUniqExt(root, 'D', ExternalNode{30, 100});
    pt.InsertUniqExt(root, 'V', ExternalNode{30, 102});
    pt.InsertUniqExt(root, '1', ExternalNode{30, 107});
    pt.InsertUniqExt(root, '_', ExternalNode{30, 100});

    std::ofstream dot_file{"dump.dot"};
    dot_file << pt.DrawTrie();
}
