#include "filter.hpp"

#include <filesystem>

#include <d2d1_3.h>
#include <d3d11.h>
#include <wrl/client.h>

#include "lut.hpp"
#include "utility.hpp"

namespace {
using Microsoft::WRL::ComPtr;

constinit LOG_HANDLE *logger = nullptr;

ColorLUT lut{};

auto file = FILTER_ITEM_FILE(L"LUT File", L"",
                             L"Cube LUT File (*.cube)\0*.cube\0"
                             L"Hald CLUT File (*.bmp;*.png;*.tiff;*.tif)\0*.bmp;*.png;*.tiff;*.tif;\0\0");
auto reload = FILTER_ITEM_BUTTON(L"Reload LUT", [](EDIT_SECTION *edit) {
    lut.reload(file.value);
    edit->set_cursor_layer_frame(edit->info->layer, edit->info->frame);
});
auto compositing_group = FILTER_ITEM_GROUP(L"Compositing", false);
FILTER_ITEM_SELECT::ITEM modes[] = {
        {L"Normal", 0},         {L"Darken", 2},
        {L"Multiply", 3},       {L"Color Burn", 4},
        {L"Linear Burn", 5},    {L"Darker Color", 6},
        {L"Lighten", 7},        {L"Screen", 8},
        {L"Color Dodge", 9},    {L"Linear Dodge (Add)", 10},
        {L"Lighter Color", 11}, {L"Overlay", 12},
        {L"Soft Light", 13},    {L"Hard Light", 14},
        {L"Linear Light", 15},  {L"Vivid Light", 16},
        {L"Pin Light", 17},     {L"Hard Mix", 18},
        {L"Difference", 19},    {L"Exclusion", 20},
        {L"Subtract", 21},      {L"Divide", 22},
        {L"Hue", 23},           {L"Saturation", 24},
        {L"Color", 25},         {L"Luminosity", 26},
        {nullptr, -1},
};
auto mode = FILTER_ITEM_SELECT(L"Blend Mode", 0, modes);
auto opacity = FILTER_ITEM_TRACK(L"Opacity", 100.0, 0.0, 100.0, 0.01);
auto clamp = FILTER_ITEM_CHECK(L"Clamp", false);
void *items[] = {&file, &reload, &compositing_group, &mode, &opacity, &clamp, nullptr};

bool
func_proc_video(FILTER_PROC_VIDEO *video) {
    std::filesystem::path path(file.value);

    if (path.empty())
        return true;

    try {
        auto src = video->get_image_texture2d();
        ComPtr<ID3D11Texture2D> dst;
        lut.setup(src);
        lut.create_texture(&dst);

        ComPtr<ID2D1Bitmap1> input;
        ComPtr<ID2D1Bitmap1> target;
        lut.wrap_texture(&input, src, D2D1_BITMAP_OPTIONS_NONE);
        lut.wrap_texture(&target, dst.Get(), D2D1_BITMAP_OPTIONS_TARGET);

        ComPtr<ID2D1Effect> fx;
        if (!lut.build_effect(&fx, input.Get(), path, static_cast<int>(mode.value),
                              static_cast<float>(opacity.value) * 0.01f, clamp.value)) {
            logger->error(logger, L"Failed to load LUT");
            return false;
        }

        lut.draw(target.Get(), fx.Get());
        lut.copy(src, dst.Get());
    } catch (const std::exception &e) {
        const auto err = string::to_wstr(reinterpret_cast<const char8_t *>(e.what()));
        logger->error(logger, err.c_str());
        return false;
    }

    return true;
}
}  // namespace

FILTER_PLUGIN_TABLE color_lut::info = {
        .flag = FILTER_PLUGIN_TABLE::FLAG_VIDEO | FILTER_PLUGIN_TABLE::FLAG_FILTER,
        .name = L"ColorLUT_K",
        .label = L"色調整",
        .information = L"ColorLUT_K applies 1D and 3D LUTs to video.",
        .items = items,
        .func_proc_video = func_proc_video,
        .func_proc_audio = nullptr,
};

void
color_lut::clear_cache() {
    lut.reload();
}

void
color_lut::initialize_logger(LOG_HANDLE *log) {
    logger = log;
}
