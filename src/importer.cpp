#include "importer.hpp"

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
    std::u8string path = string::to_u8str(file);
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
            std::string(path.begin(), path.end()));

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
