#pragma once

#include <cstdint>
#include <utility>
#include <iostream>

#include "../common/file_mapper.h"
#include "types.h"

inline bool operator<(DnaSymb lhs, DnaSymb rhs) noexcept {
    return (u8)lhs < (u8)rhs;
}

std::pair<DnaSymb, bool> ConvertTextDnaSymb2DnaSymb(uint8_t symb) noexcept;

void InsertDnaSymb(uint8_t* begin, uint64_t dna_pos, DnaSymb dna_symb);
DnaSymb ReadDnaSymb(const uint8_t* begin, uint64_t dna_pos);

const char* DNASymb2String(DnaSymb dna_symb);
std::ostream& operator<<(std::ostream& os, const DnaSymb& dna_symb);

class ObjectFileHolder {
public:
    constexpr static std::size_t c_header_size = sizeof(uint64_t);

    ObjectFileHolder(std::string_view compressed_dna_path);

    const uint8_t* cbegin() const noexcept {
        return m_file_map.begin() + c_header_size;
    }

    uint64_t Size() const noexcept {
        return m_size;
    }

private:
    FileMapperRead m_file_map;
    uint64_t m_size;
};

ObjectFileHolder BuildCompressedDnaFromTextDna(std::string_view text_dna_path,
                                               std::string_view compressed_dna_path,
                                               uint d_max = 1);

ObjectFileHolder BuildSuffArrayFromCompressedDna(std::string_view compressed_dna_path,
                                                 std::string_view suff_arr_path,
                                                 uint d = 1);

// Not garantee, that [Size() - 1] == TERM
class DnaDataAccessor {
public:
    DnaDataAccessor(const u8* dna_data_begin, uint64_t num_dna);
    DnaDataAccessor(const ObjectFileHolder& dna_file_holder);

    DnaSymb operator[](uint64_t index) const noexcept;
    uint64_t Size() const noexcept {
        return m_num_dna;
    }

    uint64_t StrSize(uint64_t index) const noexcept {
        return Size() - index;
    }

private:
    const u8* m_dna_data_begin;
    uint64_t m_num_dna;
};

class DnaBuffer {
public:
    DnaBuffer(std::string_view dna_str);

    DnaDataAccessor GetAccessor() const noexcept {
        return {m_dna_buf.data(), m_num_dna};
    }

private:
    std::vector<uint8_t> m_dna_buf;
    uint64_t m_num_dna;
};
