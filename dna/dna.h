#pragma once

#include <cstdint>
#include <utility>
#include <iostream>

#include "../common/file_manip.h"
#include "types.h"

inline bool operator<(DnaSymb lhs, DnaSymb rhs) noexcept {
    return (u8)lhs < (u8)rhs;
}

std::pair<DnaSymb, bool> ConvertTextDnaSymb2DnaSymb(uint8_t symb) noexcept;

const char* DNASymb2String(DnaSymb dna_symb);

template <u8 N>
std::string DNASymb2String(DnaSymbSeq<N> dna_symb) {
    std::string str;
    for (u8 i = 0; i < N; ++i) {
        str += DNASymb2String(dna_symb[i]);
    }
    return str;
}

std::ostream& operator<<(std::ostream& os, const DnaSymb& dna_symb);

template <u8 N>
std::ostream& operator<<(std::ostream& os, const DnaSymbSeq<N>& dna_seq) {
    for (u8 i = 0; i < N; ++i) {
        os << dna_seq[i];
    }
    return os;
}

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
                                                 std::string_view suff_arr_path, uint d = 1);

// Not garantee, that [Size() - 1] == TERM
class DnaDataAccessor {
public:
    DnaDataAccessor(const u8* dna_data_begin, uint64_t num_dna);
    DnaDataAccessor(const ObjectFileHolder& dna_file_holder);

    DnaSymb operator[](uint64_t index) const noexcept;
    const u8* data() const noexcept {
        return m_dna_data_begin;
    }
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

// Not garantee, that [Size() - 1] == TERM
template <u8 N>
class DnaSeqDataAccessor {
public:
    DnaSeqDataAccessor(const u8* dna_data_begin, uint64_t num_dna_seq)
        : m_dna_data_begin{dna_data_begin}
        , m_num_dna{num_dna_seq} {}
    DnaSeqDataAccessor(const ObjectFileHolder& dna_file_holder)
        : DnaSeqDataAccessor{dna_file_holder.cbegin(), dna_file_holder.Size() / N} {}

    DnaSymbSeq<N> operator[](uint64_t index) const noexcept {
        DnaSymbSeq<N> seq;
        for (u8 i = 0; i < N; ++i) {
            seq.Set(ReadDnaSymb(m_dna_data_begin, index * N + i), i);
        }

        return seq;
    }
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

template <u8 d>
std::size_t DnaSeq2Number(const DnaSymbSeq<d>& dna_seq) noexcept {
    std::size_t value = 0;
    for (u8 i = 0; i < d; ++i) {
        value = 8 * value + (u8)dna_seq[i];
    }
    return value;
}

template <u8 d>
std::size_t DnaSeq2RevNumber(const DnaSymbSeq<d>& dna_seq) noexcept {
    std::size_t value = 0;
    for (u8 i = 0; i < d; ++i) {
        value = 8 * value + (u8)dna_seq[d - 1 - i];
    }
    return value;
}

template <u8 d>
class ReverseBWTDnaSeqAccessor {
public:
    ReverseBWTDnaSeqAccessor(const DnaSeqDataAccessor<d>& dna, const str_pos_t* suff_arr)
        : m_dna{dna}
        , m_suff_arr{suff_arr} {}

    std::size_t operator[](str_pos_t pos) const noexcept {
        // BWT
        pos = m_suff_arr[pos];
        pos = pos ? pos - 1 : m_dna.Size() - 1;

        // Convert to reverse number
        return DnaSeq2RevNumber(m_dna[pos]);
        // return DnaSeq2Number(m_dna[pos]);
    }

    str_len_t Size() const noexcept {
        return m_dna.Size();
    }

private:
    DnaSeqDataAccessor<d> m_dna;
    const str_pos_t* m_suff_arr;
};
