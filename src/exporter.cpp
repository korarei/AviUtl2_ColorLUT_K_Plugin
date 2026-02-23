#include "exporter.hpp"

#include <immintrin.h>
#include <algorithm>
#include <execution>
#include <filesystem>
#include <ranges>

#include <vfw.h>

#include "lut.hpp"
#include "pixel.hpp"

namespace {
constinit LOG_HANDLE *logger = nullptr;

bool
hald2cube(OUTPUT_INFO *info) {
    constexpr DWORD format = MAKEFOURCC('H', 'F', '6', '4');

    HaldLUT hald{};

    const auto level = static_cast<int>(std::round(std::cbrt(static_cast<double>(info->w))));
    if (info->w != info->h || info->w < 8 || info->w != level * level * level) {
        logger->error(logger, L"Invalid HaldCLUT size.");
        return false;
    }

    hald.level = level;
    hald.w = info->w;
    hald.h = info->h;
    hald.data.resize(info->w * info->h);

    const auto *data = static_cast<const RGBAF16 *>(info->func_get_video(0, format));
    if (data == nullptr) {
        logger->error(logger, L"Failed to get image data.");
        return false;
    }

    const auto idx = std::views::iota(0, info->h);
    std::for_each(std::execution::par, idx.begin(), idx.end(), [&](int y) {
        const RGBAF16 *src = data + y * info->w;
        RGBAF32 *dst = &hald.data[y * info->w];

        int x = 0;
        while (x + 2 <= info->w) {
            __m128i h = _mm_loadu_si128(reinterpret_cast<const __m128i *>(src + x));
            __m256 f = _mm256_cvtph_ps(h);
            _mm256_storeu_ps(reinterpret_cast<float *>(dst + x), f);
            x += 2;
        }

        while (x < info->w) {
            __m128i h = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(&src[x]));
            __m128 f = _mm_cvtph_ps(h);
            _mm_storeu_ps(reinterpret_cast<float *>(&dst[x]), f);
            ++x;
        }
    });

    auto path = std::filesystem::path(info->savefile);
    return hald.save(path, path.stem().u8string());
}

bool
func_output(OUTPUT_INFO *info) {
    return hald2cube(info);
}

const wchar_t *
func_get_config_text() {
    return L"TITLE: Filename / DOMAIN_MAX: 1.0 / DOMAIN_MIN: 0.0";
}
}  // namespace

OUTPUT_PLUGIN_TABLE exporter::info = {
        .flag = OUTPUT_PLUGIN_TABLE::FLAG_IMAGE,
        .name = L"LUTファイル出力",
        .filefilter = L"Cube LUT File (*.cube)\0*.cube\0\0",
        .information = L"Export Cube LUT from HaldCLUT.",
        .func_output = func_output,
        .func_config = nullptr,
        .func_get_config_text = func_get_config_text,
};

void
exporter::initialize_logger(LOG_HANDLE *log) {
    logger = log;
}
