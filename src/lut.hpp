#pragma once

#include <d2d1_3.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>
#include <filesystem>
#include <unordered_map>
#include <vector>

struct RGB {
    float r, g, b;

    [[nodiscard]] constexpr RGB operator+(const RGB &v) const noexcept { return {r + v.r, g + v.g, b + v.b}; }
    [[nodiscard]] constexpr RGB operator-(const RGB &v) const noexcept { return {r - v.r, g - v.g, b - v.b}; }
    [[nodiscard]] constexpr RGB operator*(const RGB &v) const noexcept { return {r * v.r, g * v.g, b * v.b}; }
    [[nodiscard]] constexpr RGB operator/(const RGB &v) const noexcept { return {r / v.r, g / v.g, b / v.b}; }
};

struct RGBA {
    float r, g, b, a;
};

class ColorLUT {
public:
    void setup(ID3D11Texture2D *texture);

    void create_texture2d(ID3D11Texture2D **texture) const;
    void create_bitmap(ID3D11Texture2D *texture, D2D1_BITMAP_OPTIONS options, ID2D1Bitmap1 **bmp) const;
    [[nodiscard]] bool create_effect(const std::filesystem::path &path, float mix, ID2D1Bitmap1 *bmp, ID2D1Effect **fx);

    void draw(ID2D1Image *target, ID2D1Effect *fx) const;
    void copy(ID3D11Resource *dst, ID3D11Resource *src) const noexcept;

    void reload() noexcept;
    void reload(const std::filesystem::path &path) noexcept;

private:
    template <class T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    struct {
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
    } d3d;

    struct {
        ComPtr<ID2D1Device2> device;
        ComPtr<ID2D1DeviceContext2> context;
    } d2d;

    ComPtr<ID2D1Effect> cross_fade;
    D3D11_TEXTURE2D_DESC desc{};
    std::unordered_map<std::filesystem::path, ComPtr<ID2D1Effect>> cache{};

    [[nodiscard]] bool load(const std::filesystem::path &path, ID2D1Effect **lut);
};

struct CubeLUT {
    int dimension;

    RGB domain_min;
    RGB domain_max;
    RGB scale;

    uint32_t size;
    uint32_t capacity;
    std::vector<RGB> data;

    [[nodiscard]] bool load(const std::filesystem::path &path) noexcept;
};
