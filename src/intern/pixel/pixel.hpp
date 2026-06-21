#pragma once

#include <cstdint>

#include <Eigen/Core>

namespace lut::pixel {
using Float16 = Eigen::half;
using RGBA16 = Eigen::Vector4<uint16_t>;
using RGBAF16 = Eigen::Vector4<Float16>;

static_assert(sizeof(RGBA16) == 4uz * sizeof(uint16_t));
static_assert(sizeof(RGBAF16) == 4uz * sizeof(Float16));
static_assert(alignof(RGBA16) == alignof(uint16_t));
static_assert(alignof(RGBAF16) == alignof(Float16));
}  // namespace lut::pixel
