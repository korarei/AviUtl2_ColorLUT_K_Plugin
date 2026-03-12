#pragma once

#include <cstdint>

struct RGBA16 {
    uint16_t r, g, b, a;
};

struct RGBAF16 : RGBA16 {};

struct RGBF32 {
    float r, g, b;

    [[nodiscard]] constexpr RGBF32 operator+(const RGBF32 &v) const noexcept { return {r + v.r, g + v.g, b + v.b}; }
    [[nodiscard]] constexpr RGBF32 operator-(const RGBF32 &v) const noexcept { return {r - v.r, g - v.g, b - v.b}; }
    [[nodiscard]] constexpr RGBF32 operator*(const RGBF32 &v) const noexcept { return {r * v.r, g * v.g, b * v.b}; }
    [[nodiscard]] constexpr RGBF32 operator/(const RGBF32 &v) const noexcept { return {r / v.r, g / v.g, b / v.b}; }
};

struct RGBAF32 {
    float r, g, b, a;

    constexpr RGBAF32() noexcept = default;
    constexpr RGBAF32(const RGBF32 &v) noexcept : r{v.r}, g{v.g}, b{v.b}, a{1.0f} {}
    constexpr RGBAF32(const RGBA16 &v) noexcept :
        r{static_cast<float>(v.r) / 65535.0f},
        g{static_cast<float>(v.g) / 65535.0f},
        b{static_cast<float>(v.b) / 65535.0f},
        a{static_cast<float>(v.a) / 65535.0f} {}
};
