#include "neutral.hpp"

#include <cstdint>

#pragma warning(push)
#pragma warning(disable : 4201)  // 非標準の無名構造体 (filter2.h FILTER_ITEM_COLOR)
#include <filter2.h>
#pragma warning(pop)

#include <intern/aviutl/aviutl.hpp>

#include <hald.h>
#include <strip.h>

#ifndef VERSION
#define VERSION L"0.1.0"
#endif

namespace {
namespace aul = lut::aviutl;

enum LUTFormat : int {
    kHald = 0,
    kStrip,
};

namespace property {
FILTER_ITEM_SELECT::ITEM lut_formats[] = {
    {L"Hald", kHald},
    {L"Strip", kStrip},
    {nullptr, -1},
};
FILTER_ITEM_SELECT lut_format(L"LUT Format", 0, lut_formats);

namespace hald {
FILTER_ITEM_GROUP name(L"Hald Settings", true);
FILTER_ITEM_TRACK level(L"Hald::Level", 8.0, 2.0, 16.0, 1.0);
}  // namespace hald

namespace strip {
FILTER_ITEM_GROUP name(L"Strip Settings", false);
FILTER_ITEM_TRACK size(L"Strip::Size", 16.0, 2.0, 128.0, 1.0);
}  // namespace strip

namespace visibility {
FILTER_ITEM_GROUP name(L"Visibility", false);

namespace should_show_in {
FILTER_ITEM_SEPARATOR name(L"Show In");
FILTER_ITEM_CHECK viewports(L"Visibility::Show In::Viewports", false);
FILTER_ITEM_CHECK renders(L"Visibility::Show In::Renders", true);
}  // namespace should_show_in
}  // namespace visibility
}  // namespace property

void* props[] = {
    &property::lut_format,
    &property::hald::name,
    &property::hald::level,
    &property::strip::name,
    &property::strip::size,
    &property::visibility::name,
    &property::visibility::should_show_in::name,
    &property::visibility::should_show_in::viewports,
    &property::visibility::should_show_in::renders,
    nullptr,
};

bool Draw(FILTER_PROC_VIDEO* ctx) {
    namespace prop = property;

    constexpr float kEpsilon = 1.0e-4f;

    const bool should_pass = aul::Context::handle()->get_edit_state() == EDIT_HANDLE::EDIT_STATE_SAVE
                                 ? !prop::visibility::should_show_in::renders.value
                                 : !prop::visibility::should_show_in::viewports.value;

    float ox = 0.0f, oy = 0.0f;

    switch (prop::lut_format.value) {
        case LUTFormat::kHald: {
            uint32_t level = static_cast<uint32_t>(prop::hald::level.value);
            const int w = static_cast<int>(level * level * level);

            if (w > ctx->scene->width || w > ctx->scene->height) {
                aul::Logger::Warning(L"LUT size exceeds scene size");
            }

            if (should_pass) {
                return true;
            }

            ctx->set_image_data(nullptr, w, w);

            if (!ctx->exec_pixelshader_data(g_hald, sizeof(g_hald), nullptr, nullptr, 0, &level, sizeof(level), nullptr,
                                            nullptr)) {
                aul::Logger::Error(L"Failed to execute pixel shader");
                return false;
            }

            if (w % 2 != ctx->scene->width % 2) {
                ox = 0.5f;
            }

            if (w % 2 != ctx->scene->height % 2) {
                oy = 0.5f;
            }

            break;
        }
        case LUTFormat::kStrip: {
            uint32_t size = static_cast<uint32_t>(prop::strip::size.value);
            const int w = size * size, h = size;

            if (w > ctx->scene->width || h > ctx->scene->height) {
                aul::Logger::Warning(L"LUT size exceeds scene size");
            }

            if (should_pass) {
                return true;
            }

            ctx->set_image_data(nullptr, w, h);

            if (!ctx->exec_pixelshader_data(g_strip, sizeof(g_strip), nullptr, nullptr, 0, &size, sizeof(size), nullptr,
                                            nullptr)) {
                aul::Logger::Error(L"Failed to execute pixel shader");
                return false;
            }

            if (w % 2 != ctx->scene->width % 2) {
                ox = 0.5f;
            }

            if (h % 2 != ctx->scene->height % 2) {
                oy = 0.5f;
            }

            break;
        }
        default:
            std::unreachable();
    }

    OBJECT_IMAGE_PARAM xform;
    ctx->get_output_image_param(nullptr, 0.0, &xform, sizeof(xform));

    if (std::abs(xform.sx * xform.sy * xform.sz * xform.alpha) < kEpsilon) {
        return false;
    }

    ctx->param->cx = -xform.cx, ctx->param->cy = -xform.cy, ctx->param->cz = -xform.cz;
    ctx->param->x = -xform.x + ox, ctx->param->y = -xform.y + oy, ctx->param->z = -xform.z;
    ctx->param->rx = -xform.rx, ctx->param->ry = -xform.ry, ctx->param->rz = -xform.rz;
    ctx->param->sx = 1.0f / xform.sx, ctx->param->sy = 1.0f / xform.sy, ctx->param->sz = 1.0f / xform.sz;
    ctx->param->alpha = 1.0f / xform.alpha;

    ctx->set_billboard_mode(BILLBOARD_MODE::CAMERA);
    ctx->set_material_shine(0.0f);
    ctx->set_blend_mode(BLEND_MODE::NONE);

    return true;
}

FILTER_PLUGIN_TABLE info = {
    .flag = FILTER_PLUGIN_TABLE::FLAG_VIDEO,
    .name = L"NeutralLUT_K",
    .label = L"加工",
    .information = L"NeutralLUT_K v" VERSION L" by Korarei",
    .items = reinterpret_cast<void**>(props),
    .func_proc_video = Draw,
    .func_proc_audio = nullptr,
};
}  // namespace

namespace lut::filter::neutral {
void Init(HOST_APP_TABLE* host) { host->register_filter_plugin(&info); }
}  // namespace lut::filter::neutral
