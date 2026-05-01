#include "importer.hpp"

#include <cwctype>
#include <filesystem>
#include <format>
#include <string>

#include <utilities.hpp>

namespace {
constinit LOG_HANDLE *logger = nullptr;
}

namespace lut::io::importer {
void
init(LOG_HANDLE *handle) noexcept {
    logger = handle;
}

void
on_drop(EDIT_SECTION *edit, const wchar_t *file) {
    std::filesystem::path path(file);

    auto ext = path.extension().wstring();
    std::ranges::for_each(ext, [](wchar_t &c) { c = std::towlower(c); });
    if (ext != L".cube" && ext != L".bmp" && ext != L".png" && ext != L".tif"
        && ext != L".tiff") {
        logger->error(logger, L"Invalid file extension");
        return;
    }

    std::string alias = std::format(
            "[Object]\n"
            "[Object.0]\n"
            "effect.name=フィルタオブジェクト\n"
            "[Object.1]\n"
            "effect.name=ColorLUT_K\n"
            "LUT File={}\n"
            "Reload LUT=\n"
            "Compositing.hide=1\n"
            "Blend Mode=Normal\n"
            "Opacity=100.00\n"
            "Clamp=0\n",
            string::as_string(path.u8string()));

    int layer, frame;
    if (!edit->get_mouse_layer_frame(&layer, &frame))
        layer = edit->info->layer, frame = edit->info->frame;

    if (auto object = edit->create_object_from_alias(alias.c_str(), layer, frame, 0)) {
        edit->set_focus_object(object);

        std::wstring msg = std::format(
                L"Create filter object [ColorLUT_K] layer={}, frame={}", layer, frame);
        logger->info(logger, msg.c_str());
    } else {
        logger->error(logger, L"Failed to create filter object [ColorLUT_K]");
    }
}
}  // namespace lut::io::importer
