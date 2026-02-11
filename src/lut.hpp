#pragma once

#include <cstdint>
#include <filesystem>
#include <future>
#include <string>
#include <unordered_map>
#include <vector>

#include <d2d1_3.h>
#include <d3d11.h>
#include <wrl/client.h>

#include "pixel.hpp"

struct CubeLUT {
    int dimension = 0;

    RGBF32 domain_min{};
    RGBF32 domain_max{};
    RGBF32 scale{};

    uint32_t size = 0u;
    uint32_t capacity = 0u;
    std::vector<RGBF32> data{};

    [[nodiscard]] bool load(const std::filesystem::path &path) noexcept;
};

struct HaldLUT {
    uint32_t level = 0u;
    uint32_t w = 0u, h = 0u;
    std::vector<RGBAF32> data{};

    [[nodiscard]] bool load(const std::filesystem::path &path);
    [[nodiscard]] bool save(const std::filesystem::path &path, const std::u8string &title) const;
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

class Hald2Cube {
public:
    [[nodiscard]] bool generate_identity(ID3D11Texture2D *texture);
    [[nodiscard]] bool load_hald(ID3D11Texture2D *texture);
    void convert(const std::u8string &title, void (*callback)(bool success));
    void set_owner(HWND hwnd) noexcept { owner = hwnd; };

private:
    template <class T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D11_TEXTURE2D_DESC desc{};
    HaldLUT lut{};
    std::future<void> save_task;
    HWND owner = nullptr;

    void setup(ID3D11Texture2D *texture);
};
