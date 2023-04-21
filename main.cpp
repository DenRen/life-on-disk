#include <cstdio>
#include <iostream>

#include "src/patricia_trie.h"
#include "src/string_btree.h"

void find_suff() {
    const std::string_view data_path = "../../data/Homo_sapiens.GRCh38.dna.chromosome.MT.fa";
    printf("data path: %s\n", data_path.data());

    FileMapperRead mapper{data_path};

    const char* data = mapper.GetData().data();
    uint64_t data_len = mapper.GetData().size();
    printf("Data len: %ld\n", data_len);

    auto suff_pos = BuildSortedSuff(mapper.GetData());

    long sh = 700;
    for (int i = 0; i < 10; ++i) {
        putchar('\'');
        for (int j = 0; j < 30; ++j) {
            putchar(data[suff_pos[sh + i] + j]);
        }
        putchar('\'');
        putchar('\n');
    }
    printf("\n");
}

int main() try {
    // std::cout << SBT::InnerNode::num_leaves << std::endl;
    // std::cout << SBT::LeafNode::num_leaves << std::endl;
    // std::cout << sizeof(SBT::InnerNode) << std::endl;
    // std::cout << sizeof(SBT::LeafNode) << std::endl;

#if 0
    const std::string_view data_path = "../../data/Homo_sapiens.GRCh38.dna.chromosome.MT.fa";
#else
    const std::string_view data_path = "../../data/micro";
#endif
    FileMapperRead mapper{data_path};
    auto src = mapper.GetData();
    auto suff = BuildSortedSuff(src);

    // std::string_view src = "abbbbabcbcbbdbsjhgjbkbkk\0";
    // auto suff = BuildSortedSuff(src);
    // std::vector<std::pair<std::string_view, PT::node_pos_t>> data;
    // for (auto pos : suff) {
    //     std::string_view sv{src.data() + pos, src.size() + 1 - pos};
    //     data.emplace_back(sv, pos);
    // }

    // u8* page = new u8[SBT::g_page_size]{};

    // std::cout << SBT::Node<true>::num_leaves << std::endl;
    // PT::BuildAmdEmplacePT(data, page, SBT::g_page_size);

    // delete[] page;

    SBT::StringBTree::Build("btree.bin", std::string{data_path}, suff);
} catch (std::exception& exc) {
    std::cerr << "Exception: " << exc.what() << std::endl;
}
