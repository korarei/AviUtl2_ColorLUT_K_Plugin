#include "exporter.hpp"

#include <algorithm>
#include <cwctype>
#include <execution>
#include <filesystem>
#include <format>
#include <mutex>
#include <ranges>
#include <string>

#include <Eigen/Dense>

#include <output2.h>

#include <intern/resource.h>
#include <intern/aviutl/aviutl.hpp>
#include <intern/lut/lut.hpp>
#include <intern/pixel/pixel.hpp>
#include <intern/string.hpp>

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3) \
    ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) | ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24))
#endif

#ifndef VERSION
#define VERSION L"0.1.0"
#endif

namespace {
namespace aul = lut::aviutl;
namespace string = lut::string;

using CubeLUT = lut::CubeLUT;
using HaldLUT = lut::HaldLUT;
using StripLUT = lut::StripLUT;

struct DialogData {
    std::mutex mtx;
    std::wstring title{};
};

constinit DialogData dialog_data{};

[[nodiscard]] std::wstring ResolveTitle(std::wstring_view input, std::wstring_view stem) {
    constexpr std::wstring_view kTarget = L"${STEM}";

    std::wstring title{};
    title.reserve(input.size());

    size_t pos = 0;

    while (pos < input.size()) {
        const auto st = input.find(kTarget, pos);

        if (st == std::wstring_view::npos) {
            title.append(input.substr(pos));
            break;
        }

        title.append(input.substr(pos, st - pos));
        title.append(stem);

        pos = st + kTarget.size();
    }

    return title;
}

INT_PTR CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM) {
    switch (msg) {
        case WM_INITDIALOG: {
            {
                std::lock_guard<std::mutex> lock(dialog_data.mtx);

                SetDlgItemText(hwnd, IDC_EXPORT_TITLE_EDIT, dialog_data.title.c_str());
            }

            SetFocus(GetDlgItem(hwnd, IDOK));
            return FALSE;
        }
        case WM_COMMAND:
            if (LOWORD(wp) == IDOK) {
                const int len = GetWindowTextLength(GetDlgItem(hwnd, IDC_EXPORT_TITLE_EDIT));

                std::wstring title(len + 1, L'\0');
                GetDlgItemText(hwnd, IDC_EXPORT_TITLE_EDIT, title.data(), len + 1);
                title.resize(len);

                std::erase_if(title, [](wchar_t c) { return std::iswcntrl(c); });

                {
                    std::lock_guard<std::mutex> lock(dialog_data.mtx);

                    dialog_data.title = std::move(title);
                }

                EndDialog(hwnd, IDOK);
                return TRUE;
            }
            break;
        case WM_CLOSE:
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
    }

    return FALSE;
}

bool ShowExportDialog(HWND hwnd, HINSTANCE hinst) {
    return DialogBoxParam(hinst, MAKEINTRESOURCEW(IDD_EXPORT_DIALOG), hwnd, DlgProc, NULL) == IDOK;
}

