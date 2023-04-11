#include <cstdio>

#include "file_mapper.h"

int main() {
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
