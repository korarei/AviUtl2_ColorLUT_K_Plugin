#pragma once

#include <cstdint>

#include <Eigen/Core>

namespace lut::pixel {
using RGBA16 = Eigen::Vector4<uint16_t>;
using RGBAF16 = Eigen::Vector4<Eigen::half>;
}  // namespace lut::pixel
