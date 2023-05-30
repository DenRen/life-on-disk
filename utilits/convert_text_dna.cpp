#include <cstdio>
#include <string>

#include "../common/file_manip.h"
#include "../dna/dna.h"

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Please, enter path to file with text DNA data and blocking param d\n");
        return 0;
    }

    std::string_view text_dna_path{argv[1]};
    std::string comp_dna_path = std::string{text_dna_path} + ".comp";

    uint d = 0;
    if (sscanf(argv[2], "%u", &d) != 1) {
        throw std::invalid_argument{"Incorrect d"};
    }

    printf("d: %u\n", d);
    auto dna_file_holder = BuildCompressedDnaFromTextDna(text_dna_path, comp_dna_path, d);
    // const auto* data_begin = dna_file_holder.cbegin();
    // for (uint64_t dna_pos = 0; dna_pos < dna_file_holder.Size(); ++dna_pos) {
    //     std::cout << ReadDnaSymb(data_begin, dna_pos);
    // }
    // std::cout << std::endl;
}
