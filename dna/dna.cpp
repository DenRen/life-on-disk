#include "dna.h"

#include <algorithm>
#include <limits>

std::pair<DnaSymb, bool> ConvertTextDnaSymb2DnaSymb(u8 symb) noexcept {
    bool is_dna_symb = true;
    auto convert = [&]() {
        switch (symb) {
            case 'A':
                return DnaSymb::A;
            case 'C':
                return DnaSymb::C;
            case 'T':
                return DnaSymb::T;
            case 'G':
                return DnaSymb::G;
            case 'N':
                return DnaSymb::N;
            case '\0':
                return DnaSymb::TERM;
            default:
                is_dna_symb = false;
                return DnaSymb::TERM;
        }
    };
    return {convert(), is_dna_symb};
}

constexpr u8 c_dna_symb_size = 3;
static_assert(c_dna_symb_size <= 8);

void InsertDnaSymb(u8* begin, uint64_t dna_pos, DnaSymb dna_symb_) {
    const u8 dna_symb = (u8)dna_symb_;

    uint64_t bit_pos = c_dna_symb_size * dna_pos;
    u8* data = begin + bit_pos / 8;

    uint64_t local_bit_pos = bit_pos % 8;
    if (local_bit_pos <= 8 - c_dna_symb_size) {
        u8 ones = (1u << c_dna_symb_size) - 1u;
        u8 lshift = 8 - c_dna_symb_size - local_bit_pos;
        u8 mask = ones << lshift;

        *data = (*data & ~mask) | (dna_symb << lshift);
    } else {
        u8 l_size = 8 - local_bit_pos;
        u8 r_size = c_dna_symb_size - l_size;

        u8 l_mask = (1u << l_size) - 1u;
        u8 r_mask = ((1u << r_size) - 1u) << (8 - r_size);

        *data = (*data & ~l_mask) | (dna_symb >> r_size);
        ++data;
        *data = (*data & ~r_mask) | (dna_symb << (8 - r_size));
    }
}

DnaSymb ReadDnaSymb(const u8* begin, uint64_t dna_pos) {
    uint64_t bit_pos = c_dna_symb_size * dna_pos;
    const u8* data = begin + bit_pos / 8;

    uint64_t local_bit_pos = bit_pos % 8;
    if (local_bit_pos <= 8 - c_dna_symb_size) {
        u8 mask = (1u << c_dna_symb_size) - 1u;
        u8 rshift = 8 - c_dna_symb_size - local_bit_pos;

        u8 res = (*data >> rshift) & mask;
        return DnaSymb{res};
    } else {
        u8 l_size = 8 - local_bit_pos;
        u8 r_size = c_dna_symb_size - l_size;

        u8 l_mask = (1u << l_size) - 1u;

        u8 l_part = (*data & l_mask) << r_size;
        u8 r_part = (*(data + 1) >> (8 - r_size));

        u8 res = l_part | r_part;
        return DnaSymb{res};
    }
}

const char* DNASymb2String(DnaSymb dna_symb) {
    switch (dna_symb) {
        case DnaSymb::A:
            return "A";
        case DnaSymb::C:
            return "C";
        case DnaSymb::T:
            return "T";
        case DnaSymb::G:
            return "G";
        case DnaSymb::N:
            return "N";
        case DnaSymb::TERM:
            return "\\0";
        default:
            return "UNKNOWN";
    }
}

std::ostream& operator<<(std::ostream& os, const DnaSymb& dna_symb) {
    return os << DNASymb2String(dna_symb);
}

ObjectFileHolder::ObjectFileHolder(std::string_view compressed_dna_path)
    : m_file_map{compressed_dna_path}
    , m_size{*(const uint64_t*)m_file_map.begin()} {}

