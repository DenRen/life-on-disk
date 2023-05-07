#pragma once

#include <cstdint>
#include <utility>
#include <iostream>

enum class DnaSymb : uint8_t { TERM = 0, A, C, T, G, N };

std::pair<DnaSymb, bool> ConvertTextDnaSymb2DnaSymb(uint8_t symb) noexcept;

void InsertDnaSymb(uint8_t* begin, uint64_t dna_pos, DnaSymb dna_symb);
DnaSymb ReadDnaSymb(const uint8_t* begin, uint64_t dna_pos);

std::ostream& operator<<(std::ostream& os, const DnaSymb& dna_symb);