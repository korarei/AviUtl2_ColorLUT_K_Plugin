#pragma once

#include <cstdint>

namespace lut::pixel {
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
to_rgbaf32(RGBAF32 *dst, const RGBAF16 *src, size_t w, size_t h);

void
to_rgbaf32(RGBAF32 *dst, const RGBA16 *src, size_t w, size_t h);
}  // namespace lut::pixel
