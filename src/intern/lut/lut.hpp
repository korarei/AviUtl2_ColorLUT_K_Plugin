#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <intern/cache.hpp>
#include <intern/pixel/pixel.hpp>

namespace lut {
inline constexpr struct {
    std::wstring_view cube = L".cube";
    std::array<std::wstring_view, 4> texture = {L".png", L".bmp", L".tiff", L".tif"};
} kExtension{};

struct LUTData {
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    int dimension = 0;
    ComPtr<ID3D11ShaderResourceView> view = nullptr;
    ComPtr<ID3D11Resource> data = nullptr;
};

class LUTCache : private cache::Cache<std::optional<LUTData>> {
  public:
    [[nodiscard]] static LockedEntry Find(int64_t id, const std::wstring& path);
    static void Reset();
    static void Reset(int64_t id);
    static void Reset(const std::wstring& path);

  private:
    LUTCache() = default;
    ~LUTCache() = default;

    [[nodiscard]] static LUTCache& Instance();
};

struct LUTView {
    using LUT1D = std::span<const pixel::Float16>;
    using LUT3D = std::span<const pixel::RGBAF16>;

    uint32_t size;
    std::variant<LUT1D, LUT3D> data;
};

class TexLUT {
  public:
    TexLUT(const TexLUT&) = delete;
    TexLUT& operator=(const TexLUT&) = delete;
    TexLUT(TexLUT&&) = default;
    TexLUT& operator=(TexLUT&&) = default;

    TexLUT() = default;
    ~TexLUT() = default;

    [[nodiscard]] static std::optional<TexLUT> Load(const std::filesystem::path& path);

    [[nodiscard]] uint32_t w() const { return w_; }
    [[nodiscard]] uint32_t h() const { return h_; }
    [[nodiscard]] std::vector<pixel::RGBAF16>& data() { return data_; }

    [[nodiscard]] bool Save(const std::filesystem::path& path, const std::wstring& title) const;

  protected:
    uint32_t w_ = 0u, h_ = 0u;
    std::vector<pixel::RGBAF16> data_{};

    TexLUT(uint32_t w, uint32_t h, const std::vector<pixel::RGBAF16>& data);
};

class HaldLUT;
class StripLUT;

class CubeLUT {
  public:
    CubeLUT(const CubeLUT&) = delete;
    CubeLUT& operator=(const CubeLUT&) = delete;
    CubeLUT(CubeLUT&&) = default;
    CubeLUT& operator=(CubeLUT&&) = default;

    ~CubeLUT() = default;

    [[nodiscard]] static std::optional<CubeLUT> Init(int dimension, uint32_t size, std::vector<pixel::Float16>&& data);
    [[nodiscard]] static CubeLUT Init(HaldLUT&& lut);
    [[nodiscard]] static CubeLUT Init(StripLUT&& lut);
    [[nodiscard]] static std::optional<CubeLUT> Import(const std::filesystem::path& path);

    [[nodiscard]] LUTView View() const;
    [[nodiscard]] bool Export(const std::filesystem::path& path, const std::wstring& title) const;

  private:
    CubeLUT(int dimension, uint32_t size, std::vector<pixel::Float16>&& data);

    int dimension_ = 0;
    uint32_t size_ = 0u;
    std::vector<pixel::Float16> data_{};
};

class HaldLUT : public TexLUT {
  public:
    HaldLUT(const HaldLUT&) = delete;
    HaldLUT& operator=(const HaldLUT&) = delete;
    HaldLUT(HaldLUT&&) = default;
    HaldLUT& operator=(HaldLUT&&) = default;

    ~HaldLUT() = default;

    [[nodiscard]] static std::optional<HaldLUT> Init(uint32_t level, std::vector<pixel::RGBAF16>&& data);
    [[nodiscard]] static std::optional<HaldLUT> Init(StripLUT&& lut);
    [[nodiscard]] static std::optional<HaldLUT> Import(const std::filesystem::path& path);

    [[nodiscard]] LUTView View() const;
    [[nodiscard]] bool Export(const std::filesystem::path& path, const std::wstring& title) const;

  private:
    friend class CubeLUT;
    friend class StripLUT;

    HaldLUT(uint32_t size, uint32_t w, std::vector<pixel::RGBAF16>&& data);

    uint32_t size_ = 0u;
};

class StripLUT : public TexLUT {
  public:
    StripLUT(const StripLUT&) = delete;
    StripLUT& operator=(const StripLUT&) = delete;
    StripLUT(StripLUT&&) = default;
    StripLUT& operator=(StripLUT&&) = default;

    ~StripLUT() = default;

    [[nodiscard]] static std::optional<StripLUT> Init(uint32_t size, std::vector<pixel::RGBAF16>&& data);
    [[nodiscard]] static StripLUT Init(HaldLUT&& lut);
    [[nodiscard]] static std::optional<StripLUT> Import(const std::filesystem::path& path);

    [[nodiscard]] bool Export(const std::filesystem::path& path, const std::wstring& title) const;

  private:
    friend class CubeLUT;
    friend class HaldLUT;

    StripLUT(uint32_t size, uint32_t w, uint32_t h, std::vector<pixel::RGBAF16>&& data);

    uint32_t size_ = 0u;
};
}  // namespace lut
