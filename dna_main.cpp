#include <cstdio>
#include <iostream>
#include <future>
#include <random>

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
    std::cout << std::endl;
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

constexpr bool enable_cache = true;

namespace little_lab {
const std::string data_size_arr[] = {"1KB", "1MB", "20MB", "100MB", "200MB"};

std::string GetDataPath(const std::string& size) {
    return "../../data/T_SA/dna." + size;
}

void BuildAllStruct() {
    if constexpr (!enable_cache) {
        std::vector<std::future<void>> pull;
        for (const auto& data_size : data_size_arr) {
            std::string data_path = GetDataPath(data_size);
            MakeZeroTerminated(data_path);

            auto build = [&]<u8... ds> {
                (pull.emplace_back(std::async(BuildAllStructures<ds>, data_path)), ...);
            };

            build.template operator()<1, 2, 3, 4, 5, 6, 7, 8>();

            for (auto& task : pull) {
                task.get();
            }
            pull.clear();
        }
    }
}

auto now() {
    return std::chrono::high_resolution_clock::now();
}

auto to_microsec(auto time_start, auto time_finish) {
    auto delta = time_finish - time_start;
    return std::chrono::duration_cast<std::chrono::microseconds>(delta).count();
}

// Mean, disp
template <typename T>
std::pair<T, T> CalcMeanDisp(const std::vector<T>& buf) {
    const auto size = buf.size();
    T mean = std::accumulate(std::begin(buf), std::end(buf), T{}) / size;

    T disp = {};
    for (std::size_t i = 0; i < size; ++i) {
        auto delta = buf[i] > mean ? buf[i] - mean : mean - buf[i];
        disp += delta * delta;
    }
    disp = std::sqrt(disp / size);

    return {mean, disp};
}

std::string DnaSeq2String(const DnaDataAccessor& dna, str_pos_t begin, str_pos_t end) {
    const auto size = end - begin;
    std::string res;
    res.resize(size);

    for (str_pos_t i = 0; i < size; ++i) {
        res[i] = *DNASymb2String(dna[i + begin]);
    }

    return res;
}

template <u8 block_size>
void little_lab(unsigned num_queries, unsigned data_size_index, str_len_t pattern_len) {
    auto uncompr_data_path = GetDataPath(data_size_arr[data_size_index]);
    NameGenerator name_gen{uncompr_data_path, block_size};

    const auto data_path = name_gen.GetCompressedTextPath();
    std::cout << "data_path: " << data_path << std::endl;
    std::cout << "block_size: " << (int)block_size << std::endl;

    ObjectFileHolder dna_file_holder{data_path};
    DnaDataAccessor dna_data{dna_file_holder};

    num_queries = (unsigned)dna_data.Size() / 35'000;
    std::cout << "num_queries: " << num_queries << std::endl;

    const str_len_t seed = 0xEDA + 0xDED * 32;
    std::mt19937_64 gen{seed};

    std::cout << "pattern_len: " << pattern_len << std::endl;

    std::vector<DnaBuffer> patterns;
    patterns.reserve(num_queries);

    std::vector<std::string> patterns_str;
    patterns_str.reserve(num_queries);

    using UniDistT = std::uniform_int_distribution<str_pos_t>;
    UniDistT pos_distrib{0, (str_len_t)dna_data.Size() - pattern_len - 1};
    for (unsigned i = 0; i < num_queries; ++i) {
        const auto pos = pos_distrib(gen);
        patterns.emplace_back(dna_data, pos, pos + pattern_len);
        patterns_str.emplace_back(DnaSeq2String(dna_data, pos, pos + pattern_len));
    }

    str_pos_t unused_counter = 0;
    using time_point = std::chrono::time_point<std::chrono::high_resolution_clock>;
    std::vector<std::pair<time_point, time_point>> times(num_queries);

    if constexpr (block_size == 1) {
        using SbtT = DNA_SBT::StringBTree<DnaSymb>;
        SbtT sbt{name_gen.GetStringBTreePath()};

        for (unsigned i = 0; i < num_queries; ++i) {
            const auto& patt_buf = patterns[i];
            auto time_start = now();

            const auto pattern = patt_buf.GetAccessor();
            auto [pos, sa_pos, sa_pos_right, is_finded] = sbt.Search(pattern, dna_data);
            unused_counter += sa_pos;

            auto time_finish = now();
            times[i] = {time_start, time_finish};
        }
    } else {
        DnaSeqDataAccessor<block_size> dna_data_seq{dna_file_holder};

        using SbtT = DNA_SBT::StringBTree<DnaSymbSeq<block_size>>;
        SbtT sbt{name_gen.GetStringBTreePath()};

        ObjectFileHolder suff_arr_holder{name_gen.GetSuffixArrayPath()};
        const str_pos_t* suff_arr = (const str_pos_t*)suff_arr_holder.cbegin();

        ReverseBWTDnaSeqAccessor rev_num_dna_data{dna_data_seq, suff_arr};
        const std::size_t rev_num_alph_size = std::pow(8, block_size);

        const auto& wt_file = WaveletTreeOnDisk{name_gen.GetWaveletTreePath()};
        const auto& wt = wt_file.Get();

        std::vector<std::tuple<DnaSymbSeq<block_size>, DnaBuffer, std::size_t>> left_right_patt_buf;
        left_right_patt_buf.reserve(num_queries * block_size);
        for (unsigned i = 0; i < num_queries; ++i) {
            const auto& pattern_str = patterns_str[i];
            for (std::size_t k = 0; k < block_size; ++k) {
                left_right_patt_buf.emplace_back(GetLeftRightPattern<block_size>(pattern_str, k));
            }
        }

        str_pos_t unused_counter = 0;
        for (unsigned i = 0; i < num_queries; ++i) {
            auto time_start = now();

            bool is_finded = false;
            for (std::size_t k = 0; k < block_size; ++k) {
                const auto& [left_pattern, right_patt_buf_d1, num_term_symb] =
                    left_right_patt_buf[k + i * block_size];

                auto right_dna_d1 = right_patt_buf_d1.GetAccessor();
                DnaSeqDataAccessor<block_size> right_pattern{right_dna_d1.data(),
                                                             right_dna_d1.Size() / block_size};

                auto [pos, sa_pos, sa_pos_right, lcp] = sbt.Search(right_pattern, dna_data_seq);

                if (num_term_symb == 0) {
                    is_finded = lcp == right_pattern.Size();
                } else {
                    if (lcp == right_pattern.Size() - 1) {
                        auto patt_tail = right_pattern[lcp];
                        auto dna_tail = dna_data_seq[pos + lcp];

                        is_finded = true;
                        for (std::size_t i = 0; i < block_size - num_term_symb; ++i) {
                            if (patt_tail[i] != dna_tail[i]) {
                                is_finded = false;
                                break;
                            }
                        }
                    }
                }

                if (is_finded) {
                    unused_counter += sa_pos;
                    break;
                }
            }

            auto time_finish = now();
            times[i] = {time_start, time_finish};
        }
    }

    if (unused_counter == 123) {
        volatile auto t = 3;
    }

    const auto num_sections = 5;
    const auto step = num_queries / num_sections;
    for (unsigned j = 0; j < num_sections; ++j) {
        std::vector<std::size_t> deltas((j + 1) * step);
        for (unsigned i = 0; i < deltas.size(); ++i) {
            const auto& [time_start, time_finish] = times[i];
            deltas[i] = to_microsec(time_start, time_finish);
        }

        auto [mean, disp] = CalcMeanDisp(deltas);
        std::cout << j << ") mean: " << mean << ", disp: " << disp << std::endl;
    }
}

}  // namespace little_lab

