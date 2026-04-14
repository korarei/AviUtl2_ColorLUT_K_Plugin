#pragma once

#include <filesystem>
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

struct HaldCLUT {
    uint32_t level = 0u;
    uint32_t w = 0u, h = 0u;
    std::vector<RGBAF32> data{};

    [[nodiscard]] bool load(const std::filesystem::path &path);
    [[nodiscard]] bool save(const std::filesystem::path &path, const std::u8string &title) const;
};

// setupが呼ばれない場合にそれ以外の描画系メソッドを使用するとやばいので今後どうにかしたい
class ColorLUT {
public:
    [[nodiscard]] static bool load(ID2D1Effect **lut, ID2D1DeviceContext2 *ctx, const std::filesystem::path &path);

    void setup(ID3D11Texture2D *tex);

    void create_texture(ID3D11Texture2D **tex) const;
    void wrap_texture(ID2D1Bitmap1 **bmp, ID3D11Texture2D *tex, D2D1_BITMAP_OPTIONS options) const;
    [[nodiscard]] bool configure(const std::filesystem::path &path, int mode, double opacity, bool clamp);
    void draw(ID2D1Image *target, ID2D1Image *input) const;
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
        ComPtr<ID2D1Factory3> factory;
        ComPtr<ID2D1Device2> device;
        ComPtr<ID2D1DeviceContext2> context;
    } d2d;

    ComPtr<ID2D1Effect> lut;
    ComPtr<ID2D1Effect> blend;
    D3D11_TEXTURE2D_DESC desc{};
    std::unordered_map<std::filesystem::path, ComPtr<ID2D1Effect>> cache{};
};

class Identity {
public:
    [[nodiscard]] bool draw(ID3D11Texture2D *tex);

private:
    template <class T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    struct alignas(16) Params {
        uint32_t level;
        uint32_t _padding[3];
    };

    struct {
        D3D11_TEXTURE2D_DESC texture = {};
        const D3D11_BUFFER_DESC buffer = {
                .ByteWidth = sizeof(Params),
                .Usage = D3D11_USAGE_DEFAULT,
                .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
                .CPUAccessFlags = 0u,
                .MiscFlags = 0u,
                .StructureByteStride = 0u,
        };
        const D3D11_UNORDERED_ACCESS_VIEW_DESC uav = {
                .Format = DXGI_FORMAT_R16G16B16A16_FLOAT,
                .ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D,
                .Texture2D = {.MipSlice = 0u},
        };
    } desc;

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11ComputeShader> identity;

    void setup(ID3D11Texture2D *tex);
};
