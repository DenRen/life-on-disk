#pragma once

#include <cstdint>

template <typename T>
T misalign_load(const uint8_t* data) noexcept {
    union {
        uint8_t arr[sizeof(T)];
        T val;
    } value;

    for (std::size_t i = 0; i < sizeof(T); ++i) {
        value.arr[i] = data[i];
    }

    return value.val;
}