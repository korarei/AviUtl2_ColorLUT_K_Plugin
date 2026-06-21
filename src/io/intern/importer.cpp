#include "importer.hpp"

#include <cwctype>
#include <filesystem>
#include <format>
#include <string>

#include <intern/aviutl/aviutl.hpp>
#include <intern/lut/lut.hpp>
#include <intern/string.hpp>

namespace {
namespace aul = lut::aviutl;
namespace string = lut::string;

void OnDrop(EDIT_SECTION* edit, const wchar_t* file) {
    std::filesystem::path path(file);

    auto ext = path.extension().wstring();

    if (ext.empty()) {
        aul::Logger::Error(L"File extension not specified");
        return;
    }

    std::ranges::for_each(ext, [](wchar_t& c) { c = std::towlower(c); });

    if (ext != lut::kExtension.cube && !std::ranges::contains(lut::kExtension.texture, ext)) {
        aul::Logger::Error(L"Unsupported file format");
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
        string::AsString(path.u8string()));

    int layer, frame;
    if (!edit->get_mouse_layer_frame(&layer, &frame)) {
        layer = edit->info->layer, frame = edit->info->frame;
    }

    if (auto handle = edit->create_object_from_alias(alias.c_str(), layer, frame, 0)) {
        edit->set_focus_object(handle);
        aul::Logger::Info(std::format(L"Created filter object: Layer = {}, Frame = {}", layer, frame));
    } else {
        aul::Logger::Error(L"Failed to create filter object");
    }
}
}  // namespace

namespace lut::io::importer {
void Init(HOST_APP_TABLE* host) {
    host->register_file_drop_handler(L"LUT フィルタをレイヤーに追加", L"Cube LUT File (*.cube)\0*.cube\0\0", OnDrop);
}
}  // namespace lut::io::importer
