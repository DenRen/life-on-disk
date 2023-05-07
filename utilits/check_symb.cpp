#include <cstdio>
#include <string>
#include <array>

#include "../src/file_mapper.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        return -1;
    }

    using cur_char_t = uint8_t;
    std::array<uint64_t, 256 << sizeof(cur_char_t)> num_occ_symb{};

    FileMapperRead reader{argv[1]};
    std::string_view sv = reader.GetData();

    const cur_char_t* begin = (const cur_char_t*)sv.cbegin();
    const cur_char_t* end = (const cur_char_t*)sv.cend();

    for (const cur_char_t* it = begin; it < end; ++it) {
        ++num_occ_symb[*it];
    }

    for (uint i = 0; i < num_occ_symb.size(); ++i) {
        if (num_occ_symb[i]) {
            printf("%3u: \'%c\' -> %lu\n", i, (cur_char_t)i, num_occ_symb[i]);
        }
    }
}
