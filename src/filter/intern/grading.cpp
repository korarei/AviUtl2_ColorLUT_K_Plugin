#include "grading.hpp"

#include <d3d11.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdint>
#include <cwctype>
#include <filesystem>

#pragma warning(push)
#pragma warning(disable : 4201)  // 非標準の無名構造体 (filter2.h FILTER_ITEM_COLOR)
#include <filter2.h>
#pragma warning(pop)

#include <intern/aviutl/aviutl.hpp>
#include <intern/direct3d/direct3d.hpp>
#include <intern/lut/lut.hpp>
#include <intern/string.hpp>

#include <lut1d.h>
#include <lut3d.h>

#ifndef VERSION
#define VERSION L"0.1.0"
#endif

namespace {
namespace string = lut::string;

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;
using Logger = lut::aviutl::Logger;
using LUTCache = lut::LUTCache;
using PixelShaderDesc = lut::direct3d::PixelShaderDesc;
using Direct3D = lut::direct3d::Direct3D<1uz, 2uz>;  // 1 cache, 2 pixel shaders
using RGBAF16 = Direct3D::RGBAF16;

struct alignas(16) Params {
    int32_t blend_mode;
    float opacity;
    float should_clamp;
    float seed;
};

enum PixelShaderIndex : size_t {
    kLUT1D = 0uz,
    kLUT3D = 1uz,
};

constexpr D3D11_SAMPLER_DESC kSampler{
    .Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
    .AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
    .AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
    .AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
    .MipLODBias = 0.0f,
    .MaxAnisotropy = 0u,
    .ComparisonFunc = D3D11_COMPARISON_NEVER,
    .BorderColor = {0.0f, 0.0f, 0.0f, 0.0f},
    .MinLOD = 0.0f,
    .MaxLOD = D3D11_FLOAT32_MAX,
};

Direct3D d3d({PixelShaderDesc{g_lut1d, sizeof(Params), kSampler}, PixelShaderDesc{g_lut3d, sizeof(Params), kSampler}});

namespace property {
FILTER_ITEM_FILE file(L"LUT File", L"",
                      L"Cube LUT File (*.cube)\0*.cube\0"
                      L"Hald CLUT File (*.bmp;*.png;*.tiff;*.tif)\0*.bmp;*.png;*.tiff;*.tif;\0\0");
FILTER_ITEM_BUTTON reload(L"Reload LUT", [](EDIT_SECTION* edit) {
    LUTCache::Reset(file.value);
    edit->set_cursor_layer_frame(edit->info->layer, edit->info->frame);
});

namespace compositing {
FILTER_ITEM_GROUP name(L"Compositing", false);
FILTER_ITEM_SELECT::ITEM blend_modes[] = {
    {L"Normal", 0},
    {L"Dissolve", 1},
    {L"Darken", 2},
    {L"Multiply", 3},
    {L"Color Burn", 4},
    {L"Linear Burn", 5},
    {L"Darker Color", 6},
    {L"Lighten", 7},
    {L"Screen", 8},
    {L"Color Dodge", 9},
    {L"Linear Dodge (Add)", 10},
    {L"Lighter Color", 11},
    {L"Overlay", 12},
    {L"Soft Light", 13},
    {L"Hard Light", 14},
    {L"Linear Light", 15},
    {L"Vivid Light", 16},
    {L"Pin Light", 17},
    {L"Hard Mix", 18},
    {L"Difference", 19},
    {L"Exclusion", 20},
    {L"Subtract", 21},
    {L"Divide", 22},
    {L"Hue", 23},
    {L"Saturation", 24},
    {L"Color", 25},
    {L"Luminosity", 26},
    {nullptr, -1},
};
FILTER_ITEM_SELECT blend_mode(L"Compositing::Blend Mode", 0, blend_modes);
FILTER_ITEM_TRACK opacity(L"Compositing::Opacity", 100.0, 0.0, 100.0, 0.01);
FILTER_ITEM_CHECK clamp(L"Compositing::Clamp", false);
}  // namespace compositing
}  // namespace property

void* props[] = {
    &property::file,
    &property::reload,
    &property::compositing::name,
    &property::compositing::blend_mode,
    &property::compositing::opacity,
    &property::compositing::clamp,
    nullptr,
};

auto& file = property::file.value;
auto& blend_mode = property::compositing::blend_mode.value;
auto& opacity = property::compositing::opacity.value;
auto& clamp = property::compositing::clamp.value;

bool Apply(FILTER_PROC_VIDEO* ctx) {
    if (file[0] == L'\0') {
        return true;
    }

    const int w = ctx->object->width, h = ctx->object->height;

    if (w < 1 || h < 1) {
        return false;
    }

    const Params params{
        .blend_mode = blend_mode,
        .opacity = static_cast<float>(opacity * 0.01),
        .should_clamp = clamp ? 1.0f : 0.0f,
        .seed = static_cast<float>(w * h),
    };

    try {
        const auto tex = ctx->get_image_texture2d();
        const auto ctrl = d3d.Init(tex, []() { LUTCache::Reset(); });

        const auto dst = ctrl.GetBackBuffer(tex);
        const auto src = ctrl.CopyBuffer<0uz>(tex, w, h);

        {
            const auto entry = LUTCache::Find(ctx->object->effect_id, file);
            auto& lut = *entry;

            if (!lut.has_value()) {
                const std::filesystem::path path(file);
                auto ext = path.extension().wstring();

                if (ext.empty()) {
                    Logger::Error(L"File extension not specified");
                    return false;
                }

                std::ranges::for_each(ext, [](wchar_t& c) { c = std::towlower(c); });

                if (ext == lut::cube::kExtension) {
                    const auto cube = lut::cube::Load(path);

                    if (!cube.has_value()) {
                        Logger::Error(L"Failed to load Cube LUT");
                        return false;
                    }

                    switch (cube->dimension) {
                        case 1: {
                            ComPtr<ID3D11Texture1D> lut1d;
                            ctrl.CreateTexture(&lut1d, cube->size, 3u, cube->data.data());

                            ComPtr<ID3D11ShaderResourceView> srv;
                            ctrl.CreateShaderResourceView(&srv, lut1d.Get());

                            lut = {1, std::move(srv), std::move(lut1d)};
                            break;
                        }
                        case 3: {
                            ComPtr<ID3D11Texture3D> lut3d;
                            ctrl.CreateTexture(&lut3d, cube->size, reinterpret_cast<const RGBAF16*>(cube->data.data()));

                            ComPtr<ID3D11ShaderResourceView> srv;
                            ctrl.CreateShaderResourceView(&srv, lut3d.Get());

                            lut = {3, std::move(srv), std::move(lut3d)};
                            break;
                        }
                        default:
                            Logger::Error(L"Invalid LUT dimension");
                            return false;
                    }
                } else if (std::ranges::contains(lut::hald::kExtensions, ext)) {
                    const auto hald = lut::hald::Load(path);

                    if (!hald.has_value()) {
                        Logger::Error(L"Failed to load Hald CLUT");
                        return false;
                    }

                    ComPtr<ID3D11Texture3D> lut3d;
                    ctrl.CreateTexture(&lut3d, hald->size, hald->data.data());

                    ComPtr<ID3D11ShaderResourceView> srv;
                    ctrl.CreateShaderResourceView(&srv, lut3d.Get());

                    lut = {3, std::move(srv), std::move(lut3d)};
                } else {
                    Logger::Error(L"Unsupported file format");
                    return false;
                }
            }

            switch (lut->dimension) {
                case 1:
                    ctrl.Draw<PixelShaderIndex::kLUT1D>(&dst, w, h, {src, lut->srv.Get()}, &params);
                    break;
                case 3:
                    ctrl.Draw<PixelShaderIndex::kLUT3D>(&dst, w, h, {src, lut->srv.Get()}, &params);
                    break;
                default:
                    Logger::Error(L"Invalid LUT dimension");
                    return false;
            }
        }

        return true;
    } catch (const std::exception& e) {
        Logger::Error(string::ToWstring(string::AsUtf8(e.what())));
        return false;
    }
}

constinit FILTER_PLUGIN_TABLE info = {
    .flag = FILTER_PLUGIN_TABLE::FLAG_VIDEO | FILTER_PLUGIN_TABLE::FLAG_FILTER,
    .name = L"ColorLUT_K",
    .label = L"色調整",
    .information = L"ColorLUT_K v" VERSION L" by Korarei",
    .items = props,
    .func_proc_video = Apply,
    .func_proc_audio = nullptr,
};
}  // namespace

namespace lut::filter::grading {
void Init(HOST_APP_TABLE* host) { host->register_filter_plugin(&info); }

void Deinit() { d3d.Release(); }
}  // namespace lut::filter::grading
