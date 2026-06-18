#include "exporter.hpp"

#include <mmsystem.h>

#include <algorithm>
#include <execution>
#include <filesystem>
#include <ranges>

#include <Eigen/Dense>

#include <output2.h>

#include <intern/aviutl/aviutl.hpp>
#include <intern/lut/lut.hpp>
#include <intern/pixel/pixel.hpp>
#include <intern/string.hpp>

namespace {
namespace string = lut::string;
namespace hald = lut::hald;

using Logger = lut::aviutl::Logger;
using Box = Eigen::AlignedBox2i;
using RGBAF16 = lut::pixel::RGBAF16;

constexpr bool ExportLUT(OUTPUT_INFO* context) {
    constexpr DWORD format = MAKEFOURCC('H', 'F', '6', '4');

    Eigen::Vector2i size{context->w, context->h};

    if ((size.array() < 8).any()) {
        Logger::Error(L"Unsupported resolution");
        return false;
    }

    const auto* raw = static_cast<const RGBAF16*>(context->func_get_video(0, format));
    if (raw == nullptr) {
        Logger::Error(L"Failed to get image data");
        return false;
    }

    hald::LUT lut{};
    lut.level = static_cast<uint32_t>(std::round(std::cbrt(static_cast<double>(size.x()))));
    lut.size = lut.level * lut.level;

    if (size.x() != size.y() || size.x() != static_cast<int>(lut.size * lut.level)) {
        const size_t pitch = static_cast<size_t>(size.x());

        const auto rows = std::views::iota(0, size.y());
        const Box box = std::transform_reduce(
            std::execution::par_unseq, rows.begin(), rows.end(), Box{},
            [](const Box& a, const Box& b) {
                Box tmp = a;
                return tmp.extend(b);
            },
            [&](int y) -> Box {
                Box box{};

                const auto* row = raw + y * size.x();
                for (int x = 0; x < size.x(); ++x) {
                    if (const float alpha = static_cast<float>(row[x].w()); alpha > 0.999f && alpha < 1.001f) {
                        box.extend(Eigen::Vector2i(x, y));
                    }
                }

                return box;
            });

        if (box.isEmpty()) {
            Logger::Error(L"Unsupported resolution");
            return false;
        }

        size = box.sizes() + Eigen::Vector2i::Ones();

        if (size.x() != size.y()) {
            Logger::Error(L"Unsupported resolution");
            return false;
        }

        lut.level = static_cast<uint32_t>(std::round(std::cbrt(static_cast<double>(size.x()))));
        lut.size = lut.level * lut.level;

        if (size.x() != static_cast<int>(lut.size * lut.level)) {
            Logger::Error(L"Unsupported resolution");
            return false;
        }

        lut.data.resize(size.x() * size.y());

        std::atomic_bool is_invalid = false;
        Eigen::Vector2i origin = box.min();

        const auto indices = std::views::iota(0uz, lut.data.size());
        std::for_each(std::execution::par_unseq, indices.begin(), indices.end(), [&](size_t i) {
            const auto x = (i % size.x()) + origin.x();
            const auto y = (i / size.x()) + origin.y();
            const auto& v = raw[x + y * pitch];

            if (const float alpha = static_cast<float>(v.w()); alpha > 0.999f && alpha < 1.001f) {
                lut.data[i] = v;
            } else {
                is_invalid.store(true, std::memory_order_relaxed);
            }
        });

        if (is_invalid.load(std::memory_order_relaxed)) {
            Logger::Error(L"Invalid image data");
            return false;
        }
    } else {
        lut.data.resize(size.x() * size.y());

        std::atomic_bool is_invalid = false;

        const auto indices = std::views::iota(0uz, lut.data.size());
        std::for_each(std::execution::par_unseq, indices.begin(), indices.end(), [&](size_t i) {
            if (const float alpha = static_cast<float>(raw[i].w()); alpha > 0.999f && alpha < 1.001f) {
                lut.data[i] = raw[i];
            } else {
                is_invalid.store(true, std::memory_order_relaxed);
            }
        });

        if (is_invalid.load(std::memory_order_relaxed)) {
            Logger::Error(L"Invalid image data");
            return false;
        }
    }

    auto path = std::filesystem::path(context->savefile);
    const auto title = path.stem().wstring();

    if (path.extension().empty()) {
        Logger::Warning(L"File extension not specified. Appending '.cube'");
        path.replace_extension(L".cube");
    }

    try {
        return hald::Export(lut, path, title);
    } catch (const std::exception& e) {
        Logger::Error(string::ToWstring(string::AsUtf8(e.what())));
        return false;
    }
}

constexpr const wchar_t* Metadata() { return L"TITLE: {STEM} / DOMAIN_MAX: 1.0 / DOMAIN_MIN: 0.0"; }

constinit OUTPUT_PLUGIN_TABLE info = {
    .flag = OUTPUT_PLUGIN_TABLE::FLAG_IMAGE,
    .name = L"LUT ファイル出力",
    .filefilter = L"Cube LUT File (*.cube)\0*.cube\0Hald CLUT File (*.png)\0*.png\0\0",
    .information = L"Export Cube LUT or Hald CLUT",
    .func_output = ExportLUT,
    .func_config = nullptr,
    .func_get_config_text = Metadata,
    .func_load_project_config = nullptr,
    .func_save_project_config = nullptr,
};
}  // namespace

namespace lut::io::exporter {
void Init(HOST_APP_TABLE* host) { host->register_output_plugin(&info); }
}  // namespace lut::io::exporter
