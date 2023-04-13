#include <cstdio>
#include <iostream>

#include "file_mapper.h"
#include "patricia_trie.h"

void find_suff() {
    const std::string_view data_path = "../../data/Homo_sapiens.GRCh38.dna.chromosome.MT.fa";
    printf("data path: %s\n", data_path.data());

    FileMapper mapper{data_path};

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

int main() {
    // std::cout << SBT::InnerNode::PT_num_leaf << std::endl;
    // std::cout << SBT::LeafNode::PT_num_leaf << std::endl;
    // std::cout << sizeof(SBT::InnerNode) << std::endl;
    // std::cout << sizeof(SBT::LeafNode) << std::endl;

    const std::string_view data_path = "../../data/Homo_sapiens.GRCh38.dna.chromosome.MT.fa";
    FileMapper mapper{data_path};
    auto src = mapper.GetData();
    auto suff = BuildSortedSuff(src);

    // std::string_view src = "ababababaa\0";
    // auto suff = BuildSortedSuff(src);
    std::vector<std::pair<std::string_view, PT::node_pos_t>> data;
    for (auto pos : suff) {
        std::string_view sv{src.data() + pos, src.size() + 1 - pos};
        data.emplace_back(sv, pos);
    }

    PT::BuildAmdEmplacePT(data);
}