// #define BLK_SIZE

int main(int argc, char* argv[]) try {
    int block_size_input = atoi(argv[1]);
    unsigned num_queries = 0;
    unsigned data_size_index = atoi(argv[2]);
    str_len_t pattern_len = 320;

    switch (block_size_input) {
        case 1: {
            little_lab::little_lab<1>(num_queries, data_size_index, pattern_len);
        } break;
        case 2: {
            little_lab::little_lab<2>(num_queries, data_size_index, pattern_len);
        } break;
        case 3: {
            little_lab::little_lab<3>(num_queries, data_size_index, pattern_len);
        } break;
        case 4: {
            little_lab::little_lab<4>(num_queries, data_size_index, pattern_len);
        } break;
        case 5: {
            little_lab::little_lab<5>(num_queries, data_size_index, pattern_len);
        } break;
        case 6: {
            little_lab::little_lab<6>(num_queries, data_size_index, pattern_len);
        } break;
        case 7: {
            little_lab::little_lab<7>(num_queries, data_size_index, pattern_len);
        } break;
        case 8: {
            little_lab::little_lab<8>(num_queries, data_size_index, pattern_len);
        } break;
        default: {
            throw std::runtime_error{"Incorrect block size"};
        }
    }

    // constexpr u8 d = 2;

    // if constexpr (d == 1) {
    //     return main_blocking_1();
    // } else {
    //     return main_blocking_d<d>();
    // }

} catch (std::exception& exc) {
    std::cerr << "Exception: " << exc.what() << std::endl;
}
