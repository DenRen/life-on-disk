#include <cstdio>
#include <iostream>

#include "src/patricia_trie.h"
#include "src/string_btree.h"

int main() try {
    std::cout << SBT::InnerNode::num_leaves << std::endl;
    std::cout << SBT::LeafNode::num_leaves << std::endl;
    std::cout << sizeof(SBT::InnerNode) << std::endl;
    std::cout << sizeof(SBT::LeafNode) << std::endl;

#if 1
    // const std::string_view data_path = "../../data/Homo_sapiens.GRCh38.dna.chromosome.MT.fa";
    const std::string_view data_path = "../../data/proteins.200MB";
    // const std::string_view data_path = "../../data/english.5MB";
#else
    const std::string_view data_path = "../../data/micro";
#endif

#define CACHE_SA
#define CACHE_SBT

#ifndef CACHE_SBT
    FileMapperRead mapper{data_path};
    auto src = mapper.GetData();

#ifndef CACHE_SA
    auto suff = BuildSortedSuff(src);
    std::cout << "SA builded\n";

    FileMapperWrite suff_mapper{"eng.sa", suff.size() * sizeof(str_pos_t)};
    std::copy(suff.cbegin(), suff.cend(), (str_pos_t*)suff_mapper.GetData().first);
#else
    FileMapperRead suff_mapper{"eng.sa"};
    std::vector<str_pos_t> suff(suff_mapper.GetData().size() / sizeof(str_pos_t));

    const auto* src_suff_begin = (const str_pos_t*)suff_mapper.GetData().data();
    const auto* src_suff_end = src_suff_begin + suff.size();
    std::copy(src_suff_begin, src_suff_end, suff.data());
#endif

    std::cout << "min: " << src.substr(suff[0], 20) << std::endl;
    std::cout << "max: " << src.substr(suff.back(), 20) << std::endl;

    auto sbt = SBT::StringBTree::Build("btree.bin", std::string{data_path}, suff);
    // sbt.Dump();
    std::cout << "SBT builded\n";
#else
    SBT::StringBTree sbt{"btree.bin", std::string{data_path}};
    std::cout << "SBT loaded\n";
#endif

    while (true) {
        std::string str;
        std::getline(std::cin, str);
        if (str == "end") {
            return 0;
        }

        std::string_view pattern{str};
        std::string_view res = sbt.Search(pattern);
        if (res.empty()) {
            std::cout << "not found" << std::endl;
        } else {
            std::cout << "str: `" << res.substr(0, 50) << "`" << std::endl;
            std::cout << "len: " << res.size() << std::endl;
        }
    }
} catch (std::exception& exc) {
    std::cerr << "Exception: " << exc.what() << std::endl;
}