ObjectFileHolder BuildCompressedDnaFromTextDna(std::string_view text_dna_path,
                                               std::string_view compressed_dna_path) {
    const FileMapperRead mapper_text_dna{text_dna_path};
    FileMapperWrite mapper_comp_dna{compressed_dna_path,
                                    ObjectFileHolder::c_header_size + mapper_text_dna.Size()};

    u8* const dna_begin = mapper_comp_dna.begin() + ObjectFileHolder::c_header_size;
    uint64_t dna_pos = 0;

    bool is_header = false;
    for (u8 symb : mapper_text_dna) {
        if (is_header) {
            if (symb == '\n')
                is_header = false;
            continue;
        }

        if (symb == '>') {
            is_header = true;
        } else {
            if (auto [dna_symb, is_dna_symb] = ConvertTextDnaSymb2DnaSymb(symb); is_dna_symb) {
                InsertDnaSymb(dna_begin, dna_pos++, dna_symb);
            }
        }
    }

    if (ReadDnaSymb(dna_begin, dna_pos - 1) != DnaSymb::TERM) {
        InsertDnaSymb(dna_begin, dna_pos++, DnaSymb::TERM);
    }

    uint64_t* num_dna_symb = (uint64_t*)mapper_comp_dna.begin();
    *num_dna_symb = dna_pos;

    uint64_t num_bits = 8 * ObjectFileHolder::c_header_size + c_dna_symb_size * dna_pos;
    uint64_t num_bytes = num_bits / 8 + (num_bits % 8 != 0);
    mapper_comp_dna.Truncate(num_bytes);

    return {compressed_dna_path};
}

DnaDataAccessor::DnaDataAccessor(const u8* dna_data_begin, uint64_t num_dna) noexcept
    : m_dna_data_begin{dna_data_begin}
    , m_num_dna{num_dna} {}

DnaSymb DnaDataAccessor::operator[](uint64_t index) const noexcept {
    return ReadDnaSymb(m_dna_data_begin, index);
}


#if 1

struct suffix {
    uint32_t index;
    int rank[2];
};

#include <execution>

ObjectFileHolder BuildSuffArrayFromCompressedDna(std::string_view compressed_dna_path,
                                                 std::string_view suff_arr_path) {
    ObjectFileHolder dna_file_holder{compressed_dna_path};
    DnaDataAccessor dna{dna_file_holder.cbegin(), dna_file_holder.Size()};

    using sa_index_t = uint32_t;

    if (dna.Size() > std::numeric_limits<sa_index_t>::max()) {
        throw std::runtime_error{"Too big dna size for build SA"};
    }

    const sa_index_t n = (sa_index_t)dna.Size();

    auto cmp = [](const suffix& a, const suffix& b) {
        return (a.rank[0] == b.rank[0]) ? (a.rank[1] < b.rank[1]) : (a.rank[0] < b.rank[0]);
    };

    std::vector<suffix> suffixes(n);
    for (sa_index_t i = 0; i < n; i++) {
        suffixes[i].index = i;
        suffixes[i].rank[0] = (int)dna[i];
        suffixes[i].rank[1] = ((i + 1) < n) ? ((int)dna[i + 1]) : -1;
    }

    std::sort(std::execution::par_unseq, suffixes.begin(), suffixes.end(), cmp);

    std::vector<sa_index_t> ind(n);
    for (sa_index_t k = 4; k < 2 * n; k = k * 2) {
        int rank = 0;
        int prev_rank = suffixes[0].rank[0];
        suffixes[0].rank[0] = rank;
        ind[suffixes[0].index] = 0;

        for (sa_index_t i = 1; i < n; i++) {
            if (suffixes[i].rank[0] == prev_rank &&
                suffixes[i].rank[1] == suffixes[i - 1].rank[1]) {
                prev_rank = suffixes[i].rank[0];
                suffixes[i].rank[0] = rank;
            } else {
                prev_rank = suffixes[i].rank[0];
                suffixes[i].rank[0] = ++rank;
            }
            ind[suffixes[i].index] = i;
        }

        for (sa_index_t i = 0; i < n; i++) {
            sa_index_t nextindex = suffixes[i].index + k / 2;
            suffixes[i].rank[1] = (nextindex < n) ? suffixes[ind[nextindex]].rank[0] : -1;
        }

        std::sort(std::execution::par_unseq, suffixes.begin(), suffixes.end(), cmp);
    }

    {
        FileMapperWrite suff_arr_mapper{suff_arr_path, sizeof(uint64_t) + sizeof(sa_index_t) * n};
        *(uint64_t*)suff_arr_mapper.begin() = n;

        sa_index_t* suffixArr = (sa_index_t*)(suff_arr_mapper.begin() + 8);
        for (uint32_t i = 0; i < n; ++i) {
            suffixArr[i] = suffixes[i].index;
        }
    }

    return {suff_arr_path};
}

#elif 1

#include <string.h>

