#include "baker.hpp"

#include <cstring>

#include <d3d11.h>
#include <wrl/client.h>

#include "lut.hpp"

namespace {
using Microsoft::WRL::ComPtr;
constinit LOG_HANDLE *logger = nullptr;

Hald2Cube lut{};

FILTER_ITEM_SELECT::ITEM modes[] = {{L"Generate Identity", 0}, {L"Convert to Cube", 1}, {nullptr}};
auto mode = FILTER_ITEM_SELECT(L"Operation Mode", 1, modes);
auto group0 = FILTER_ITEM_GROUP(L"Identity Settings", false);
auto level = FILTER_ITEM_TRACK(L"Level", 8.0, 2.0, 24.0, 1.0);
auto group1 = FILTER_ITEM_GROUP(L"Conversion Settings", true);
auto title = FILTER_ITEM_STRING(L"Title", L"");
auto convert = FILTER_ITEM_BUTTON(L"Convert to .cube", []([[maybe_unused]] EDIT_SECTION *edit) {
    if (mode.value != 1)
        return;

    lut.convert(std::filesystem::path(title.value).u8string(), [](bool success) {
        if (logger == nullptr)
            return;

        if (success)
            logger->info(logger, L"Successfully converted to .cube");
        else
            logger->error(logger, L"Failed to convert to .cube");
    });
});
void *items[] = {&mode, &group0, &level, &group1, &title, &convert, nullptr};

bool
func_proc_video(FILTER_PROC_VIDEO *video) {
    try {
        switch (mode.value) {
            case 0: {
                const auto lv = static_cast<uint32_t>(level.value);
                const auto size = lv * lv * lv;
                video->set_image_data(nullptr, size, size);
                auto dst = video->get_image_texture2d();
                if (!lut.generate_identity(dst)) {
                    logger->error(logger, L"Failed to generate identity");
                    return false;
                }
                break;
            }
            case 1: {
                auto src = video->get_image_texture2d();
                if (!lut.load_hald(src)) {
                    logger->error(logger, L"Invalid Hald image size");
                    return false;
                }
                break;
            }
            default:
                logger->error(logger, L"Invalid operation mode");
                return false;
        }
    } catch (const std::exception &e) {
        std::filesystem::path p(reinterpret_cast<const char8_t *>(e.what()));  // 手抜きutf8->utf16変換
        logger->error(logger, p.wstring().c_str());
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
