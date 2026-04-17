#include "exporter.hpp"

#include <immintrin.h>
#include <algorithm>
#include <execution>
#include <filesystem>
#include <future>

#include <mmsystem.h>

#include <lut.hpp>
#include <pixel.hpp>

namespace {
using namespace lut;
using namespace pixel;

constinit LOG_HANDLE *logger = nullptr;

bool
export_lut(OUTPUT_INFO *info) {
    constexpr DWORD format = MAKEFOURCC('H', 'F', '6', '4');

    size_t w = info->w, h = info->h;

    size_t level = static_cast<size_t>(std::round(std::cbrt(static_cast<double>(w))));
    if (w < 8uz || h < 8uz) {
        logger->error(logger, L"Invalid HaldCLUT size.");
        return false;
    }

    const auto *data = static_cast<const RGBAF16 *>(info->func_get_video(0, format));
    if (data == nullptr) {
        logger->error(logger, L"Failed to get image data.");
        return false;
    }

    HaldCLUT hald{};

    // サイズがおかしい時はリサイズを試みる (AutoClippingの閾値が1.0なやつ)
    if (w != h || w != level * level * level) {
        std::vector<RGBAF32> tmp(w * h);
        to_rgbaf32(tmp.data(), data, w, h);

        size_t edges[4] = {0uz, w - 1uz, 0uz, h - 1uz};

        auto is_invalid = [&](size_t x, size_t y) {
            const float a = tmp[x + y * w].a;
            return a < 0.9999f || a > 1.0001f;
        };

        auto future = std::async(std::launch::async, [&]() {
            for (size_t y = 0uz; y < h; ++y) {
                bool flag = true;
                for (size_t x = 0uz; x < w; ++x) {
                    if (is_invalid(x, y)) {
                        flag = false;
                        break;
                    }
                }

                if (flag) {
                    edges[0] = y;
                    break;
                }
            }
        });

        for (size_t y = h - 1uz; y < h; --y) {
            bool flag = true;
            for (size_t x = w - 1uz; x < w; --x) {
                if (is_invalid(x, y)) {
                    flag = false;
                    break;
                }
            }

            if (flag) {
                edges[1] = y;
                break;
            }
        }

        future.get();

        future = std::async(std::launch::async, [&]() {
            for (size_t x = 0uz; x < w; ++x) {
                bool flag = true;
                for (size_t y = edges[0]; y <= edges[1]; ++y) {
                    if (is_invalid(x, y)) {
                        flag = false;
                        break;
                    }
                }

                if (flag) {
                    edges[2] = x;
                    break;
                }
            }
        });

        for (size_t x = w - 1uz; x < w; --x) {
            bool flag = true;
            for (size_t y = edges[0]; y <= edges[1]; ++y) {
                if (is_invalid(x, y)) {
                    flag = false;
                    break;
                }
            }

            if (flag) {
                edges[3] = x;
                break;
            }
        }

        future.get();

        w = edges[3] - edges[2] + 1uz, h = edges[1] - edges[0] + 1uz;
        level = static_cast<size_t>(std::round(std::cbrt(static_cast<double>(w))));

        if (w != h || w != level * level * level) {
            logger->error(logger, L"Invalid HaldCLUT size.");
            return false;
        }

        const size_t x = edges[0], y = edges[2];

        hald.level = static_cast<uint32_t>(level);
        hald.w = static_cast<uint32_t>(w), hald.h = static_cast<uint32_t>(h);
        hald.data.resize(w * h);
        const auto st = hald.data.data();

        std::for_each(std::execution::par_unseq, hald.data.begin(), hald.data.end(), [&](auto &elem) {
            const size_t i = elem - st;
            elem = tmp[(i % w) + x + ((i / w) + y) * w];
        });
    } else {
        hald.level = static_cast<uint32_t>(level);
        hald.w = static_cast<uint32_t>(w), hald.h = static_cast<uint32_t>(h);
        hald.data.resize(w * h);
        to_rgbaf32(hald.data.data(), data, w, h);
    }

    const auto path = std::filesystem::path(info->savefile);
    const auto title = path.stem().u8string();
    return hald.save(path, title);
}

const wchar_t *
describe_metadata() {
    return L"TITLE: {STEM} / DOMAIN_MAX: 1.0 / DOMAIN_MIN: 0.0";
}
}  // namespace

namespace lut::io::exporter {
constinit OUTPUT_PLUGIN_TABLE info = {
        .flag = OUTPUT_PLUGIN_TABLE::FLAG_IMAGE,
        .name = L"LUTファイル出力",
        .filefilter = L"Cube LUT File (*.cube)\0*.cube\0\0",
        .information = L"Export Cube LUT from Hald CLUT.",
        .func_output = export_lut,
        .func_config = nullptr,
        .func_get_config_text = describe_metadata,
};

void
init(LOG_HANDLE *handle) {
    logger = handle;
}
}  // namespace lut::io::exporter
