#include "exporter.hpp"

#include <immintrin.h>
#include <algorithm>
#include <cstddef>
#include <execution>
#include <filesystem>
#include <format>
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

    int w = info->w, h = info->h;

    int level = static_cast<int>(std::round(std::cbrt(static_cast<double>(w))));
    if (w < 8 || h < 8) {
        logger->error(logger, L"Too small HaldCLUT size.");
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
        const size_t pitch = w;

        std::vector<RGBAF32> tmp(w * h);
        to_rgbaf32(tmp.data(), data, w, h);

        int top = 0, bottom = h - 1, left = 0, right = w - 1;

        auto is_valid = [&](int x, int y) {
            const float a = tmp[x + y * w].a;
            return a >= 0.999f && a <= 1.001f;
        };

        auto future = std::async(std::launch::async, [&]() {
            bool flag = false;
            for (int y = 0; y < h && !flag; ++y) {
                for (int x = 0; x < w; ++x) {
                    if (is_valid(x, y)) {
                        top = y;
                        flag = true;
                        break;
                    }
                }
            }
        });

        {
            bool flag = false;
            for (int y = h - 1; y >= 0 && !flag; --y) {
                for (int x = w - 1; x >= 0; --x) {
                    if (is_valid(x, y)) {
                        bottom = y;
                        flag = true;
                        break;
                    }
                }
            }
        }

        future.get();

        future = std::async(std::launch::async, [&]() {
            bool flag = false;
            for (int x = 0; x < w && !flag; ++x) {
                for (int y = top; y <= bottom; ++y) {
                    if (is_valid(x, y)) {
                        left = x;
                        flag = true;
                        break;
                    }
                }
            }
        });

        {
            bool flag = false;
            for (int x = w - 1; x >= 0 && !flag; --x) {
                for (int y = bottom; y >= top; --y) {
                    if (is_valid(x, y)) {
                        right = x;
                        flag = true;
                        break;
                    }
                }
            }
        }

        future.get();

        w = right - left + 1, h = bottom - top + 1;
        level = static_cast<int>(std::round(std::cbrt(static_cast<double>(w))));

        if (w != h || w != level * level * level) {
            logger->error(logger, std::format(L"Invalid HaldCLUT size: {}x{}", w, h).c_str());
            return false;
        }

        hald.level = static_cast<uint32_t>(level);
        hald.data.resize(w * h);
        const auto st = hald.data.data();

        std::for_each(std::execution::par_unseq, hald.data.begin(), hald.data.end(), [&](auto &elem) {
            const size_t i = &elem - st;
            const auto x = (i % w) + left;
            const auto y = (i / w) + top;
            elem = tmp[x + y * pitch];
        });
    } else {
        hald.level = static_cast<uint32_t>(level);
        hald.data.resize(w * h);
        to_rgbaf32(hald.data.data(), data, w, h);
    }

    const auto path = std::filesystem::path(info->savefile);
    const auto title = path.stem().u8string();
    return hald.export_cube(path, title);
}

constexpr const wchar_t *
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
init(LOG_HANDLE *handle) noexcept {
    logger = handle;
}
}  // namespace lut::io::exporter
