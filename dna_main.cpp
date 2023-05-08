#include <cstdio>
#include <iostream>

#include "dna/dna.h"
#include "dna/patricia_trie.h"
#include "dna/string_btree.h"

int main() try {
    // std::cout << SBT::InnerNode::num_leaves << std::endl;
    // std::cout << SBT::LeafNode::num_leaves << std::endl;
    // std::cout << sizeof(SBT::InnerNode) << std::endl;
    // std::cout << sizeof(SBT::LeafNode) << std::endl;

    std::string btree_name = "dna_btree.bin";
    const std::string data_dir = "../../data/T_SA/";
    const char* data_size_arr[] = {"1Kb", "1Mb", "200MB"};

    const str_len_t max_print_len = 25;

    const char* data_size = data_size_arr[2];
    std::string dna_data_path = data_dir + "dna." + data_size + ".comp";
    // std::string dna_data_path = data_dir + "micro.comp";
    std::string dna_data_sa_path = dna_data_path + ".sa";

    std::cout << "DNA: " << dna_data_path << std::endl;
    std::cout << "SA:  " << dna_data_sa_path << "\n\n";

// #define CACHE_SBT

#ifndef CACHE_SBT
    std::cout << "SBT building started\n";
    auto sbt = DNA_SBT::StringBTree::Build(btree_name, dna_data_path, dna_data_sa_path);
    // sbt.Dump();
    std::cout << "SBT building finished\n";
#else
    DNA_SBT::StringBTree sbt{btree_name, dna_data_path};
    std::cout << "SBT loaded\n";
#endif

    ObjectFileHolder dna_file_holder{dna_data_path};
    DnaDataAccessor dna_data{dna_file_holder};

    while (true) {
        std::string str;
        std::getline(std::cin, str);
        if (str == "end") {
            return 0;
        }

        DnaBuffer dna_buf{str};
        DnaDataAccessor pattern = dna_buf.GetAccessor();
        
        auto [pos, is_finded] = sbt.Search(pattern);
        if (is_finded) {
            str_len_t answer_len = dna_data.Size() - pos;
            str_len_t len = std::min(answer_len, max_print_len);

            std::cout << "res: `";
            for (str_pos_t i = 0; i < len; ++i) {
                std::cout << dna_data[pos + i];
            }

            if (answer_len > len) {
                std::cout << "...";
            }

            std::cout << "`" << std::endl;

            std::cout << "len: " << answer_len << std::endl;
        } else {
            std::cout << "not found" << std::endl;
        }
    }
} catch (std::exception& exc) {
    std::cerr << "Exception: " << exc.what() << std::endl;
}
