#include "pixel.hpp"

#include <immintrin.h>
#include <execution>
#include <ranges>

namespace lut::pixel {
void
to_rgb16(RGB16 *dst, const RGBAF32 *src, size_t w, size_t h) {
    alignas(32) constexpr uint8_t mask[32] = {
            0,  1,  2,  3,  4,  5,  8,  9,  10,  11,  12,  13,  16,  17,  18,  19,
            20, 21, 24, 25, 26, 27, 28, 29, 255, 255, 255, 255, 255, 255, 255, 255,
    };

    const auto idx = std::views::iota(0uz, h);

    const __m256 v_min = _mm256_setzero_ps();
    const __m256 v_max = _mm256_set1_ps(1.0f);
    const __m256 v_scale = _mm256_set1_ps(65535.0f);
    const __m256i v_mask = _mm256_load_si256(reinterpret_cast<const __m256i *>(mask));

    std::for_each(std::execution::par, idx.begin(), idx.end(), [&](size_t y) {
        const RGBAF32 *in = src + y * w;
        RGB16 *out = dst + y * w;

        size_t x = 0uz;
        while (x + 4uz <= w) {
            __m256 f0 = _mm256_loadu_ps(reinterpret_cast<const float *>(in + x));
            __m256 f1 = _mm256_loadu_ps(reinterpret_cast<const float *>(in + x + 2));

            f0 = _mm256_mul_ps(_mm256_min_ps(_mm256_max_ps(f0, v_min), v_max), v_scale);
            f1 = _mm256_mul_ps(_mm256_min_ps(_mm256_max_ps(f1, v_min), v_max), v_scale);

            __m256i i0 = _mm256_cvttps_epi32(f0);
            __m256i i1 = _mm256_cvttps_epi32(f1);

            __m256i packed = _mm256_packus_epi32(i0, i1);
            packed = _mm256_permute4x64_epi64(packed, _MM_SHUFFLE(3, 1, 2, 0));

            __m256i rgb = _mm256_shuffle_epi8(packed, v_mask);

            _mm_storeu_si128(reinterpret_cast<__m128i *>(out + x), _mm256_castsi256_si128(rgb));
            _mm_storel_epi64(reinterpret_cast<__m128i *>(reinterpret_cast<uint8_t *>(out + x) + 16),
                             _mm256_extracti128_si256(rgb, 1));
            x += 4uz;
        }

        while (x < w) {
            auto saturate = [](float v) { return std::clamp(v, 0.0f, 1.0f); };
            out[x].r = static_cast<uint16_t>(saturate(in[x].r) * 65535.0f);
            out[x].g = static_cast<uint16_t>(saturate(in[x].g) * 65535.0f);
            out[x].b = static_cast<uint16_t>(saturate(in[x].b) * 65535.0f);
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

    const __m256 v_step = _mm256_set1_ps(step);

    std::for_each(std::execution::par, idx.begin(), idx.end(), [&](size_t y) {
        const RGBA16 *in = src + y * w;
        RGBAF32 *out = dst + y * w;

        size_t x = 0uz;
        while (x + 2uz <= w) {
            const __m128i u16 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(in + x));
            const __m256 f32 = _mm256_mul_ps(_mm256_cvtepi32_ps(_mm256_cvtepu16_epi32(u16)), v_step);
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
