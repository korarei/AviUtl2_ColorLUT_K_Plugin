#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <intern/cache.hpp>
#include <intern/pixel/pixel.hpp>

namespace lut {
struct LUTData {
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    int dimension = 0;
    ComPtr<ID3D11ShaderResourceView> srv;
    ComPtr<ID3D11Resource> data;
};

class LUTCache : private cache::Cache<std::optional<LUTData>> {
  public:
    [[nodiscard]] static LockedEntry Find(int64_t id, const std::wstring& path);
    static void Reset();
    static void Reset(int64_t id);
    static void Reset(const std::wstring& path);

  private:
    constexpr LUTCache() = default;
    constexpr ~LUTCache() = default;

    [[nodiscard]] static LUTCache& Instance();
};

namespace cube {
inline constexpr std::wstring_view kExtension = L".cube";

struct LUT {
    int dimension = 0;
    uint32_t size = 0u;
    std::vector<Eigen::half> data{};
};

[[nodiscard]] std::optional<LUT> Load(const std::filesystem::path& path);
}  // namespace cube

namespace hald {
inline constexpr std::array<std::wstring_view, 4> kExtensions = {L".png", L".bmp", L".tiff", L".tif"};

struct LUT {
    uint32_t level = 0u;
    uint32_t size = 0u;
    std::vector<pixel::RGBAF16> data{};
};

[[nodiscard]] std::optional<LUT> Load(const std::filesystem::path& path);
[[nodiscard]] bool Export(const LUT& lut, const std::filesystem::path& path, const std::wstring& title);
}  // namespace hald
}  // namespace lut
