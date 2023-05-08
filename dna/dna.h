#pragma once

#include <cstdint>
#include <utility>
#include <iostream>

#include "../src/file_mapper.h"

enum class DnaSymb : uint8_t { TERM = 0, A, C, T, G, N };
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
                                               std::string_view compressed_dna_path);

ObjectFileHolder BuildSuffArrayFromCompressedDna(std::string_view compressed_dna_path,
                                                 std::string_view suff_arr_path);

class DnaDataAccessor {
public:
    DnaDataAccessor(const u8* dna_data_begin, uint64_t num_dna) noexcept;

    DnaSymb operator[](uint64_t index) const noexcept;
    uint64_t Size() const noexcept {
        return m_num_dna;
    }

private:
    const u8* m_dna_data_begin;
    uint64_t m_num_dna;
};