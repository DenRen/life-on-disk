#include "dna.h"
#include "../common/help_func.h"

#include <algorithm>
#include <limits>
#include <execution>

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
            case '\0':
                return DnaSymb::TERM;
            default:
                is_dna_symb = false;
                return DnaSymb::TERM;
        }
    };
    return {convert(), is_dna_symb};
}

void InsertDnaSymb(u8* begin, uint64_t dna_pos, DnaSymb dna_symb_) {
    const u8 dna_symb = (u8)dna_symb_;

    uint64_t bit_pos = DnaSymbBitSize * dna_pos;
    u8* data = begin + bit_pos / 8;

    uint64_t local_bit_pos = bit_pos % 8;
    if (local_bit_pos <= 8 - DnaSymbBitSize) {
        u8 ones = (1u << DnaSymbBitSize) - 1u;
        u8 lshift = 8 - DnaSymbBitSize - local_bit_pos;
        u8 mask = ones << lshift;

        *data = (*data & ~mask) | (dna_symb << lshift);
    } else {
        u8 l_size = 8 - local_bit_pos;
        u8 r_size = DnaSymbBitSize - l_size;

        u8 l_mask = (1u << l_size) - 1u;
        u8 r_mask = ((1u << r_size) - 1u) << (8 - r_size);

        *data = (*data & ~l_mask) | (dna_symb >> r_size);
        ++data;
        *data = (*data & ~r_mask) | (dna_symb << (8 - r_size));
    }
}

DnaSymb ReadDnaSymb(const u8* begin, uint64_t dna_pos) {
    uint64_t bit_pos = DnaSymbBitSize * dna_pos;
    const u8* data = begin + bit_pos / 8;

    uint64_t local_bit_pos = bit_pos % 8;
    if (local_bit_pos <= 8 - DnaSymbBitSize) {
        u8 mask = (1u << DnaSymbBitSize) - 1u;
        u8 rshift = 8 - DnaSymbBitSize - local_bit_pos;

        u8 res = (*data >> rshift) & mask;
        return DnaSymb{res};
    } else {
        u8 l_size = 8 - local_bit_pos;
        u8 r_size = DnaSymbBitSize - l_size;

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
                                               std::string_view compressed_dna_path, uint d_max) {
    if (d_max < 1) {
        throw std::runtime_error{"d_max must be >= 1"};
    }

    const FileMapperRead mapper_text_dna{text_dna_path};
    FileMapperWrite mapper_comp_dna{
        compressed_dna_path,
        ObjectFileHolder::c_header_size + mapper_text_dna.Size() + DivUp(d_max, DnaSymbBitSize)};

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

    uint64_t dna_pos_with_d = d_max * DivUp(dna_pos, d_max);

    uint64_t num_bits = 8 * ObjectFileHolder::c_header_size + DnaSymbBitSize * dna_pos_with_d;
    uint64_t num_bytes = DivUp(num_bits, 8);
    mapper_comp_dna.Truncate(num_bytes);

    return {compressed_dna_path};
}

DnaDataAccessor::DnaDataAccessor(const u8* dna_data_begin, uint64_t num_dna)
    : m_dna_data_begin{dna_data_begin}
    , m_num_dna{num_dna} {}

DnaDataAccessor::DnaDataAccessor(const ObjectFileHolder& dna_file_holder)
    : DnaDataAccessor{dna_file_holder.cbegin(), dna_file_holder.Size()} {}

DnaSymb DnaDataAccessor::operator[](uint64_t index) const noexcept {
    return ReadDnaSymb(m_dna_data_begin, index);
}

struct suffix {
    uint32_t index;
    int rank[2];
};

static int DnaSymbSeq2Int(const DnaDataAccessor& dna, uint i_begin, uint d) {
    uint res = 0;
    for (uint i = 0; i < d; ++i) {
        res += ((uint)dna[i_begin + i]) << (DnaSymbBitSize * (d - 1 - i));
    }
    return res;
}

ObjectFileHolder BuildSuffArrayFromCompressedDna(std::string_view compressed_dna_path,
                                                 std::string_view suff_arr_path, uint d) {
    if (d < 1) {
        throw std::runtime_error{"d_max must be >= 1"};
    }

    ObjectFileHolder dna_file_holder{compressed_dna_path};
    DnaDataAccessor dna{dna_file_holder.cbegin(), dna_file_holder.Size()};

    using sa_index_t = uint32_t;

    auto num_items = dna.Size() / d;
    if (num_items > std::numeric_limits<sa_index_t>::max()) {
        throw std::runtime_error{"Too big dna size for build SA"};
    }

    const sa_index_t n = (sa_index_t)num_items;

    auto cmp = [](const suffix& a, const suffix& b) {
        return (a.rank[0] == b.rank[0]) ? (a.rank[1] < b.rank[1]) : (a.rank[0] < b.rank[0]);
    };

    std::vector<suffix> suffixes(n);
    for (sa_index_t i = 0; i < n; i++) {
        suffixes[i].index = i;
        suffixes[i].rank[0] = DnaSymbSeq2Int(dna, d * i, d);
        suffixes[i].rank[1] = ((i + 1) < n) ? DnaSymbSeq2Int(dna, d * (i + 1), d) : -1;
    }
    std::cout << "DnaSeq to num complete\n";

    std::sort(std::execution::par_unseq, suffixes.begin(), suffixes.end(), cmp);

    std::vector<sa_index_t> ind(n);
    for (sa_index_t k = 4; k < 2 * n; k = k * 2) {
        std::cout << k << " / " << n << '\n';
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

DnaBuffer::DnaBuffer(std::string_view dna_str)
    : m_num_dna{dna_str.size()} {
    m_dna_buf.resize(DivUp(m_num_dna * DnaSymbBitSize, 8));
    uint8_t* dest_data_begin = m_dna_buf.data();

    for (uint32_t i = 0; i < dna_str.size(); ++i) {
        auto [dna_symb, is_dna_symb] = ConvertTextDnaSymb2DnaSymb(dna_str[i]);
        if (!is_dna_symb) {
            throw std::invalid_argument{"DNA string have invalid symbol"};
        }

        InsertDnaSymb(dest_data_begin, i, dna_symb);
    }
}