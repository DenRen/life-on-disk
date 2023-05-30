#include <cstdio>
#include <iostream>

#include "dna/dna.h"
#include "dna/patricia_trie.h"
#include "dna/string_btree.h"
#include "dna/wavelet_tree_on_disk.hpp"

/*
    0) Text
    1) Compressed Text, SA
    2) SBT или {SBT, WT}
*/

void CheckBlockSize(u8 block_size) {
    if (block_size == 0) {
        throw std::invalid_argument{"Block size \"d\" must be greater 0"};
    }
}

class NameGenerator {
public:
    NameGenerator(std::string text_path, u8 block_size /* d */)
        : m_compressed_text{text_path + ".comp"}
        , m_suffix_array{m_compressed_text + ".sa"}
        , m_string_btree{m_compressed_text + ".sbt"}
        , m_wavelet_tree{m_compressed_text + ".wt"} {
        CheckBlockSize(block_size);
        if (block_size > 1) {
            auto block_size_ext = ".d" + std::to_string(block_size);
            m_suffix_array += block_size_ext;
            m_string_btree += block_size_ext;
            m_wavelet_tree += block_size_ext;
        }
    }

    const std::string& GetCompressedTextPath() const noexcept {
        return m_compressed_text;
    }
    const std::string& GetSuffixArrayPath() const noexcept {
        return m_suffix_array;
    }
    const std::string& GetStringBTreePath() const noexcept {
        return m_string_btree;
    }
    const std::string& GetWaveletTreePath() const {
        return m_wavelet_tree;
    }

private:
    std::string m_compressed_text;
    std::string m_suffix_array;
    std::string m_string_btree;
    std::string m_wavelet_tree;
};

template <u8 block_size>
void BuildAllStructures(std::string dna_path) {
    CheckBlockSize(block_size);

    NameGenerator name_gen{dna_path, block_size};

    auto done = [] {
        std::cout << "\ndone\n";
    };

    // 0) Zero Term
    std::cout << "Make text zero terminated -> " << dna_path << std::endl;
    MakeZeroTerminated(dna_path);

    // 1) Compr
    const auto& dna_compr_path = name_gen.GetCompressedTextPath();
    std::cout << "Build compressed dna text -> " << dna_compr_path << std::endl;
    BuildCompressedDnaFromTextDna(dna_path, dna_compr_path, block_size);

    // 2) SA
    const auto& suff_arr_path = name_gen.GetSuffixArrayPath();
    std::cout << "Build suffix array -> " << suff_arr_path << std::endl;
    BuildSuffArrayFromComprDna(dna_compr_path, suff_arr_path, block_size);

    const auto& sbt_path = name_gen.GetStringBTreePath();
    if constexpr (block_size == 1) {
        using SbtT = DNA_SBT::StringBTree<DnaSymb>;
        ObjectFileHolder dna_file_holder{dna_compr_path};
        DnaDataAccessor dna{dna_file_holder};

        std::cout << "Build string b-tree -> " << sbt_path << std::endl;
        SbtT::Build(sbt_path, dna, suff_arr_path);
    } else {
        using SbtT = DNA_SBT::StringBTree<DnaSymbSeq<block_size>>;

        ObjectFileHolder dna_file_holder{dna_compr_path};
        DnaSeqDataAccessor<block_size> dna{dna_file_holder};

        if (auto sa_size = ObjectFileHolder{suff_arr_path}.Size(); sa_size != dna.Size()) {
            throw std::runtime_error{"Incorrect size of SA and DNA: " + std::to_string(sa_size) +
                                     " != " + std::to_string(dna.Size())};
        }

        std::cout << "Build string b-tree -> " << sbt_path << std::endl;
        SbtT::Build(sbt_path, dna, suff_arr_path);

        ObjectFileHolder suff_arr_holder{suff_arr_path};
        const str_pos_t* suff_arr = (const str_pos_t*)suff_arr_holder.cbegin();

        ReverseBWTDnaSeqAccessor rev_num_dna{dna, suff_arr};
        const std::size_t rev_num_alph_size = std::pow(8, block_size);

        const auto& wt_path = name_gen.GetWaveletTreePath();
        std::cout << "Build wavelet tree -> " << wt_path << std::endl;
        WaveletTreeOnDisk::Build(wt_path, rev_num_dna, rev_num_alph_size);
    }
}