bool ExportLUT(OUTPUT_INFO* ctx) {
    using Box = Eigen::AlignedBox2i;
    using RGBAF16 = lut::pixel::RGBAF16;

    constexpr DWORD format = MAKEFOURCC('H', 'F', '6', '4');

    const auto* raw = static_cast<const RGBAF16*>(ctx->func_get_video(0, format));

    if (raw == nullptr) {
        aul::Logger::Error(L"Failed to get image data");
        return false;
    }

    const auto rows = std::views::iota(0, ctx->h);
    const Box box = std::transform_reduce(
        std::execution::par_unseq, rows.begin(), rows.end(), Box{},
        [](const Box& a, const Box& b) {
            Box tmp = a;
            return tmp.extend(b);
        },
        [&](int y) -> Box {
            Box box{};

            const auto* row = raw + y * ctx->w;
            for (int x = 0; x < ctx->w; ++x) {
                if (const float alpha = static_cast<float>(row[x].w()); alpha > 0.999f && alpha < 1.001f) {
                    box.extend(Eigen::Vector2i(x, y));
                }
            }

            return box;
        });

    if (box.isEmpty()) {
        aul::Logger::Error(L"Bounding box has no area");
        return false;
    }

    const Eigen::Vector2i size = box.sizes() + Eigen::Vector2i::Ones();

    std::vector<RGBAF16> data(size.x() * size.y());

    const Eigen::Vector2i origin = box.min();

    const auto indices = std::views::iota(0uz, data.size());
    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(), [&](size_t i) {
        const auto x = (i % size.x()) + origin.x();
        const auto y = (i / size.x()) + origin.y();
        data[i] = raw[x + y * ctx->w];
    });

    auto path = std::filesystem::path(ctx->savefile);
    auto ext = path.extension().wstring();

    if (ext.empty()) {
        aul::Logger::Warning(L"File extension not specified. Appending '.cube'");
        path.replace_extension(L".cube");
        ext = L".cube";
    } else {
        std::ranges::for_each(ext, [](wchar_t& c) { c = std::towlower(c); });
    }

    const auto stem = path.stem().wstring();
    std::wstring title;

    {
        std::lock_guard<std::mutex> lock(dialog_data.mtx);

        title = ResolveTitle(dialog_data.title, stem);
    }

    try {
        if (size.x() == size.y()) {
            const uint32_t level = static_cast<uint32_t>(std::lround(std::cbrt(static_cast<double>(size.x()))));

            const auto hald = HaldLUT::Init(level, std::move(data));

            if (!hald.has_value()) {
                aul::Logger::Error(L"Not a valid Hald CLUT format");
                return false;
            }

            if (ext == L".png") {
                if (!hald->Export(path, title)) {
                    aul::Logger::Error(L"Failed to export Hald CLUT");
                    return false;
                }

                return true;
            } else if (ext == L".cube") {
                if (!title.empty() &&
                    std::ranges::any_of(title, [](wchar_t c) { return c == 0x22 || c < 0x20 || c > 0x7e; })) {
                    aul::Logger::Error(L"Title contains characters other than printable ASCII characters");
                    return false;
                }

                if (!CubeLUT::Init(*hald).Export(path, title)) {
                    aul::Logger::Error(L"Failed to export Cube LUT");
                    return false;
                }

                return true;
            } else {
                aul::Logger::Error(L"Unsupported file extension");
                return false;
            }
        } else {
            const auto strip = StripLUT::Init(size.y(), std::move(data));

            if (!strip.has_value()) {
                aul::Logger::Error(L"Not a valid Strip LUT format");
                return false;
            }

            if (ext == L".png") {
                const auto hald = HaldLUT::Init(*strip);

                if (!hald.has_value()) {
                    aul::Logger::Error(L"Not a valid Hald CLUT format");
                    return false;
                }

                if (!hald->Export(path, title)) {
                    aul::Logger::Error(L"Failed to export Hald CLUT");
                    return false;
                }

                return true;
            } else if (ext == L".cube") {
                if (!title.empty() &&
                    std::ranges::any_of(title, [](wchar_t c) { return c == 0x22 || c < 0x20 || c > 0x7e; })) {
                    aul::Logger::Error(L"Title contains characters other than printable ASCII characters");
                    return false;
                }

                if (!CubeLUT::Init(*strip).Export(path, title)) {
                    aul::Logger::Error(L"Failed to export Cube LUT");
                    return false;
                }

                return true;
            } else {
                aul::Logger::Error(L"Unsupported file extension");
                return false;
            }
        }
    } catch (const std::exception& e) {
        aul::Logger::Error(string::ToWstring(string::AsUtf8(e.what())));
        return false;
    }
}

std::wstring& Metadata() {
    static std::wstring metadata{};

    std::lock_guard<std::mutex> lock(dialog_data.mtx);

    metadata = std::format(L"TITLE: {} / DOMAIN_MAX: 1.0 / DOMAIN_MIN: 0.0", dialog_data.title);

    return metadata;
}

bool LoadConfig(PROJECT_FILE* ctx) {
    const auto title = ctx->get_param_string("Export::Title");

    if (title == nullptr) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(dialog_data.mtx);

        dialog_data.title = string::ToWstring(string::AsUtf8(title));
    }

    return true;
}

bool SaveConfig(PROJECT_FILE* ctx) {
    {
        std::lock_guard<std::mutex> lock(dialog_data.mtx);

        ctx->set_param_string("Export::Title", string::AsString(string::ToUtf8(dialog_data.title)).c_str());
    }

    return true;
}

OUTPUT_PLUGIN_TABLE info = {
    .flag = OUTPUT_PLUGIN_TABLE::FLAG_IMAGE | OUTPUT_PLUGIN_TABLE::FLAG_PROJECT_CONFIG,
    .name = L"LUT ファイル出力",
    .filefilter = L"Cube LUT File (*.cube)\0*.cube\0Hald CLUT File (*.png)\0*.png\0\0",
    .information = L"LUT ファイル出力 v" VERSION L" by Korarei",
    .func_output = ExportLUT,
    .func_config = ShowExportDialog,
    .func_get_config_text = []() { return Metadata().c_str(); },
    .func_load_project_config = LoadConfig,
    .func_save_project_config = SaveConfig,
};
}  // namespace

namespace lut::io::exporter {
void Init(HOST_APP_TABLE* host) {
    host->register_output_plugin(&info);

    std::lock_guard<std::mutex> lock(dialog_data.mtx);

    if (dialog_data.title.empty()) {
        dialog_data.title = L"${STEM}";
    }
}

void Deinit() {
    std::wstring{}.swap(Metadata());

    std::lock_guard<std::mutex> lock(dialog_data.mtx);

    std::wstring{}.swap(dialog_data.title);
}
}  // namespace lut::io::exporter
