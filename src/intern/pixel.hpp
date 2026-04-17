#pragma once

#include <immintrin.h>
#include <cstdint>
#include <execution>
#include <ranges>

namespace lut::pixel {
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

inline void
to_rgbaf32(RGBAF32 *dst, const RGBAF16 *src, size_t w, size_t h) {
    const auto idx = std::views::iota(0uz, h);

    std::for_each(std::execution::par, idx.begin(), idx.end(), [=](size_t y) {
        const RGBAF16 *src_row = src + y * w;
        RGBAF32 *dst_row = dst + y * w;

        size_t x = 0uz;
        while (x + 2uz <= w) {
            __m128i h_vec = _mm_loadu_si128(reinterpret_cast<const __m128i *>(src_row + x));
            __m256 f_vec = _mm256_cvtph_ps(h_vec);
            _mm256_storeu_ps(reinterpret_cast<float *>(dst_row + x), f_vec);
            x += 2uz;
        }

        while (x < w) {
            __m128i h_val = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(&src_row[x]));
            __m128 f_val = _mm_cvtph_ps(h_val);
            _mm_storeu_ps(reinterpret_cast<float *>(&dst_row[x]), f_val);
            ++x;
        }
    });
}
}  // namespace lut::pixel