ObjectFileHolder BuildSuffArrayFromCompressedDna(std::string_view compressed_dna_path,
                                                 std::string_view suff_arr_path) {
    ObjectFileHolder dna_file_holder{compressed_dna_path};
    DnaDataAccessor dna{dna_file_holder.cbegin(), dna_file_holder.Size()};

    using sa_index_t = uint32_t;

    if (dna.Size() > std::numeric_limits<sa_index_t>::max()) {
        throw std::runtime_error{"Too big dna size for build SA"};
    }

    const sa_index_t n = (sa_index_t)dna.Size();
    const sa_index_t alphabet = 256;

    std::vector<int> p(n), cnt(n), c(n);
    memset(cnt.data(), 0, alphabet * sizeof(int));
    for (int i = 0; i < n; ++i) {
        ++cnt[(u8)dna[i]];
    }
    for (int i = 1; i < alphabet; ++i) {
        cnt[i] += cnt[i - 1];
    }
    for (int i = 0; i < n; ++i) {
        p[--cnt[(u8)dna[i]]] = i;
    }
    c[p[0]] = 0;
    int classes = 1;
    for (int i = 1; i < n; ++i) {
        if (dna[p[i]] != dna[p[i - 1]])
            ++classes;
        c[p[i]] = classes - 1;
    }

    std::vector<int> pn(n), cn(n);
    for (int h = 0; (1 << h) < n; ++h) {
        for (int i = 0; i < n; ++i) {
            pn[i] = p[i] - (1 << h);
            if (pn[i] < 0)
                pn[i] += n;
        }
        memset(cnt.data(), 0, classes * sizeof(int));
        for (int i = 0; i < n; ++i) {
            ++cnt[c[pn[i]]];
        }
        for (int i = 1; i < classes; ++i) {
            cnt[i] += cnt[i - 1];
        }
        for (int i = n - 1; i >= 0; --i) {
            p[--cnt[c[pn[i]]]] = pn[i];
        }
        cn[p[0]] = 0;
        classes = 1;
        for (int i = 1; i < n; ++i) {
            int mid1 = (p[i] + (1 << h)) % n, mid2 = (p[i - 1] + (1 << h)) % n;
            if (c[p[i]] != c[p[i - 1]] || c[mid1] != c[mid2])
                ++classes;
            cn[p[i]] = classes - 1;
        }
        memcpy(c.data(), cn.data(), n * sizeof(int));
    }

    {
        FileMapperWrite suff_arr_mapper{suff_arr_path, sizeof(uint64_t) + sizeof(sa_index_t) * n};
        *(uint64_t*)suff_arr_mapper.begin() = n;

        sa_index_t* suffixArr = (sa_index_t*)(suff_arr_mapper.begin() + 8);
        for (uint32_t i = 0; i < n; ++i) {
            suffixArr[i] = p[i];
        }
    }

    return {suff_arr_path};
}

#else

ObjectFileHolder BuildSuffArrayFromCompressedDna(std::string_view compressed_dna_path,
                                                 std::string_view suff_arr_path) {
    ObjectFileHolder dna_file_holder{compressed_dna_path};
    DnaDataAccessor dna{dna_file_holder.cbegin(), dna_file_holder.Size()};

    using sa_index_t = uint32_t;

    if (dna.Size() > std::numeric_limits<sa_index_t>::max()) {
        throw std::runtime_error{"Too big dna size for build SA"};
    }

    const sa_index_t n = (sa_index_t)dna.Size();
    const sa_index_t alphabet = 6;

    std::vector<sa_index_t> sa(n);
    for (sa_index_t i = 0; i < n; ++i) {
        sa[i] = i;
    }

    std::sort(sa.begin(), sa.end(), [n, &dna](sa_index_t lhs, sa_index_t rhs) {
        sa_index_t len = n - std::max(lhs, rhs);
        for (sa_index_t i = 0; i < len; ++i) {
            if (dna[lhs + i] != dna[rhs + i]) {
                return (u8)dna[lhs + i] < (u8)dna[rhs + i];
            }
        }

        return false;
    });

    {
        FileMapperWrite suff_arr_mapper{suff_arr_path, sizeof(uint64_t) + sizeof(sa_index_t) * n};
        *(uint64_t*)suff_arr_mapper.begin() = n;

        sa_index_t* suffixArr = (sa_index_t*)(suff_arr_mapper.begin() + 8);
        for (uint32_t i = 0; i < n; ++i) {
            suffixArr[i] = sa[i];
        }
    }

    return {suff_arr_path};
}

#endif
