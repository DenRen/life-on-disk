#pragma once

#include <cstdint>
#include <utility>
#include <iostream>

#include "../src/file_mapper.h"

enum class DnaSymb : uint8_t { TERM = 0, A, C, T, G, N };

std::pair<DnaSymb, bool> ConvertTextDnaSymb2DnaSymb(uint8_t symb) noexcept;

void InsertDnaSymb(uint8_t* begin, uint64_t dna_pos, DnaSymb dna_symb);
DnaSymb ReadDnaSymb(const uint8_t* begin, uint64_t dna_pos);

std::ostream& operator<<(std::ostream& os, const DnaSymb& dna_symb);

class DNAFileHolder {
    constexpr static std::size_t c_data_shift = sizeof(uint64_t);

public:
    DNAFileHolder(std::string_view compressed_dna_path);
    static DNAFileHolder BuildFromTextDNA(std::string_view text_dna_path,
                                          std::string_view compressed_dna_path);

    const uint8_t* cbegin() const noexcept {
        return m_file_map.begin() + c_data_shift;
    }

    uint64_t Size() const noexcept {
        return m_size;
    }

private:
    FileMapperRead m_file_map;
    uint64_t m_size;
};
