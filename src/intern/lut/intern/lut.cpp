#include "lut.hpp"

#include <charconv>
#include <cmath>
#include <execution>
#include <fstream>
#include <sstream>

#include <wincodec.h>
#include <wrl/client.h>

#include "../../utilities.hpp"
#include "../../wic/wic.hpp"

#define HR(expr)                             \
    do {                                     \
        HRESULT hr__ = (expr);               \
        if (FAILED(hr__))                    \
            throw std::runtime_error(#expr); \
    } while (0)

namespace {
using Microsoft::WRL::ComPtr;
}  // namespace

namespace lut {
bool
CubeLUT::load(const std::filesystem::path &path) {
    constexpr float eps = 1.0e-4f;

    std::ifstream file(path);
    if (!file.is_open())
        return false;

    auto is_number = [](const std::string &s, float &v) -> bool {
        const char *first = s.data();
        const char *last = first + s.size();
        auto [ptr, ec] = std::from_chars(first, last, v);
        return ptr == last && ec == std::errc{};
    };

    size = 0u;
    std::vector<float>{}.swap(data);

    pixel::RGBF32 domain_min = {0.0f, 0.0f, 0.0f};
    pixel::RGBF32 domain_max = {1.0f, 1.0f, 1.0f};
    uint32_t capacity = 0u;
    std::vector<pixel::RGBF32> tmp;

    std::string line;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream iss(line);
        std::string token;
        iss >> token;

        if (token == "TITLE") {
            ;  // 無視
        } else if (token == "DOMAIN_MIN") {
            if (!(iss >> domain_min.r >> domain_min.g >> domain_min.b))
                return false;
        } else if (token == "DOMAIN_MAX") {
            if (!(iss >> domain_max.r >> domain_max.g >> domain_max.b))
                return false;
        } else if (token == "LUT_1D_SIZE") {
            dimension = 1;
            iss >> size;
            if (size < 2u || size > 65536u)
                return false;

            capacity = size;
        } else if (token == "LUT_3D_SIZE") {
            dimension = 3;
            iss >> size;
            if (size < 2u || size > 256u)
                return false;

            capacity = size * size * size;
        } else {
            float r, g, b;
            if (is_number(token, r) && size != 0u && (iss >> g >> b)) {
                tmp.reserve(capacity);
                tmp.push_back({r, g, b});
                break;
            } else {
                return false;
            }
        }
    }

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream iss(line);
        float r, g, b;
        if (iss >> r >> g >> b)
            tmp.push_back({r, g, b});
    }

    if (tmp.size() != capacity)
        return false;

    pixel::RGBF32 range = {domain_max.r - domain_min.r, domain_max.g - domain_min.g, domain_max.b - domain_min.b};
    if (range.r < eps || range.g < eps || range.b < eps)
        return false;

    pixel::RGBF32 step = {1.0f / range.r, 1.0f / range.g, 1.0f / range.b};

    auto normalize = [&](pixel::RGBF32 &elem) {
        elem.r = (elem.r - domain_min.r) * step.r;
        elem.g = (elem.g - domain_min.g) * step.g;
        elem.b = (elem.b - domain_min.b) * step.b;
    };

    data.resize(capacity * 3u);
    const auto st = tmp.data();

    switch (dimension) {
        case 1:
            std::for_each(std::execution::par_unseq, tmp.begin(), tmp.end(), [&](auto &elem) {
                normalize(elem);

                const size_t i = &elem - st;
                data[i] = elem.r;
                data[i + size] = elem.g;
                data[i + size * 2u] = elem.b;
            });
            break;
        case 3:
            std::for_each(std::execution::par_unseq, tmp.begin(), tmp.end(), [&](auto &elem) {
                normalize(elem);

                const size_t i = (&elem - st) * 3uz;
                data[i] = elem.r;
                data[i + 1u] = elem.g;
                data[i + 2u] = elem.b;
            });
            break;
        default:
            return false;
    }

    return true;
}

