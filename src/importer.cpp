#include "importer.hpp"

#include <string>

#include "core/utility.hpp"

namespace {
constinit LOG_HANDLE *logger = nullptr;
constinit EDIT_HANDLE *edit_handle = nullptr;

INPUT_HANDLE
func_open(const wchar_t *file) {
    std::u8string path = string::to_u8str(file);
    std::string
            alias = "[Object]\n"
                    "[Object.0]\n"
                    "effect.name=フィルタオブジェクト\n"
                    "[Object.1]\n"
                    "effect.name=ColorLUT_K\n"
                    "LUT File="
                  + std::string(path.begin(), path.end())
                  + "\n"
                    "Reload LUT=\n"
                    "Compositing.hide=1\n"
                    "Blend Mode=Normal\n"
                    "Opacity=100.00\n"
                    "Clamp=0\n";

    edit_handle->call_edit_section_param(const_cast<char *>(alias.c_str()), [](void *param, EDIT_SECTION *edit) {
        if (edit->create_object_from_alias(static_cast<const char *>(param), edit->info->layer, edit->info->frame, 0))
            logger->info(logger, L"Success to create object");
        else
            logger->error(logger, L"Failed to create object");
    });

    return nullptr;
}

bool
func_close([[maybe_unused]] INPUT_HANDLE ih) {
    return true;
}

bool
func_info_get([[maybe_unused]] INPUT_HANDLE ih, INPUT_INFO *iip) {
    iip->flag = 0;
    return false;
}

int
func_read_video([[maybe_unused]] INPUT_HANDLE ih, [[maybe_unused]] int frame, [[maybe_unused]] void *buf) {
    return 0;
}
}  // namespace

INPUT_PLUGIN_TABLE importer::info = {
        .flag = INPUT_PLUGIN_TABLE::FLAG_VIDEO,
        .name = L"Import LUT",
        .filefilter = L"Cube LUT File (*.cube)\0*.cube\0\0",
        .information = L"Import LUT File",
        .func_open = func_open,
        .func_close = func_close,
        .func_info_get = func_info_get,
        .func_read_video = func_read_video,
        .func_read_audio = nullptr,
        .func_config = nullptr,
        .func_set_track = nullptr,
        .func_time_to_frame = nullptr,
};

void
importer::initialize_logger(LOG_HANDLE *log) {
    logger = log;
}

void
importer::initialize_edit_handle(EDIT_HANDLE *edit) {
    edit_handle = edit;
}
