#include "filter.hpp"

#include <filesystem>
#include <stdexcept>

#include <d2d1_3.h>
#include <d3d11.h>
#include <wrl/client.h>

#include "lut.hpp"

namespace {
using Microsoft::WRL::ComPtr;

constinit LOG_HANDLE *logger = nullptr;

ColorLUT lut{};

auto file = FILTER_ITEM_FILE(L"LUT File", L"",
                             L"Cube LUT File (*.cube)\0*.cube\0"
                             L"Hald LUT File (*.bmp;*.png;*.tiff;*.tif)\0*.bmp;*.png;*.tiff;*.tif;\0\0");
auto reload = FILTER_ITEM_BUTTON(L"Reload LUT", []([[maybe_unused]] EDIT_SECTION *edit) { lut.reload(file.value); });
auto group0 = FILTER_ITEM_GROUP(L"Compositing", false);
auto opacity = FILTER_ITEM_TRACK(L"Opacity", 100.0, 0.0, 100.0, 0.01);
void *items[] = {&file, &reload, &group0, &opacity, nullptr};

bool
func_proc_video(FILTER_PROC_VIDEO *video) {
    std::filesystem::path path(file.value);

    if (path.empty())
        return true;

    try {
        auto src = video->get_image_texture2d();
        ComPtr<ID3D11Texture2D> dst;
        lut.setup(src);
        lut.create_texture2d(&dst);

        ComPtr<ID2D1Bitmap1> input;
        ComPtr<ID2D1Bitmap1> target;
        lut.create_bitmap(src, D2D1_BITMAP_OPTIONS_NONE, &input);
        lut.create_bitmap(dst.Get(), D2D1_BITMAP_OPTIONS_TARGET, &target);

        ComPtr<ID2D1Effect> fx;
        if (!lut.create_effect(path, static_cast<float>(opacity.value) * 0.01f, input.Get(), &fx)) {
            logger->error(logger, L"Failed to load LUT");
            return false;
        }

        lut.draw(target.Get(), fx.Get());
        lut.copy(src, dst.Get());
    } catch (const std::exception &e) {
        std::filesystem::path p(reinterpret_cast<const char8_t *>(e.what()));  // 手抜きutf8->utf16変換
        logger->error(logger, p.wstring().c_str());
        return false;
    }

    return true;
}
}  // namespace

FILTER_PLUGIN_TABLE color_lut::info = {
        .flag = FILTER_PLUGIN_TABLE::FLAG_VIDEO | FILTER_PLUGIN_TABLE::FLAG_FILTER,
        .name = L"ColorLUT_K",
        .label = L"色調整",
        .information = L"ColorLUT_K by Korarei",
        .items = items,
        .func_proc_video = func_proc_video,
};

void
color_lut::clear_cache() {
    lut.reload();
}

void
color_lut::initialize_logger(LOG_HANDLE *log) {
    logger = log;
}
