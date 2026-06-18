#include "object.hpp"

#include <cstdint>

#pragma warning(push)
#pragma warning(disable : 4201)  // 非標準の無名構造体 (filter2.h FILTER_ITEM_COLOR)
#include <filter2.h>
#pragma warning(pop)

#include <intern/aviutl/aviutl.hpp>
#include <intern/string.hpp>

#include <hald.h>

namespace {
namespace string = lut::string;

using Logger = lut::aviutl::Logger;

uintptr_t* props = []() {
    static FILTER_ITEM_TRACK level(L"Level", 8.0, 2.0, 24.0, 1.0);
    static FILTER_ITEM_BUTTON resize(L"Resize Scene to LUT", [](EDIT_SECTION* edit) {
        const int lv = static_cast<int>(level.value);
        const int size = lv * lv * lv;
        edit->set_scene_size(size, size);
    });

    static void* items[] = {&level, &resize, nullptr};
    return reinterpret_cast<uintptr_t*>(items);
}();

auto& level = reinterpret_cast<FILTER_ITEM_TRACK*>(props[0])->value;

constexpr bool Draw(FILTER_PROC_VIDEO* context) {
    uint32_t lv = static_cast<uint32_t>(level);

    try {
        const int size = static_cast<int>(lv * lv * lv);
        context->set_image_data(nullptr, size, size);

        if (!context->exec_pixelshader_data(g_hald, sizeof(g_hald), L"object", nullptr, 0, &lv, sizeof(uint32_t),
                                            nullptr, nullptr)) {
            Logger::Error(L"Failed to execute pixel shader");
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        Logger::Error(string::ToWstring(string::AsUtf8(e.what())));
        return false;
    }
}

FILTER_PLUGIN_TABLE info = {
    .flag = FILTER_PLUGIN_TABLE::FLAG_VIDEO | FILTER_PLUGIN_TABLE::FLAG_INPUT,
    .name = L"HaldCLUT_K",
    .label = L"LUT",
    .information = L"HaldCLUT_K generates Identity Hald CLUTs",
    .items = reinterpret_cast<void**>(props),
    .func_proc_video = Draw,
    .func_proc_audio = nullptr,
};
}  // namespace

namespace lut::filter::object {
void Init(HOST_APP_TABLE* host) { host->register_filter_plugin(&info); }
}  // namespace lut::filter::object
