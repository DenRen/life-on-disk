#include "dna.h"

std::pair<DnaSymb, bool> ConvertTextDnaSymb2DnaSymb(uint8_t symb) noexcept {
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

constexpr uint8_t c_dna_symb_size = 3;
static_assert(c_dna_symb_size <= 8);

void InsertDnaSymb(uint8_t* begin, uint64_t dna_pos, DnaSymb dna_symb_) {
    const uint8_t dna_symb = (uint8_t)dna_symb_;

    uint64_t bit_pos = c_dna_symb_size * dna_pos;
    uint8_t* data = begin + bit_pos / 8;

    uint64_t local_bit_pos = bit_pos % 8;
    if (local_bit_pos <= 8 - c_dna_symb_size) {
        uint8_t ones = (1u << c_dna_symb_size) - 1u;
        uint8_t lshift = 8 - c_dna_symb_size - local_bit_pos;
        uint8_t mask = ones << lshift;

        *data = (*data & ~mask) | (dna_symb << lshift);
    } else {
        uint8_t l_size = 8 - local_bit_pos;
        uint8_t r_size = c_dna_symb_size - l_size;

        uint8_t l_mask = (1u << l_size) - 1u;
        uint8_t r_mask = ((1u << r_size) - 1u) << (8 - r_size);

        *data = (*data & ~l_mask) | (dna_symb >> r_size);
        ++data;
        *data = (*data & ~r_mask) | (dna_symb << (8 - r_size));
    }
}

DnaSymb ReadDnaSymb(const uint8_t* begin, uint64_t dna_pos) {
    uint64_t bit_pos = c_dna_symb_size * dna_pos;
    const uint8_t* data = begin + bit_pos / 8;

    uint64_t local_bit_pos = bit_pos % 8;
    if (local_bit_pos <= 8 - c_dna_symb_size) {
        uint8_t mask = (1u << c_dna_symb_size) - 1u;
        uint8_t rshift = 8 - c_dna_symb_size - local_bit_pos;

        uint8_t res = (*data >> rshift) & mask;
        return DnaSymb{res};
    } else {
        uint8_t l_size = 8 - local_bit_pos;
        uint8_t r_size = c_dna_symb_size - l_size;

        uint8_t l_mask = (1u << l_size) - 1u;

        uint8_t l_part = (*data & l_mask) << r_size;
        uint8_t r_part = (*(data + 1) >> (8 - r_size));

        uint8_t res = l_part | r_part;
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