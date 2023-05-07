#include "dna.h"

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

static const char* DNASymb2String(DnaSymb dna_symb) {
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

DNAFileHolder::DNAFileHolder(std::string_view compressed_dna_path)
    : m_file_map{compressed_dna_path}
    , m_size{*(const uint64_t*)m_file_map.begin()} {}

DNAFileHolder DNAFileHolder::BuildFromTextDNA(std::string_view text_dna_path,
                                              std::string_view compressed_dna_path) {
    const FileMapperRead mapper_text_dna{text_dna_path};
    FileMapperWrite mapper_comp_dna{compressed_dna_path, c_data_shift + mapper_text_dna.Size()};

    u8* const dna_begin = mapper_comp_dna.begin() + c_data_shift;
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

    uint64_t num_bits = 8 * c_data_shift + c_dna_symb_size * dna_pos;
    uint64_t num_bytes = num_bits / 8 + (num_bits % 8 != 0);
    mapper_comp_dna.Truncate(num_bytes);

    return {compressed_dna_path};
}
