#include "identity.hpp"

#include <cstdint>

#pragma warning(push)
#pragma warning(disable : 4201)  // 非標準の無名構造体 (filter2.h FILTER_ITEM_COLOR)
#include <filter2.h>
#pragma warning(pop)

#include <intern/aviutl/aviutl.hpp>
#include <intern/direct3d/direct3d.hpp>
#include <intern/string.hpp>

#include <identity.h>

namespace {
namespace string = lut::string;

using Logger = lut::aviutl::Logger;
using PixelShaderDesc = lut::direct3d::PixelShaderDesc;
using Direct3D = lut::direct3d::Direct3D<0uz, 1uz>;  // No cache, 1 pixel shader

struct alignas(16) Params {
    uint32_t level;
    uint32_t _padding[3];
};

Direct3D d3d({PixelShaderDesc{g_identity, sizeof(Params)}});

uintptr_t* props = []() {
    static FILTER_ITEM_TRACK level(L"Level", 8.0, 2.0, 24.0, 1.0);
    static FILTER_ITEM_BUTTON fit_scene(L"Resize Scene to LUT", [](EDIT_SECTION* edit) {
        const int lv = static_cast<int>(level.value);
        const int size = lv * lv * lv;
        edit->set_scene_size(size, size);
    });

    static void* items[] = {&level, &fit_scene, nullptr};
    return reinterpret_cast<uintptr_t*>(items);
}();

auto& level = reinterpret_cast<FILTER_ITEM_TRACK*>(props[0])->value;

constexpr bool Draw(FILTER_PROC_VIDEO* context) {
    const Params params{static_cast<uint32_t>(level), {0u, 0u, 0u}};

    try {
        const int size = static_cast<int>(params.level * params.level * params.level);
        context->set_image_data(nullptr, size, size);

        auto tex = context->get_image_texture2d();
        const auto ctrl = d3d.Init(tex);

        const auto dst = ctrl.GetBackBuffer(tex);
        ctrl.Draw<0uz>(&dst, size, size, &params);
        return true;
    } catch (const std::exception& e) {
        Logger::Error(string::ToWstring(string::AsUtf8(e.what())));
        return false;
    }
}

FILTER_PLUGIN_TABLE info = {
    .flag = FILTER_PLUGIN_TABLE::FLAG_VIDEO | FILTER_PLUGIN_TABLE::FLAG_INPUT,
    .name = L"HaldCLUT_K",
    .label = L"LUT",
    .information = L"HaldCLUT_K generates Identity Hald CLUTs",
    .items = reinterpret_cast<void**>(props),
    .func_proc_video = Draw,
    .func_proc_audio = nullptr,
};
}  // namespace

namespace lut::filter::identity {
void Init(HOST_APP_TABLE* host) { host->register_filter_plugin(&info); }

void Deinit() { d3d.Release(); }
}  // namespace lut::filter::identity
