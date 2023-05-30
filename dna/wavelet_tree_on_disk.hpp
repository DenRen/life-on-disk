#pragma once

#include "../wavelet_tree/wavelet_tree.hpp"
#include "../common/file_manip.h"

#include <string>

class WaveletTreeOnDisk {
public:
    WaveletTreeOnDisk(std::string wt_path)
        : m_wt{wt_path} {}

    const WaveletTree& Get() const noexcept {
        return *(const WaveletTree*)m_wt.begin();
    }

    template <typename NumberAccessorT>
    static WaveletTreeOnDisk Build(std::string wt_path, const NumberAccessorT& text,
                                   WaveletTree::size_t alph_size);

private:
    FileMapperRead m_wt;
};

template <typename NumberAccessorT>
WaveletTreeOnDisk WaveletTreeOnDisk::Build(std::string wt_path, const NumberAccessorT& text,
                                           WaveletTree::size_t alph_size) {
    {
        auto build_info = WaveletTree::PrepareBuild(text, alph_size);
        FileMapperWrite file{wt_path, build_info.CalcOccupiedSize()};
        new (file.begin()) WaveletTree{text, alph_size, build_info};
    }

    return {wt_path};
}