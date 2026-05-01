#include "pixel.hpp"

#include <immintrin.h>
#include <execution>
#include <ranges>

namespace lut::pixel {
void
to_rgb16(RGB16 *dst, const RGBF32 *src, size_t w, size_t h) {
    constexpr float scale = 65535.0f;
    const __m256 v_scale = _mm256_set1_ps(scale);

    const auto idx = std::views::iota(0uz, h);

    std::for_each(std::execution::par, idx.begin(), idx.end(), [&](size_t y) {
        const float *in = reinterpret_cast<const float *>(src + y * w);
        uint16_t *out = reinterpret_cast<uint16_t *>(dst + y * w);

        const size_t len = w * 3;
        size_t x = 0uz;
        while (x + 8uz <= len) {
            const __m256 f32 = _mm256_loadu_ps(in + x);
            const __m256i i32 = _mm256_cvttps_epi32(_mm256_mul_ps(f32, v_scale));
            const __m128i u16 = _mm_packus_epi32(
                    _mm256_castsi256_si128(i32), _mm256_extractf128_si256(i32, 1));

            _mm_storeu_si128(reinterpret_cast<__m128i *>(out + x), u16);
            x += 8uz;
        }

        while (x < len) {
            const float v = in[x] * scale;
            out[x] = static_cast<uint16_t>(std::clamp(v, 0.0f, 65535.0f));
            ++x;
        }
    });
}

void
to_rgbaf32(RGBAF32 *dst, const RGBAF16 *src, size_t w, size_t h) {
    const auto idx = std::views::iota(0uz, h);

    std::for_each(std::execution::par, idx.begin(), idx.end(), [&](size_t y) {
        const RGBAF16 *in = src + y * w;
        RGBAF32 *out = dst + y * w;

        size_t x = 0uz;
        while (x + 2uz <= w) {
            const __m128i f16 =
                    _mm_loadu_si128(reinterpret_cast<const __m128i *>(in + x));
            const __m256 f32 = _mm256_cvtph_ps(f16);
            _mm256_storeu_ps(reinterpret_cast<float *>(out + x), f32);
            x += 2uz;
        }

        while (x < w) {
            const __m128i f16 =
                    _mm_loadl_epi64(reinterpret_cast<const __m128i *>(&in[x]));
            const __m128 f32 = _mm_cvtph_ps(f16);
            _mm_storeu_ps(reinterpret_cast<float *>(&out[x]), f32);
            ++x;
        }
    });
}

void
to_rgbf32(RGBF32 *dst, const RGB16 *src, size_t w, size_t h) {
    constexpr float step = 1.0f / 65535.0f;
    const __m256 v_step = _mm256_set1_ps(step);

    const auto idx = std::views::iota(0uz, h);

    std::for_each(std::execution::par, idx.begin(), idx.end(), [&](size_t y) {
        const uint16_t *in = reinterpret_cast<const uint16_t *>(src + y * w);
        float *out = reinterpret_cast<float *>(dst + y * w);

        const size_t len = w * 3;
        size_t x = 0uz;
        while (x + 8uz <= len) {
            const __m128i u16 =
                    _mm_loadu_si128(reinterpret_cast<const __m128i *>(in + x));
            const __m256 f32 = _mm256_mul_ps(
                    _mm256_cvtepi32_ps(_mm256_cvtepu16_epi32(u16)), v_step);
            _mm256_storeu_ps(out + x, f32);
            x += 8uz;
        }

        while (x < len) {
            out[x] = static_cast<float>(in[x]) * step;
            ++x;
        }
    });
}
}  // namespace lut::pixel
