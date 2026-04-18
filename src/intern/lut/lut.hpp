#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "pixel.hpp"

namespace lut {
struct CubeLUT {
    int dimension = 0;
    uint32_t size = 0u;
    std::vector<float> data{};

    [[nodiscard]] bool load(const std::filesystem::path &path);
};

struct HaldCLUT {
    uint32_t level = 0u;
    std::vector<pixel::RGBAF32> data{};

    [[nodiscard]] bool load(const std::filesystem::path &path);
    [[nodiscard]] bool export_cube(const std::filesystem::path &path, const std::u8string &title) const;
    [[nodiscard]] bool export_png(const std::filesystem::path &path, const std::u8string &title) const;
};
}  // namespace lut
