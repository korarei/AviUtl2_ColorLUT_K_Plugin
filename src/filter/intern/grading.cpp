#include "grading.hpp"

#include <d3d11.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstddef>
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
namespace aul = lut::aviutl;
namespace string = lut::string;

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;
using LUTCache = lut::LUTCache;
using LUTView = lut::LUTView;
using CubeLUT = lut::CubeLUT;
using HaldLUT = lut::HaldLUT;
using StripLUT = lut::StripLUT;
using PixelShaderDesc = lut::direct3d::PixelShaderDesc;
using Direct3D = lut::direct3d::Direct3D<1uz, 2uz>;  // 1 cache, 2 pixel shaders
using RGBAF16 = Direct3D::RGBAF16;

struct alignas(16) Params {
    int32_t blend_mode;
    float opacity;
    float should_clamp;
    float seed;
};

enum BlendMode : int {
    kNormal = 0,
    kDissolve,
    kDarken,
    kMultiply,
    kColorBurn,
    kLinearBurn,
    kDarkerColor,
    kLighten,
    kScreen,
    kColorDodge,
    kLinearDodge,
    kLighterColor,
    kOverlay,
    kSoftLight,
    kHardLight,
    kLinearLight,
    kVividLight,
    kPinLight,
    kHardMix,
    kDifference,
    kExclusion,
    kSubtract,
    kDivide,
    kHue,
    kSaturation,
    kColor,
    kLuminosity,
};

enum CacheIndex : size_t {
    kSource = 0uz,
};

enum PixelShaderIndex : size_t {
    kLUT1D = 0uz,
    kLUT3D,
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
                      L"Hald CLUT / Strip LUT File (*.bmp;*.png;*.tiff;*.tif)\0*.bmp;*.png;*.tiff;*.tif;\0\0");
FILTER_ITEM_BUTTON reload_lut(L"Reload LUT", [](EDIT_SECTION* edit) {
    LUTCache::Reset(file.value);
    edit->set_cursor_layer_frame(edit->info->layer, edit->info->frame);
});

namespace compositing {
FILTER_ITEM_GROUP name(L"Compositing", false);
FILTER_ITEM_SELECT::ITEM blend_modes[] = {
    {L"Normal", BlendMode::kNormal},
    {L"Dissolve", BlendMode::kDissolve},
    {L"Darken", BlendMode::kDarken},
    {L"Multiply", BlendMode::kMultiply},
    {L"Color Burn", BlendMode::kColorBurn},
    {L"Linear Burn", BlendMode::kLinearBurn},
    {L"Darker Color", BlendMode::kDarkerColor},
    {L"Lighten", BlendMode::kLighten},
    {L"Screen", BlendMode::kScreen},
    {L"Color Dodge", BlendMode::kColorDodge},
    {L"Linear Dodge (Add)", BlendMode::kLinearDodge},
    {L"Lighter Color", BlendMode::kLighterColor},
    {L"Overlay", BlendMode::kOverlay},
    {L"Soft Light", BlendMode::kSoftLight},
    {L"Hard Light", BlendMode::kHardLight},
    {L"Linear Light", BlendMode::kLinearLight},
    {L"Vivid Light", BlendMode::kVividLight},
    {L"Pin Light", BlendMode::kPinLight},
    {L"Hard Mix", BlendMode::kHardMix},
    {L"Difference", BlendMode::kDifference},
    {L"Exclusion", BlendMode::kExclusion},
    {L"Subtract", BlendMode::kSubtract},
    {L"Divide", BlendMode::kDivide},
    {L"Hue", BlendMode::kHue},
    {L"Saturation", BlendMode::kSaturation},
    {L"Color", BlendMode::kColor},
    {L"Luminosity", BlendMode::kLuminosity},
    {nullptr, -1},
};
FILTER_ITEM_SELECT blend_mode(L"Compositing::Blend Mode", 0, blend_modes);
FILTER_ITEM_TRACK opacity(L"Compositing::Opacity", 100.0, 0.0, 100.0, 0.01);
FILTER_ITEM_CHECK should_clamp(L"Compositing::Clamp", false);
}  // namespace compositing
}  // namespace property

