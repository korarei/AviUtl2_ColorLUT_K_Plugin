#include "baker.hpp"

#include <cstring>

#include <d3d11.h>
#include <wrl/client.h>

#include "lut.hpp"
#include "utility.hpp"

namespace {
using Microsoft::WRL::ComPtr;
constinit LOG_HANDLE *logger = nullptr;

Hald2Cube lut{};

FILTER_ITEM_SELECT::ITEM actions[] = {{L"Generate Identity Hald CLUT", 0}, {L"Export Cube LUT", 1}, {nullptr}};
auto action = FILTER_ITEM_SELECT(L"Action", 1, actions);
auto hald_group = FILTER_ITEM_GROUP(L"Hald CLUT Generation", false);
auto level = FILTER_ITEM_TRACK(L"Level", 8.0, 2.0, 24.0, 1.0);
auto fit_scene = FILTER_ITEM_BUTTON(L"Fit Scene to LUT", [](EDIT_SECTION *edit) {
    if (action.value != 0) {
        logger->error(logger, L"Invalid action");
        return;
    }

    const auto lv = static_cast<int>(level.value);
    const auto size = lv * lv * lv;
    edit->set_scene_size(size, size);
});
auto cube_group = FILTER_ITEM_GROUP(L"Cube LUT Export", true);
auto title = FILTER_ITEM_STRING(L"Title", L"");
auto export_cube = FILTER_ITEM_BUTTON(L"Export as .cube...", [](EDIT_SECTION *edit) {
    if (action.value != 1) {
        logger->error(logger, L"Invalid action");
        return;
    }

    auto object = edit->get_object_layer_frame(edit->get_focus_object());

    if (edit->info->frame < object.start || object.end < edit->info->frame)
        logger->warn(logger, L"Hald CLUT was not updated");

    lut.export_cube(string::to_u8str(title.value), [](bool success, const wchar_t *msg) {
        if (success)
            logger->info(logger, msg);
        else
            logger->error(logger, msg);
    });
});
void *items[] = {&action, &hald_group, &level, &fit_scene, &cube_group, &title, &export_cube, nullptr};

bool
func_proc_video(FILTER_PROC_VIDEO *video) {
    try {
        switch (action.value) {
            case 0: {
                const auto lv = static_cast<int>(level.value);
                const auto size = lv * lv * lv;
                video->set_image_data(nullptr, size, size);
                auto dst = video->get_image_texture2d();
                if (!lut.draw_identity(dst)) {
                    logger->error(logger, L"Failed to generate identity");
                    return false;
                }
                break;
            }
            case 1: {
                auto src = video->get_image_texture2d();
                if (!lut.load(src)) {
                    logger->error(logger, L"Invalid Hald image size");
                    return false;
                }
                break;
            }
            default:
                logger->error(logger, L"Invalid action");
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

FILTER_PLUGIN_TABLE hald2cube::info = {
        .flag = FILTER_PLUGIN_TABLE::FLAG_VIDEO,
        .name = L"Hald2Cube_K",
        .label = L"Utility",
        .information = L"Hald2Cube_K by Korarei",
        .items = items,
        .func_proc_video = func_proc_video,
};

void
hald2cube::initialize_logger(LOG_HANDLE *log) {
    logger = log;
}

void
hald2cube::set_owner(HWND hwnd) {
    lut.set_owner(hwnd);
}