int main_blocking_1() {
    std::string btree_name = "dna_btree.bin";
    const std::string data_dir = "../../data/T_SA/";
    const char* data_size_arr[] = {"1Kb", "1Mb", "20MB", "100MB", "200MB"};

    const str_len_t max_print_len = 25;

    const char* data_size = data_size_arr[1];
    std::string dna_data_path = data_dir + "dna." + data_size + ".comp";
    std::string dna_data_sa_path = dna_data_path + ".sa";
    std::string sbt_bin_path = btree_name + '.' + data_size;

    std::cout << "DNA: " << dna_data_path << std::endl;
    std::cout << "SA:  " << dna_data_sa_path << "\n\n";

    using SbtT = DNA_SBT::StringBTree<DnaSymb>;

    ObjectFileHolder dna_file_holder{dna_data_path};
    DnaDataAccessor dna_data{dna_file_holder};

// #define CACHE_SBT
#ifndef CACHE_SBT
    std::cout << "SBT building started\n";
    auto sbt = SbtT::Build(sbt_bin_path, dna_data, dna_data_sa_path);
    // sbt.Dump();
    std::cout << "SBT building finished\n";
#else
    SbtT sbt{sbt_bin_path, dna_data_path};
    std::cout << "SBT loaded\n";
#endif

    ObjectFileHolder suff_arr_holder{dna_data_sa_path};
    const str_pos_t* suff_arr = (const str_pos_t*)suff_arr_holder.cbegin();

    while (true) {
        std::string str;
        std::getline(std::cin, str);
        if (str == "end") {
            return 0;
        }

        DnaBuffer dna_buf{str};
        DnaDataAccessor pattern = dna_buf.GetAccessor();

        auto [pos, sa_pos, sa_pos_right, is_finded] = sbt.Search(pattern, dna_data);
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
            std::cout << "len: " << answer_len << ", sa pos: " << sa_pos << std::endl;

            str_pos_t str_pos_from_sa = suff_arr[sa_pos];
            if (pos != str_pos_from_sa) {
                std::cout << pos << " == " << str_pos_from_sa << " -> " << std::boolalpha
                          << (pos == str_pos_from_sa) << std::endl;
                throw std::runtime_error{"Incorrect SA index!"};
            }
        } else {
            std::cout << "not found" << std::endl;
        }
    }

    return 0;
}

template <u8 d>
std::tuple<DnaSymbSeq<d>, DnaBuffer, std::size_t> GetLeftRightPattern(const std::string& str,
                                                                      std::size_t k) {
    assert(k < d);
    assert(str.size() > d);

    DnaSymbSeq<d> left_patt;
    for (u8 i = 0; i < d; ++i) {
        DnaSymb symb = DnaSymb::A;
        if (i < k) {
            auto [getted_symb, is_good] = ConvertTextDnaSymb2DnaSymb(str[k - 1 - i]);
            if (!is_good) {
                throw std::invalid_argument{"Incorect pattern"};
            }
            symb = getted_symb;
        }
        left_patt.Set(symb, i);
    }

    auto right_patt_str = str.substr(k);

    std::size_t num_term_symb = 0;

    const bool is_pattern_div_d = right_patt_str.size() % d == 0;
    if (!is_pattern_div_d) {
        str_len_t add_size = d - right_patt_str.size() % d;
        right_patt_str.resize(right_patt_str.size() + add_size);

        for (str_pos_t i = right_patt_str.size() - add_size; i < right_patt_str.size(); ++i) {
            right_patt_str[i] = '\0';
        }

        num_term_symb = add_size;
    }

    DnaBuffer right_patt_buf_d1{right_patt_str};
    if (right_patt_buf_d1.GetAccessor().Size() % d) {
        std::cout << right_patt_buf_d1.GetAccessor().Size() << std::endl;
        throw std::runtime_error{"Too strange"};
    }

    return {left_patt, right_patt_buf_d1, num_term_symb};
}