bool
HaldCLUT::load(const std::filesystem::path &path) {
    using Microsoft::WRL::ComPtr;

    if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path))
        return false;

    auto factory = wic::WIC::get_factory();

    auto filename = path.wstring();
    ComPtr<IWICBitmapDecoder> decoder;
    HR(factory->CreateDecoderFromFilename(filename.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand,
                                          &decoder));

    ComPtr<IWICBitmapFrameDecode> frame;
    HR(decoder->GetFrame(0u, &frame));

    WICPixelFormatGUID format;
    HR(frame->GetPixelFormat(&format));

    ComPtr<IWICComponentInfo> info;
    HR(factory->CreateComponentInfo(format, &info));

    ComPtr<IWICPixelFormatInfo2> pixel_info;
    HR(info.As(&pixel_info));

    WICPixelFormatNumericRepresentation repr;
    HR(pixel_info->GetNumericRepresentation(&repr));

    const bool is_hdr =
            repr == WICPixelFormatNumericRepresentationFloat || repr == WICPixelFormatNumericRepresentationFixed;
    const auto target = is_hdr ? GUID_WICPixelFormat128bppRGBAFloat : GUID_WICPixelFormat64bppRGBA;

    ComPtr<IWICFormatConverter> converter;
    HR(factory->CreateFormatConverter(&converter));
    HR(converter->Initialize(frame.Get(), target, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom));

    uint32_t w, h;
    HR(converter->GetSize(&w, &h));
    level = static_cast<uint32_t>(std::round(std::cbrt(static_cast<double>(w))));

    if (w != h || w < 8u || w != level * level * level)
        return false;

    const size_t size = w * h;
    data.resize(size);

    if (is_hdr) {
        const uint32_t stride = w * sizeof(pixel::RGBAF32);
        HR(converter->CopyPixels(nullptr, stride, stride * h, reinterpret_cast<BYTE *>(data.data())));
    } else {
        std::vector<pixel::RGBA16> tmp(size);
        const uint32_t stride = w * sizeof(pixel::RGBA16);
        HR(converter->CopyPixels(nullptr, stride, stride * h, reinterpret_cast<BYTE *>(tmp.data())));
        pixel::to_rgbaf32(data.data(), tmp.data(), w, h);
    }

    return true;
}

bool
HaldCLUT::export_cube(const std::filesystem::path &path, const std::u8string &title) const {
    size_t size = level * level * level;
    size *= size;
    if (level < 2u || data.size() != size)
        return false;

    if (auto parent = path.parent_path(); !parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec)
            return false;
    }

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open())
        return false;

    auto saturate = [](float v) -> float { return std::clamp(v, 0.0f, 1.0f); };

    file << "# Generated by ColorLUT_K.aux2\n";
    if (!title.empty())
        file << "TITLE \"" << string::as_string(title) << "\"\n";

    file << "LUT_3D_SIZE " << level * level << "\n";

    for (size_t i = 0; i < size; ++i) {
        const auto &pixel = data[i];
        file << saturate(pixel.r) << " " << saturate(pixel.g) << " " << saturate(pixel.b) << "\n";
    }

    return true;
}

bool
HaldCLUT::export_png(const std::filesystem::path &path, const std::u8string &title) const {
    WICPixelFormatGUID format = GUID_WICPixelFormat48bppRGB;

    uint32_t size = level * level * level;
    size_t capacity = size * size;
    if (level < 2u || data.size() != capacity)
        return false;

    if (auto parent = path.parent_path(); !parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec)
            return false;
    }

    auto factory = wic::WIC::get_factory();

    ComPtr<IWICBitmapEncoder> encoder;
    HR(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder));

    ComPtr<IWICStream> stream;
    HR(factory->CreateStream(&stream));
    HR(stream->InitializeFromFilename(path.wstring().c_str(), GENERIC_WRITE));

    HR(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache));

    ComPtr<IWICBitmapFrameEncode> frame;
    HR(encoder->CreateNewFrame(&frame, nullptr));
    HR(frame->Initialize(nullptr));

    HR(frame->SetSize(size, size));
    HR(frame->SetPixelFormat(&format));

    ComPtr<IWICMetadataQueryWriter> writer;
    HR(frame->GetMetadataQueryWriter(&writer));
    PROPVARIANT value;
    PropVariantInit(&value);
    value.vt = VT_LPWSTR;

    value.pwszVal = _wcsdup(L"Created by ColorLUT_K.aux2");
    writer->SetMetadataByName(L"/tEXt/{str=Software}", &value);
    free(value.pwszVal);

    auto name = string::to_wstring(title);
    value.pwszVal = _wcsdup(name.c_str());
    writer->SetMetadataByName(L"/tEXt/{str=Title}", &value);
    free(value.pwszVal);

    value.pwszVal = nullptr;
    PropVariantClear(&value);

    uint32_t stride = size * 6u;
    std::vector<pixel::RGB16> tmp(capacity);
    pixel::to_rgb16(tmp.data(), data.data(), size, size);

    HR(frame->WritePixels(size, stride, stride * size, reinterpret_cast<BYTE *>(tmp.data())));
    HR(frame->Commit());
    HR(encoder->Commit());

    return true;
}
}  // namespace lut