void* props[] = {
    &property::file,
    &property::reload_lut,
    &property::compositing::name,
    &property::compositing::blend_mode,
    &property::compositing::opacity,
    &property::compositing::should_clamp,
    nullptr,
};

bool Apply(FILTER_PROC_VIDEO* ctx) {
    namespace prop = property;

    if (prop::file.value[0] == L'\0') {
        return true;
    }

    const int w = ctx->object->width, h = ctx->object->height;

    if (w < 1 || h < 1) {
        return false;
    }

    const Params params{
        .blend_mode = prop::compositing::blend_mode.value,
        .opacity = static_cast<float>(prop::compositing::opacity.value * 0.01),
        .should_clamp = prop::compositing::should_clamp.value ? 1.0f : 0.0f,
        .seed = static_cast<float>(w * h),
    };

    try {
        const auto tex = ctx->get_image_texture2d();
        const auto ctrl = d3d.Init(tex, []() { LUTCache::Reset(); });

        const auto dst = ctrl.GetBackBuffer(tex);
        const auto src = ctrl.CopyBuffer<CacheIndex::kSource>(tex, w, h);

        {
            const auto entry = LUTCache::Find(ctx->object->effect_id, prop::file.value);
            auto& lut = *entry;

            if (!lut.has_value()) {
                const std::filesystem::path path(prop::file.value);
                auto ext = path.extension().wstring();

                if (ext.empty()) {
                    aul::Logger::Error(L"File extension not specified");
                    return false;
                }

                std::ranges::for_each(ext, [](wchar_t& c) { c = std::towlower(c); });

                auto load_lut = [&](const LUTView& view) {
                    std::visit(
                        [&](auto p) {
                            using T = std::decay_t<decltype(p)>;

                            if constexpr (std::is_same_v<T, LUTView::LUT3D>) {
                                ComPtr<ID3D11Texture3D> data;
                                ctrl.CreateTexture(&data, view.size, p.data());

                                ComPtr<ID3D11ShaderResourceView> srv;
                                ctrl.CreateShaderResourceView(&srv, data.Get());

                                lut = {
                                    .dimension = 3,
                                    .view = std::move(srv),
                                    .data = std::move(data),
                                };
                            } else if constexpr (std::is_same_v<T, LUTView::LUT1D>) {
                                ComPtr<ID3D11Texture1D> data;
                                ctrl.CreateTexture(&data, view.size, 3u, p.data());

                                ComPtr<ID3D11ShaderResourceView> srv;
                                ctrl.CreateShaderResourceView(&srv, data.Get());

                                lut = {
                                    .dimension = 1,
                                    .view = std::move(srv),
                                    .data = std::move(data),
                                };
                            } else {
                                std::unreachable();
                            }
                        },
                        view.data);
                };

                if (ext == lut::kExtension.cube) {
                    const auto cube = CubeLUT::Import(path);

                    if (!cube.has_value()) {
                        aul::Logger::Error(L"Failed to load Cube LUT");
                        return false;
                    }

                    load_lut(cube->View());
                } else if (std::ranges::contains(lut::kExtension.texture, ext)) {
                    const auto hald = HaldLUT::Import(path);

                    if (!hald.has_value()) {
                        auto strip = StripLUT::Import(path);

                        if (!strip.has_value()) {
                            aul::Logger::Error(L"Failed to load LUT texture");
                            return false;
                        }

                        load_lut(CubeLUT::Init(std::move(*strip)).View());
                    } else {
                        load_lut(hald->View());
                    }
                } else {
                    aul::Logger::Error(L"Unsupported file format");
                    return false;
                }
            }

            switch (lut->dimension) {
                case 1:
                    ctrl.Draw<PixelShaderIndex::kLUT1D>(&dst, w, h, {src, lut->view.Get()}, &params);
                    break;
                case 3:
                    ctrl.Draw<PixelShaderIndex::kLUT3D>(&dst, w, h, {src, lut->view.Get()}, &params);
                    break;
                default:
                    std::unreachable();
            }
        }

        return true;
    } catch (const std::exception& e) {
        aul::Logger::Error(string::ToWstring(string::AsUtf8(e.what())));
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