template <u8 d>
int main_blocking_d() {
    std::string btree_name = "dna_btree.bin";
    const std::string data_dir = "../../data/T_SA/";
    const char* data_size_arr[] = {"1KB", "1MB", "20MB", "100MB", "200MB"};

    const str_len_t max_print_len = 25 / 2;

    const char* data_size = data_size_arr[2];
    std::string dna_data_path = data_dir + "dna." + data_size + ".comp";
    // std::string dna_data_path = data_dir + "micro.comp";
    std::string dna_data_sa_path = dna_data_path + ".sa.d" + std::to_string(d);
    std::string sbt_bin_path = btree_name + '.' + data_size + ".d" + std::to_string(d);

    std::cout << "DNA: " << dna_data_path << std::endl
              << "SA:  " << dna_data_sa_path << std::endl
              << "d:   " << (uint)d << std::endl
              << std::endl;

    using SbtT = DNA_SBT::StringBTree<DnaSymbSeq<d>>;

    ObjectFileHolder dna_file_holder{dna_data_path};
    DnaSeqDataAccessor<d> dna_data{dna_file_holder};

    if (auto sa_size = ObjectFileHolder{dna_data_sa_path}.Size(); sa_size != dna_data.Size()) {
        throw std::runtime_error{"Incorrect size of SA and DNA: " + std::to_string(sa_size) +
                                 " != " + std::to_string(dna_data.Size())};
    }

#define CACHE_SBT
#ifndef CACHE_SBT
    std::cout << "SBT building started\n";
    auto sbt = SbtT::Build(sbt_bin_path, dna_data, dna_data_sa_path);
    // sbt.Dump();
    std::cout << "SBT building finished\n";
#else
    SbtT sbt{sbt_bin_path};
    std::cout << "SBT loaded\n";
#endif

    ObjectFileHolder suff_arr_holder{dna_data_sa_path};
    const str_pos_t* suff_arr = (const str_pos_t*)suff_arr_holder.cbegin();

    ReverseBWTDnaSeqAccessor rev_num_dna_data{dna_data, suff_arr};
    const std::size_t rev_num_alph_size = std::pow(8, d);

    std::cout << "WT building started\n";
#define CACHE_WT
#ifndef CACHE_WT
    auto wt_file = WaveletTreeOnDisk::Build("wt_tree.20MB", rev_num_dna_data, rev_num_alph_size);
#else
    const auto& wt_file = WaveletTreeOnDisk{"wt_tree.20MB"};
#endif
    const auto& wt = wt_file.Get();

    std::cout << "WT building finished\n\n";

    while (true) {
        std::string str;
        std::getline(std::cin, str);
        if (str == "end") {
            return 0;
        }

        if (str.size() <= d) {
            std::cout << "Pattern len must be more " << (int)d << std::endl;
            continue;
        }

        bool is_finded = false;
        for (std::size_t k = 1; k < d; ++k) {
            auto [left_pattern, right_patt_buf_d1, num_term_symb] = GetLeftRightPattern<d>(str, k);

            auto right_dna_d1 = right_patt_buf_d1.GetAccessor();
            DnaSeqDataAccessor<d> right_pattern{right_dna_d1.data(), right_dna_d1.Size() / d};

            auto [pos, sa_pos, sa_pos_right, lcp] = sbt.Search(right_pattern, dna_data);

            if (num_term_symb == 0) {
                is_finded = lcp == right_pattern.Size();
            } else {
                if (lcp == right_pattern.Size() - 1) {
                    auto patt_tail = right_pattern[lcp];
                    auto dna_tail = dna_data[pos + lcp];

                    is_finded = true;
                    for (std::size_t i = 0; i < d - num_term_symb; ++i) {
                        if (patt_tail[i] != dna_tail[i]) {
                            is_finded = false;
                            break;
                        }
                    }
                }
            }

            if (!is_finded) {
                std::cout << "k: " << k << "not found" << std::endl;
                continue;
            }

            std::cout << "sa_pos: " << sa_pos << ", sa_pos_right: " << sa_pos_right << std::endl;

            std::cout << "left_pattern: " << left_pattern << std::endl;
            const auto left_pattern_num = DnaSeq2Number(left_pattern);
            auto [res_pos, is_rank_finded] =
                wt.GetFirstRank(left_pattern_num, 3 /* bits */ * k, sa_pos, sa_pos_right);

            if (!is_rank_finded) {
                std::cout << "WT: rank not found\n";
                continue;
            }
            // std::size_t rank_begin = 0;
            // std::cout << "left_pattern_num: " << left_pattern_num << std::endl;
            // for (std::size_t i = 0; i + 1 < left_pattern_num; ++i) {
            //     rank_begin += symb_freq[i];
            // }

            // const auto res_pos = /*rank_begin + */ rank + sa_pos;
            // std::cout << "rank: " << rank << std::endl;
            // std::cout << "rank_begin: " << rank_begin << std::endl;
            std::cout << "pos: " << res_pos << std::endl;

            std::cout << "SYMB: " << dna_data[suff_arr[res_pos] - 1] << '\''
                      << dna_data[suff_arr[res_pos] + 0] << '\'' << dna_data[suff_arr[res_pos] + 1]
                      << std::endl;

            // std::cout << "SYMB: " << dna_data[suff_arr[res_pos] + 0] << '\''
            //                       << dna_data[suff_arr[res_pos] + 1] << '\''
            //                       << dna_data[suff_arr[res_pos] + 2] << std::endl;

            // std::cout << "SYMB: " << dna_data[suff_arr[res_pos + 1] + 0] << '\''
            //                       << dna_data[suff_arr[res_pos + 1] + 1] << '\''
            //                       << dna_data[suff_arr[res_pos + 1] + 2] << std::endl;

            // std::cout << "is_finded: " << is_finded << std::endl;
            // std::cout << "num_term_symb: " << num_term_symb << std::endl;
            // std::cout << "lcp: " << lcp << std::endl;
            if (is_finded) {
                str_len_t answer_len = dna_data.Size() - pos;
                str_len_t len = std::min(answer_len, max_print_len);

                std::cout << "l res: `";
                for (str_pos_t i = 0; i < len; ++i) {
                    std::cout << dna_data[pos + i];
                }
                if (answer_len > len) {
                    std::cout << "...";
                }
                std::cout << "`" << std::endl;

                std::cout << "r res: `";
                for (str_pos_t i = 0; i < len; ++i) {
                    std::cout << dna_data[suff_arr[sa_pos_right - 1] + i];
                }

                if (answer_len > len) {
                    std::cout << "...";
                }

                std::cout << "`" << std::endl;
                std::cout << "len: " << answer_len << ", sa pos: " << sa_pos << std::endl;

                str_pos_t str_pos_from_sa = suff_arr[sa_pos];
                if (pos != str_pos_from_sa) {
                    std::cout << pos << " == " << str_pos_from_sa << " -> " << std::boolalpha
                              << (pos == str_pos_from_sa) << std::endl;
                    throw std::runtime_error{"Incorrect SA index!"};
                }

                break;
            } else {
                std::cout << "not found" << std::endl;
            }
            std::cout << std::endl;

            if (is_finded) {
                break;
            }
        }
    }

    return 0;
}

template <u8 d>
void print_num_leaves() {
    std::cout << (int)d << std::endl;
    if constexpr (d == 1) {
        using SbtT = DNA_SBT::StringBTree<DnaSymb>;
        std::cout << '\t' << SbtT::InnerNode::num_leaves << std::endl;
        std::cout << '\t' << SbtT::LeafNode::num_leaves << std::endl;
    } else {
        using SbtT = DNA_SBT::StringBTree<DnaSymbSeq<d>>;
        std::cout << '\t' << SbtT::InnerNode::num_leaves << std::endl;
        std::cout << '\t' << SbtT::LeafNode::num_leaves << std::endl;
    }
}

template <u8... ds>
void print_num_leaves_arr() {
    (print_num_leaves<ds>(), ...);
}

int main() try {
    BuildAllStructures<1>("../../data/T_SA/dna.1MB");
    BuildAllStructures<2>("../../data/T_SA/dna.1MB");

    // constexpr u8 d = 4;

    // if constexpr (d == 1) {
    //     return main_blocking_1();
    // } else {
    //     return main_blocking_d<d>();
    // }

} catch (std::exception& exc) {
    std::cerr << "Exception: " << exc.what() << std::endl;
}
