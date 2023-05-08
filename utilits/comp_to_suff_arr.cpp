#include <cstdio>
#include <string>

#include "../src/file_mapper.h"
#include "../dna/dna.h"

#include <algorithm>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Please, enter path to file with compressed DNA data\n");
        return 0;
    }

    std::string_view comp_dna_path{argv[1]};

    std::string_view comp_ext{".comp"};
    const auto ext_pos = comp_dna_path.rfind(comp_ext);
    if (ext_pos == std::string_view::npos || (comp_dna_path.size() - ext_pos) != comp_ext.size()) {
        printf("Incorrect path: \'%s\'\n", comp_dna_path.data());
    }

    std::string suff_arr_path = std::string{comp_dna_path} + ".sa";

    std::cout << "SA building started" << std::endl;
    auto dna_sa_file_holder = BuildSuffArrayFromCompressedDna(comp_dna_path, suff_arr_path);
    const uint32_t* sa = (const uint32_t*)dna_sa_file_holder.cbegin();
    const uint32_t sa_size = dna_sa_file_holder.Size();
    std::cout << "SA building finished" << std::endl;

    // Validate SA
    std::cout << "Starting validation SA" << std::endl;
    ObjectFileHolder dna_file_holder{comp_dna_path};
    DnaDataAccessor dna{dna_file_holder.cbegin(), dna_file_holder.Size()};

#if 0
    std::string dna_text;
    dna_text.reserve(dna.Size());
    for (int i = 0; i + 1 < dna.Size(); ++i) {
        dna_text += std::to_string((u8)dna[i]);
    }
    dna_text += '\0';
    // std::cout << dna_text << std::endl;

    std::vector<uint32_t> sa_ref(dna.Size());
    for (uint32_t i = 0; i < sa_ref.size(); ++i) {
        sa_ref[i] = i;
    }

    std::sort(sa_ref.begin(), sa_ref.end(), [&dna_text](uint32_t lhs, uint32_t rhs) {
        std::string_view sv_l{dna_text.data() + lhs, dna_text.size() - lhs};
        std::string_view sv_r{dna_text.data() + rhs, dna_text.size() - rhs};
        return sv_l < sv_r;
    });

    for (uint64_t i_sa = 0; i_sa < sa_ref.size(); ++i_sa) {
        if (sa_ref[i_sa] != sa[i_sa]) {
            throw std::runtime_error{"Incorrect SA here!"};
        }
    }
#else
    for (uint32_t i_sa = 0; i_sa + 1 < sa_size; ++i_sa) {
        uint32_t lhs_begin = sa[i_sa];
        uint32_t rhs_begin = sa[i_sa + 1];

        uint32_t len = sa_size - std::max(lhs_begin, rhs_begin);
        for (uint32_t i = 0; i < len; ++i) {
            auto lhs_symb = dna[lhs_begin + i];
            auto rhs_symb = dna[rhs_begin + i];
            if (lhs_symb != rhs_symb) {
                if (lhs_symb < rhs_symb) {
                    break;
                }

                std::cout << "len: " << len << std::endl;
                std::cout << "i_sa: " << i_sa << std::endl;
                std::cout << "i: " << i << std::endl;
                std::cout << "lhs_begin: " << lhs_begin << std::endl;
                std::cout << "rhs_begin: " << rhs_begin << std::endl;
                std::cout << "lhs_symb: " << lhs_symb << std::endl;
                std::cout << "rhs_symb: " << rhs_symb << std::endl;

                throw std::runtime_error{"Incorrect SA!"};
            }
        }
    }
#endif

    std::cout << "SA is valid" << std::endl;
}
