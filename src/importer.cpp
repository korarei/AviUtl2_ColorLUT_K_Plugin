#include "importer.hpp"

#include <cwctype>
#include <filesystem>
#include <format>
#include <string>

#include "core/utility.hpp"

namespace {
constinit LOG_HANDLE *logger = nullptr;
}  // namespace

void
importer::initialize_logger(LOG_HANDLE *log) {
    logger = log;
}

void
importer::add_filter(EDIT_SECTION *edit, const wchar_t *file) {
    std::filesystem::path path(file);

    auto ext = path.extension().wstring();
    std::ranges::for_each(ext, [](wchar_t &c) { c = std::towlower(c); });
    if (ext != L".cube" && ext != L".bmp" && ext != L".png" && ext != L".tif" && ext != L".tiff") {
        logger->error(logger, L"invalid file extension");
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
            string::to_str(path.u8string()));

    // get_mouse_layer_frameはダメそう (あさってのところに置かれる)

    int layer = edit->info->layer;
    int frame = edit->info->frame;

    if (auto object = edit->create_object_from_alias(alias.c_str(), layer, frame, 0)) {
        edit->set_focus_object(object);

        std::wstring msg = std::format(L"create filter object [ColorLUT_K] layer={}, frame={}", layer, frame);
        logger->info(logger, msg.c_str());
    } else {
        logger->error(logger, L"create filter object failed [ColorLUT_K]");
    }
}
