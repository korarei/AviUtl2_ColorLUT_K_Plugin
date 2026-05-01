#include "identity.hpp"

#include <plugin2.h>

#include "direct3d.hpp"
#include "utilities.hpp"

#include <identity.h>

namespace lut::filter::identity::intern {
using Direct3D = direct3d::Direct3D<0uz, 1uz, 1uz, 1uz>;

constinit LOG_HANDLE *logger = nullptr;

struct alignas(16) Params {
    uint32_t level;
    uint32_t _padding[3];
};

Direct3D d3d({sizeof(Params)}, {g_identity});

auto level = FILTER_ITEM_TRACK(L"Level", 8.0, 2.0, 24.0, 1.0);
auto fit_scene = FILTER_ITEM_BUTTON(L"Resize Scene to LUT", [](EDIT_SECTION *edit) {
    const int lv = static_cast<int>(level.value);
    const int size = lv * lv * lv;
    edit->set_scene_size(size, size);
});
void *items[] = {&level, &fit_scene, nullptr};

bool
draw(FILTER_PROC_VIDEO *video) {
    const Params params{static_cast<uint32_t>(level.value), {0u, 0u, 0u}};

    try {
        const int size = static_cast<int>(params.level * params.level * params.level);
        video->set_image_data(nullptr, size, size);

        auto tex = video->get_image_texture2d();
        const auto ctrl = d3d.init(tex, nullptr);

        const auto dst = ctrl.fetch_cache<0uz>(tex);
        ctrl.pixel_shader<0uz, 0uz, 0uz>()(&dst.rtv, size, size, params);
        return true;
    } catch (const std::exception &e) {
        const auto err = string::to_wstring(string::as_utf8(e.what()));
        logger->error(logger, err.c_str());
        return false;
    }
}
}  // namespace lut::filter::identity::intern

namespace lut::filter::identity {
constinit FILTER_PLUGIN_TABLE info = {
        .flag = FILTER_PLUGIN_TABLE::FLAG_VIDEO | FILTER_PLUGIN_TABLE::FLAG_INPUT,
        .name = L"HaldCLUT_K",
        .label = L"LUT",
        .information = L"HaldCLUT_K generates Identity Hald CLUTs",
        .items = intern::items,
        .func_proc_video = intern::draw,
        .func_proc_audio = nullptr,
};

void
init(LOG_HANDLE *handle) noexcept {
    intern::logger = handle;
}

void
deinit() {
    intern::d3d.release();
}
}  // namespace lut::filter::identity
