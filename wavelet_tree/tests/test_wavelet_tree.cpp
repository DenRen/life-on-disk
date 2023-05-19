#include <gtest/gtest.h>
// #include <random>

#include "wavelet_tree.hpp"

class TestText : public std::vector<unsigned> {
public:
    std::size_t Size() const noexcept {
        return size();
    }
};

TEST(WAVELET_TREE, MANUAL) {
    TestText text{{1, 2, 4, 4, 6, 3, 3, 2}};

    auto build_info = WaveletTree::PrepareBuild(text, 10);
    std::vector<u8> mapped_buf(build_info.CalcOccupiedSize());
    new (mapped_buf.data()) WaveletTree{text, 10, build_info};
}