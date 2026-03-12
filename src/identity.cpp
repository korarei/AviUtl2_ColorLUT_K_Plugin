#include "identity.hpp"

#include <d3d11.h>

#include "core/lut.hpp"
#include "core/utility.hpp"

namespace {
constinit LOG_HANDLE *logger = nullptr;

Identity lut{};

auto level = FILTER_ITEM_TRACK(L"Level", 8.0, 2.0, 24.0, 1.0);
auto fit_scene = FILTER_ITEM_BUTTON(L"Resize Scene to LUT", [](EDIT_SECTION *edit) {
    const int lv = static_cast<int>(level.value);
    const int size = lv * lv * lv;
    edit->set_scene_size(size, size);
});
void *items[] = {&level, &fit_scene, nullptr};

bool
func_proc_video(FILTER_PROC_VIDEO *video) {
    try {
        const int lv = static_cast<int>(level.value);
        const int size = lv * lv * lv;
        video->set_image_data(nullptr, size, size);
        auto dst = video->get_image_texture2d();
        if (!lut.draw(dst)) {
            logger->error(logger, L"Failed to generate identity");
            return false;
        }
    } catch (const std::exception &e) {
        const auto err = string::to_wstr(reinterpret_cast<const char8_t *>(e.what()));
        logger->error(logger, err.c_str());
        return false;
    }

    return true;
}
}  // namespace

constinit FILTER_PLUGIN_TABLE identity::info = {
        .flag = FILTER_PLUGIN_TABLE::FLAG_VIDEO | FILTER_PLUGIN_TABLE::FLAG_INPUT,
        .name = L"HaldCLUT_K",
        .label = L"LUT",
        .information = L"HaldCLUT_K generates Hald CLUTs.",
        .items = items,
        .func_proc_video = func_proc_video,
        .func_proc_audio = nullptr,
};

void
identity::initialize_logger(LOG_HANDLE *log) {
    logger = log;
}
