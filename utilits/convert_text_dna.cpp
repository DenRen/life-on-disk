#include <cstdio>
#include <string>

#include "../common/file_mapper.h"
#include "../dna/dna.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Please, enter path to file with text DNA data\n");
        return 0;
    }

    std::string_view text_dna_path{argv[1]};
    std::string comp_dna_path = std::string{text_dna_path} + ".comp";

    auto dna_file_holder = BuildCompressedDnaFromTextDna(text_dna_path, comp_dna_path);
    const auto* data_begin = dna_file_holder.cbegin();
    for (uint64_t dna_pos = 0; dna_pos < dna_file_holder.Size(); ++dna_pos) {
        std::cout << ReadDnaSymb(data_begin, dna_pos);
    }
    std::cout << std::endl;
}
