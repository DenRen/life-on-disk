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
#include <tuple>

#include "patricia_trie.h"

using std::cout;
using std::endl;

using namespace PT;

std::tuple<char*, size_t, std::vector<ext_pos_t>> CreateFilledExternalStrings() {
    std::vector<std::string> strs = {"abaaba",  "abaabb",  "abac",    "bcbcaba",
                                     "bcbcabb", "bcbcbba", "bcbcbbba"};

    uint ext_buf_len = strs.size();
    for (const auto& str : strs) ext_buf_len += str.size();

    char* const ext_buf_strs = new char[ext_buf_len];
    std::vector<ext_pos_t> ext_poss;
    char* ptr = ext_buf_strs;
    for (const auto& str : strs) {
        ext_poss.push_back(ptr - ext_buf_strs);
        ptr = std::copy(begin(str), end(str), ptr);
        *ptr++ = '\0';
    }

    // for (uint i = 0; i < ext_buf_len; ++i) {
    //     putchar(ext_buf_strs[i] != '\0' ? ext_buf_strs[i] : ' ');
    // }
    // putchar('\n');

    return {ext_buf_strs, ext_buf_len, ext_poss};
}

int main() {
    const auto [ext_buf_strs, ext_buf_len, ext_poss] = CreateFilledExternalStrings();

    PatriciaTrie pt;
    for (int i = 0; i < ext_poss.size(); ++i) {
        const auto pos = ext_poss[i];
        const char_t* str = ext_buf_strs + pos;
        pt.Insert(ext_buf_strs, pos, strlen(str) + 1);

        cout << i << ") " << str << ": " << pos << endl;
    }

    std::ofstream dot_file{"dump.dot"};
    dot_file << pt.DrawTrie(ext_buf_strs);

    delete[] ext_buf_strs;
}
