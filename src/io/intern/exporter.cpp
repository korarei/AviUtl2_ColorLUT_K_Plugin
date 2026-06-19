#include "exporter.hpp"

#include <Eigen/Core>
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
namespace string = lut::string;
namespace cube = lut::cube;
namespace hald = lut::hald;

using Logger = lut::aviutl::Logger;
using Box = Eigen::AlignedBox2i;
using RGBAF16 = lut::pixel::RGBAF16;

struct DialogData {
    std::mutex mtx;
    std::wstring metadata = L"TITLE: ${STEM} / DOMAIN_MAX: 1.0 / DOMAIN_MIN: 0.0";
    std::wstring title = L"${STEM}";
};

DialogData dialog_data{};

INT_PTR CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wp, [[maybe_unused]] LPARAM lp) {
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

                std::wstring tmp(len, L'\0');
                GetDlgItemText(hwnd, IDC_EXPORT_TITLE_EDIT, tmp.data(), len + 1);

                std::wstring title;
                title.reserve(tmp.size());

                for (wchar_t c : tmp) {
                    if (!std::iswcntrl(c)) {
                        title.push_back(c);
                    }
                }

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

bool SettingDialog(HWND hwnd, HINSTANCE hinst) {
    if (hinst == nullptr) {
        GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCWSTR>(&SettingDialog), &hinst);
    }

    const auto result = DialogBoxParam(hinst, MAKEINTRESOURCEW(IDD_EXPORT_DIALOG), hwnd, DlgProc, NULL);

    if (result != IDOK) {
        return false;
    }

    return true;
}

constexpr bool ExportLUT(OUTPUT_INFO* ctx) {
    constexpr DWORD format = MAKEFOURCC('H', 'F', '6', '4');
    constexpr std::wstring_view kPlaceholder = L"${STEM}";

    const auto* raw = static_cast<const RGBAF16*>(ctx->func_get_video(0, format));

    if (raw == nullptr) {
        Logger::Error(L"Failed to get image data");
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
        Logger::Error(L"Unsupported resolution");
        return false;
    }

    Eigen::Vector2i size = box.sizes() + Eigen::Vector2i::Ones();

    if (size.x() != size.y()) {
        Logger::Error(L"Unsupported resolution");
        return false;
    }

    hald::LUT hald{};
    hald.level = static_cast<uint32_t>(std::round(std::cbrt(static_cast<double>(size.x()))));
    hald.size = hald.level * hald.level;

    if (size.x() != static_cast<int>(hald.size * hald.level)) {
        Logger::Error(L"Unsupported resolution");
        return false;
    }

    hald.data.resize(size.x() * size.y());

    std::atomic_bool is_invalid = false;
    Eigen::Vector2i origin = box.min();

    const auto indices = std::views::iota(0uz, hald.data.size());
    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(), [&](size_t i) {
        const auto x = (i % size.x()) + origin.x();
        const auto y = (i / size.x()) + origin.y();
        const auto& v = raw[x + y * ctx->w];

        if (const float alpha = static_cast<float>(v.w()); alpha > 0.999f && alpha < 1.001f) {
            hald.data[i] = v;
        } else {
            is_invalid.store(true, std::memory_order_relaxed);
        }
    });

    if (is_invalid.load(std::memory_order_relaxed)) {
        Logger::Error(L"Only fully opaque pixels are supported");
        return false;
    }

    auto path = std::filesystem::path(ctx->savefile);
    auto ext = path.extension().wstring();

    if (ext.empty()) {
        Logger::Warning(L"File extension not specified. Appending '.cube'");
        path.replace_extension(L".cube");
        ext = L".cube";
    } else {
        std::ranges::for_each(ext, [](wchar_t& c) { c = std::towlower(c); });
    }

    const auto stem = path.stem().wstring();
    std::wstring title;

    {
        std::lock_guard<std::mutex> lock(dialog_data.mtx);

        title.reserve(dialog_data.title.size());

        std::wstring_view sv{dialog_data.title};
        size_t pos = 0;

        while (pos < sv.size()) {
            const auto st = sv.find(kPlaceholder, pos);

            if (st == std::wstring_view::npos) {
                title.append(sv.substr(pos));
                break;
            }

            title.append(sv.substr(pos, st - pos));
            title.append(stem);

            pos = st + kPlaceholder.size();
        }
    }

    try {
        if (ext == L".png") {
            return hald::Export(hald, path, title);
        } else if (ext == L".cube") {
            if (!title.empty() &&
                std::ranges::any_of(title, [](wchar_t c) { return c == 0x22 || c < 0x20 || c > 0x7e; })) {
                Logger::Error(L"Title contains characters other than printable ASCII characters");
                return false;
            }

            return cube::Export(hald, path, title);
        } else {
            Logger::Error(L"Unsupported file extension");
            return false;
        }
    } catch (const std::exception& e) {
        Logger::Error(string::ToWstring(string::AsUtf8(e.what())));
        return false;
    }
}

constexpr const wchar_t* Metadata() {
    std::lock_guard<std::mutex> lock(dialog_data.mtx);

    dialog_data.metadata = std::format(L"TITLE: {} / DOMAIN_MAX: 1.0 / DOMAIN_MIN: 0.0", dialog_data.title);

    return dialog_data.metadata.c_str();
}

constexpr bool LoadConfig(PROJECT_FILE* ctx) {
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

constexpr bool SaveConfig(PROJECT_FILE* ctx) {
    {
        std::lock_guard<std::mutex> lock(dialog_data.mtx);

        ctx->set_param_string("Export::Title", string::AsString(string::ToUtf8(dialog_data.title)).c_str());
    }

    return true;
}

constinit OUTPUT_PLUGIN_TABLE info = {
    .flag = OUTPUT_PLUGIN_TABLE::FLAG_IMAGE | OUTPUT_PLUGIN_TABLE::FLAG_PROJECT_CONFIG,
    .name = L"LUT ファイル出力",
    .filefilter = L"Cube LUT File (*.cube)\0*.cube\0Hald CLUT File (*.png)\0*.png\0\0",
    .information = L"LUT ファイル出力 v" VERSION L" by Korarei",
    .func_output = ExportLUT,
    .func_config = SettingDialog,
    .func_get_config_text = Metadata,
    .func_load_project_config = LoadConfig,
    .func_save_project_config = SaveConfig,
};
}  // namespace

namespace lut::io::exporter {
void Init(HOST_APP_TABLE* host) { host->register_output_plugin(&info); }

void Deinit() {
    {
        std::lock_guard<std::mutex> lock(dialog_data.mtx);

        std::wstring{}.swap(dialog_data.metadata);
        std::wstring{}.swap(dialog_data.title);
    }
}
}  // namespace lut::io::exporter
