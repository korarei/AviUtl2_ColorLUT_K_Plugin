#pragma once

#include <cstdint>

namespace lut::pixel {
struct RGB16 {
    uint16_t r, g, b;
};

struct RGBA16 {
    uint16_t r, g, b, a;
};

struct RGBAF16 : RGBA16 {};

struct RGBF32 {
    float r, g, b;
};

struct RGBAF32 {
    float r, g, b, a;
};

void
to_rgb16(RGB16 *dst, const RGBF32 *src, size_t w, size_t h);

void
to_rgbaf32(RGBAF32 *dst, const RGBAF16 *src, size_t w, size_t h);

void
to_rgbf32(RGBF32 *dst, const RGB16 *src, size_t w, size_t h);
}  // namespace lut::pixel
