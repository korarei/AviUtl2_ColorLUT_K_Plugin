#include "transform.hpp"

#include <d3d11.h>
#include <wrl/client.h>

#include <plugin2.h>

#include "cache.hpp"
#include "direct3d.hpp"
#include "utilities.hpp"

#include <lut1d.h>
#include <lut3d.h>

namespace lut::filter::transform::intern {
using Direct3D = direct3d::Direct3D<1uz, 1uz, 1uz, 2uz>;
using LUTCache = cache::LUTCache;

constinit LOG_HANDLE *logger = nullptr;

struct alignas(16) Params {
    int blend_mode;
    float opacity;
    float should_clamp;
    float seed;
};

Direct3D d3d({sizeof(Params)}, {g_lut1d, g_lut3d});
LUTCache cache;

auto file = FILTER_ITEM_FILE(
        L"LUT File",
        L"",
        L"Cube LUT File (*.cube)\0*.cube\0"
        L"Hald CLUT File (*.bmp;*.png;*.tiff;*.tif)\0*.bmp;*.png;*.tiff;*.tif;\0\0");
auto reload = FILTER_ITEM_BUTTON(L"Reload LUT", [](EDIT_SECTION *edit) {
    cache.release(file.value);
    edit->set_cursor_layer_frame(edit->info->layer, edit->info->frame);
});
auto group_compositing = FILTER_ITEM_GROUP(L"Compositing", false);
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
auto blend_mode = FILTER_ITEM_SELECT(L"Blend Mode", 0, blend_modes);
auto opacity = FILTER_ITEM_TRACK(L"Opacity", 100.0, 0.0, 100.0, 0.01);
auto clamp = FILTER_ITEM_CHECK(L"Clamp", false);
void *items[] = {
        &file, &reload, &group_compositing, &blend_mode, &opacity, &clamp, nullptr};

constexpr bool
apply(FILTER_PROC_VIDEO *video) {
    if (file.value[0] == L'\0')
        return true;

    const int w = video->object->width, h = video->object->height;
    if (w < 1 || h < 1)
        return true;

    const Params params{
            .blend_mode = static_cast<int>(blend_mode.value),
            .opacity = static_cast<float>(opacity.value * 0.01),
            .should_clamp = clamp.value ? 1.0f : 0.0f,
            .seed = static_cast<float>(w * h),
    };

    try {
        const auto tex = video->get_image_texture2d();
        const auto ctrl = d3d.init(tex, []() { cache.release(); });

        const auto dst = ctrl.fetch_cache<0uz>(tex);
        const auto src = ctrl.blit<0uz>(&dst.srv, w, h);

        LUTCache::LUT lut;
        if (!cache.load(lut, video->object->effect_id, file.value, ctrl)) {
            logger->error(logger, L"Failed to load LUT");
            return false;
        }

        std::array<ID3D11ShaderResourceView *, 2> srvs{src.srv, lut.srv.Get()};

        switch (lut.dimension) {
            case 1:
                ctrl.pixel_shader<0uz, 0uz, 2uz>()(&dst.rtv, w, h, srvs, params);
                break;
            case 3:
                ctrl.pixel_shader<1uz, 0uz, 2uz>()(&dst.rtv, w, h, srvs, params);
                break;
            default:
                logger->error(logger, L"Invalid LUT dimension");
                return false;
        }
        return true;
    } catch (const std::exception &e) {
        const auto err = string::to_wstring(string::as_utf8(e.what()));
        logger->error(logger, err.c_str());
        return false;
    }
}
}  // namespace lut::filter::transform::intern

namespace lut::filter::transform {
constinit FILTER_PLUGIN_TABLE info = {
        .flag = FILTER_PLUGIN_TABLE::FLAG_VIDEO | FILTER_PLUGIN_TABLE::FLAG_FILTER,
        .name = L"ColorLUT_K",
        .label = L"色調整",
        .information = L"ColorLUT_K applies 1D and 3D LUTs to video",
        .items = intern::items,
        .func_proc_video = intern::apply,
        .func_proc_audio = nullptr,
};

void
reset() {
    intern::cache.release();
}

void
init(LOG_HANDLE *handle) noexcept {
    intern::logger = handle;
}

void
deinit() {
    intern::cache.release();
    intern::d3d.release();
}
}  // namespace lut::filter::transform
