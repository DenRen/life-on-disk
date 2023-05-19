#pragma once

#include <cstdint>
#include <bit>

template <typename T>
T misalign_load(const u8* data) noexcept {
    union {
        uint8_t arr[sizeof(T)];
        T val;
    } value;

    for (std::size_t i = 0; i < sizeof(T); ++i) {
        value.arr[i] = data[i];
    }

    return value.val;
}

template <typename T>
void misalign_store(u8* dest, T value) noexcept {
    u8* value_arr = (u8*)&value;

    for (std::size_t i = 0; i < sizeof(T); ++i) {
        dest[i] = value_arr[i];
    }
}

template <typename T, typename U>
constexpr auto DivUp(T value, U dvider) noexcept {
    return value / dvider + !!(value % dvider);
}

template <typename U>
constexpr U Log2Up(U value) noexcept {
    return 8 * sizeof(value) - std::countl_zero(value);
}
