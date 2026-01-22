#pragma once

#include <d2d1_3.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <unordered_map>
#include <vector>

class ColorLUT {
public:
    void setup(ID3D11Texture2D *texture);

    void create_texture2d(ID3D11Texture2D **texture) const;
    void create_bitmap(ID3D11Texture2D *texture, D2D1_BITMAP_OPTIONS options, ID2D1Bitmap1 **bmp) const;
    [[nodiscard]] bool create_effect(const std::wstring &path, float mix, ID2D1Bitmap1 *bmp, ID2D1Effect **effect);

    void draw(ID2D1Image *target, ID2D1Effect *effect) const;
    void copy(ID3D11Resource *dst, ID3D11Resource *src) const noexcept;

    void reload() noexcept;
    void reload(const std::wstring &path) noexcept;

private:
    template <class T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    struct RGBA {
        float r, g, b, a;
    };

    struct LUT {
        int dimension;
        ComPtr<ID2D1Effect> _1d;
        ComPtr<ID2D1Effect> _3d;
    };

    ComPtr<ID3D11Device> d3d_device;
    ComPtr<ID2D1Device2> d2d_device;
    ComPtr<ID2D1DeviceContext2> d2d_context;
    ComPtr<ID2D1Effect> cross_fade;

    D3D11_TEXTURE2D_DESC desc{};
    std::unordered_map<std::wstring, LUT> cache{};

    [[nodiscard]] bool load(const std::wstring &path, LUT &lut);
};

struct CubeLUT {
    struct RGB {
        float r, g, b;

        [[nodiscard]] constexpr RGB operator+(const RGB &v) const noexcept { return {r + v.r, g + v.g, b + v.b}; }
        [[nodiscard]] constexpr RGB operator-(const RGB &v) const noexcept { return {r - v.r, g - v.g, b - v.b}; }
        [[nodiscard]] constexpr RGB operator*(const RGB &v) const noexcept { return {r * v.r, g * v.g, b * v.b}; }
        [[nodiscard]] constexpr RGB operator/(const RGB &v) const noexcept { return {r / v.r, g / v.g, b / v.b}; }
    };

    int dimension;

    RGB domain_min{0.0f, 0.0f, 0.0f};
    RGB domain_max{1.0f, 1.0f, 1.0f};
    RGB scale{1.0f, 1.0f, 1.0f};

    UINT32 size = 0u;
    UINT32 capacity = 0u;
    std::vector<RGB> data{};

    [[nodiscard]] bool load(const std::wstring &path) noexcept;
};
