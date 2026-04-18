#include "pixel.hpp"

#include <immintrin.h>
#include <execution>
#include <ranges>

namespace lut::pixel {
void
to_rgbaf32(RGBAF32 *dst, const RGBAF16 *src, size_t w, size_t h) {
    const auto idx = std::views::iota(0uz, h);

    std::for_each(std::execution::par, idx.begin(), idx.end(), [&](size_t y) {
        const RGBAF16 *in = src + y * w;
        RGBAF32 *out = dst + y * w;

        size_t x = 0uz;
        while (x + 2uz <= w) {
            const __m128i f16 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(in + x));
            const __m256 f32 = _mm256_cvtph_ps(f16);
            _mm256_storeu_ps(reinterpret_cast<float *>(out + x), f32);
            x += 2uz;
        }

        while (x < w) {
            const __m128i f16 = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(&in[x]));
            const __m128 f32 = _mm_cvtph_ps(f16);
            _mm_storeu_ps(reinterpret_cast<float *>(&out[x]), f32);
            ++x;
        }
    });
}

void
to_rgbaf32(RGBAF32 *dst, const RGBA16 *src, size_t w, size_t h) {
    constexpr float step = 1.0f / 65535.0f;

    const auto idx = std::views::iota(0uz, h);

    std::for_each(std::execution::par, idx.begin(), idx.end(), [&](size_t y) {
        const RGBA16 *in = src + y * w;
        RGBAF32 *out = dst + y * w;

        size_t x = 0uz;
        while (x + 2uz <= w) {
            const __m128i u16 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(in + x));
            const __m256 f32 = _mm256_mul_ps(_mm256_cvtepi32_ps(_mm256_cvtepu16_epi32(u16)), _mm256_set1_ps(step));
            _mm256_storeu_ps(reinterpret_cast<float *>(out + x), f32);
            x += 2uz;
        }

        while (x < w) {
            out[x].r = static_cast<float>(in[x].r) * step;
            out[x].g = static_cast<float>(in[x].g) * step;
            out[x].b = static_cast<float>(in[x].b) * step;
            out[x].a = static_cast<float>(in[x].a) * step;
            ++x;
        }
    });
}
}  // namespace lut::pixel
