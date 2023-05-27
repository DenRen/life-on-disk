#include <gtest/gtest.h>

#include <array>
#include <random>
#include <vector>

#include "../dna.h"

std::array<DnaSymb, 6> g_dna_symbs{DnaSymb::TERM, DnaSymb::A, DnaSymb::C, DnaSymb::T, DnaSymb::G};

TEST(DNA_BIT_OPS, READ) {
    for (uint shift = 0; shift <= 5; ++shift) {
        for (auto symb : g_dna_symbs) {
            uint8_t byte = (uint8_t)symb << (8 - 3);
            ASSERT_EQ(ReadDnaSymb(&byte, 0), symb) << symb << ", shift: " << shift;
        }
    }

    {
        DnaSymb dna_symbs[]{DnaSymb::TERM, DnaSymb::A, DnaSymb::C, DnaSymb::T,
                            DnaSymb::G,    DnaSymb::G, DnaSymb::A};

        std::array<uint8_t, 20> buf;
        uint64_t i = 0;
        for (auto dna_symb : dna_symbs) {
            InsertDnaSymb(buf.data(), i, dna_symb);
            ASSERT_EQ(ReadDnaSymb(buf.data(), i), dna_symb);
            ++i;
        }

        i = 0;
        for (auto dna_symb : dna_symbs) {
            ASSERT_EQ(ReadDnaSymb(buf.data(), i), dna_symb);
            ++i;
        }
    }

    {
        const unsigned seed = 0xEDA;

        std::array<uint8_t, 1000> buf;

        uint64_t pos_min = 0;
        uint64_t pos_max = 8 * sizeof(buf) / 3 - 1;

        std::mt19937_64 gen{seed};
        std::uniform_int_distribution<uint64_t> pos_distrib{pos_min, pos_max};
        std::uniform_int_distribution<uint8_t> dna_symb_distrib{0, 5};

        const unsigned num_repeats = 10000;
        std::map<uint64_t, DnaSymb> ops;

        for (unsigned i_repeat = 0; i_repeat < num_repeats; ++i_repeat) {
            for (unsigned i = 0; i < buf.size(); ++i) {
                uint64_t dna_pos = pos_distrib(gen);
                DnaSymb dna_symb{dna_symb_distrib(gen)};

                ops[dna_pos] = dna_symb;
                InsertDnaSymb(buf.data(), dna_pos, dna_symb);
            }

            for (const auto [dna_pos, dna_symb] : ops) {
                ASSERT_EQ(ReadDnaSymb(buf.data(), dna_pos), dna_symb);
            }

            ops.clear();
        }
    }
}

TEST(DnaSeqSymb, Naive) {
    DnaSymbSeq<4> seq;
    seq.Set(DnaSymb::A, 0);
    seq.Set(DnaSymb::C, 1);
    seq.Set(DnaSymb::T, 2);
    seq.Set(DnaSymb::G, 3);

    ASSERT_EQ(seq[0], DnaSymb::A);
    ASSERT_EQ(seq[1], DnaSymb::C);
    ASSERT_EQ(seq[2], DnaSymb::T);
    ASSERT_EQ(seq[3], DnaSymb::G);

    DnaSymbSeq<4> seq2;
    seq2.Set(DnaSymb::A, 0);
    seq2.Set(DnaSymb::C, 1);
    seq2.Set(DnaSymb::G, 2);
    seq2.Set(DnaSymb::G, 3);

    ASSERT_LT(seq, seq2);
    ASSERT_GT(seq2, seq);

    seq.Set(DnaSymb::C, 0);

    ASSERT_GT(seq, seq2);
    ASSERT_LT(seq2, seq);
}