#include "../lut.hpp"

#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <execution>
#include <fstream>
#include <ranges>
#include <span>
#include <spanstream>

#include <intern/string.hpp>
#include <intern/wic/wic.hpp>

#define HR(expr)                                           \
    do {                                                   \
        HRESULT hr__ = (expr);                             \
        if (FAILED(hr__)) throw std::runtime_error(#expr); \
    } while (0)

namespace {
template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;
using RGBAF16 = lut::pixel::RGBAF16;
using RGBA16 = lut::pixel::RGBA16;
}  // namespace

namespace lut {
LUTCache::LockedEntry LUTCache::Find(int64_t id, const std::wstring& path) { return Instance().Fetch(id, path); }

void LUTCache::Reset() { Instance().cache::Cache<std::optional<LUTData>>::Reset(); }

void LUTCache::Reset(int64_t id) { Instance().cache::Cache<std::optional<LUTData>>::Reset(id); }

void LUTCache::Reset(const std::wstring& path) { Instance().cache::Cache<std::optional<LUTData>>::Reset(path); }

LUTCache& LUTCache::Instance() {
    static LUTCache inst;
    return inst;
}

namespace cube {
std::optional<LUT> Load(const std::filesystem::path& path) {
    constexpr float kEpsilon = 1.0e-4f;

    std::ifstream file(path);

    if (!file.is_open()) {
        return std::nullopt;
    }

    int dimension = 0;
    uint32_t size = 0u;
    std::vector<Eigen::half> data;

    Eigen::Vector3f min{0.0f, 0.0f, 0.0f};
    Eigen::Vector3f max{1.0f, 1.0f, 1.0f};
    Eigen::Vector3f scale{1.0f, 1.0f, 1.0f};

    size_t index = 0uz, length = 0uz;
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::ispanstream iss(std::span<const char>(line.data(), line.size()));
        std::string key;
        iss >> key;

        if (key == "TITLE") {
            continue;
        } else if (key == "DOMAIN_MIN") {
            iss >> min.x() >> min.y() >> min.z();

            if (iss.fail()) {
                return std::nullopt;
            }

            iss >> std::ws;

            if (!iss.eof()) {
                return std::nullopt;
            }
        } else if (key == "DOMAIN_MAX") {
            iss >> max.x() >> max.y() >> max.z();

            if (iss.fail()) {
                return std::nullopt;
            }

            iss >> std::ws;

            if (!iss.eof()) {
                return std::nullopt;
            }
        } else if (key == "LUT_1D_SIZE") {
            if (dimension != 0) {
                return std::nullopt;
            }

            iss >> size;

            if (iss.fail()) {
                return std::nullopt;
            }

            iss >> std::ws;

            if (!iss.eof()) {
                return std::nullopt;
            }

            if (size < 2u || size > 65536u) {
                return std::nullopt;
            }

            dimension = 1;
            length = static_cast<size_t>(size);
            data.resize(length * 3uz);
        } else if (key == "LUT_3D_SIZE") {
            if (dimension != 0) {
                return std::nullopt;
            }

            iss >> size;

            if (iss.fail()) {
                return std::nullopt;
            }

            iss >> std::ws;

            if (!iss.eof()) {
                return std::nullopt;
            }

            if (size < 2u || size > 256u) {
                return std::nullopt;
            }

            dimension = 3;
            length = 4uz * size * static_cast<size_t>(size) * size;
            data.resize(length);
        } else {
            if (dimension != 1 && dimension != 3) {
                return std::nullopt;
            }

            const auto range = max - min;

            if ((range.array() < kEpsilon).any()) {
                return std::nullopt;
            }

            scale = range.cwiseInverse();

            Eigen::Vector3f v;
            iss >> v.y() >> v.z();

            if (!string::ToNumber(key, v.x()) || iss.fail()) {
                return std::nullopt;
            }

            iss >> std::ws;

            if (!iss.eof()) {
                return std::nullopt;
            }

            v = (v - min).cwiseProduct(scale);

            switch (dimension) {
                case 1:
                    data[0uz] = static_cast<Eigen::half>(v.x());
                    data[size] = static_cast<Eigen::half>(v.y());
                    data[size * 2u] = static_cast<Eigen::half>(v.z());

                    ++index;
                    break;
                case 3:
                    data[0uz] = static_cast<Eigen::half>(v.x());
                    data[1uz] = static_cast<Eigen::half>(v.y());
                    data[2uz] = static_cast<Eigen::half>(v.z());
                    data[3uz] = static_cast<Eigen::half>(1.0f);

                    index += 4uz;
                    break;
                default:
                    return std::nullopt;
            }

            break;
        }
    }

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        if (index >= length) {
            return std::nullopt;
        }

        std::ispanstream iss(std::span<const char>(line.data(), line.size()));

        Eigen::Vector3f v;
        iss >> v.x() >> v.y() >> v.z();

        if (iss.fail()) {
            return std::nullopt;
        }

        iss >> std::ws;

        if (!iss.eof()) {
            return std::nullopt;
        }

        v = (v - min).cwiseProduct(scale);

        switch (dimension) {
            case 1:
                data[index] = static_cast<Eigen::half>(v.x());
                data[index + size] = static_cast<Eigen::half>(v.y());
                data[index + size * 2u] = static_cast<Eigen::half>(v.z());
                ++index;
                break;
            case 3:
                data[index] = static_cast<Eigen::half>(v.x());
                data[index + 1uz] = static_cast<Eigen::half>(v.y());
                data[index + 2uz] = static_cast<Eigen::half>(v.z());
                data[index + 3uz] = static_cast<Eigen::half>(1.0f);
                index += 4uz;
                break;
            default:
                return std::nullopt;
        }
    }

    if (index != length) {
        return std::nullopt;
    }

    return LUT{dimension, size, std::move(data)};
}

bool Export(const LUT& lut, const std::filesystem::path& path, const std::wstring& title) {
    if (lut.dimension != 1 && lut.dimension != 3) {
        throw std::runtime_error("Invalid LUT dimension");
    }

    size_t length;

    switch (lut.dimension) {
        case 1:
            if (lut.size < 2u || lut.size > 65536u) {
                throw std::runtime_error("Invalid LUT size");
            }

            length = lut.size;

            if (lut.data.size() != 3uz * length) {
                throw std::runtime_error("Invalid LUT data size");
            }

            break;
        case 3:
            if (lut.size < 2u || lut.size > 256u) {
                throw std::runtime_error("Invalid LUT size");
            }

            length = 4uz * lut.size * static_cast<size_t>(lut.size) * lut.size;

            if (lut.data.size() != length) {
                throw std::runtime_error("Invalid LUT data size");
            }

            break;
        default:
            return false;
    }

    if (auto parent = path.parent_path(); !parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec != std::error_code{}) {
            throw std::runtime_error("Failed to create directories");
        }
    }

    std::ofstream file(path, std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file");
    }

    auto saturate = [](Eigen::half v) -> float { return std::clamp(static_cast<float>(v), 0.0f, 1.0f); };

    file << "# Generated by ColorLUT_K.aux2\n";

    if (!title.empty()) {
        file << "TITLE \"" << string::AsString(string::ToUtf8(title)) << "\"\n";
    }

    size_t index = 0uz;

    switch (lut.dimension) {
        case 1:
            file << "LUT_1D_SIZE " << lut.size << "\n";

            while (index < length) {
                file << saturate(lut.data[index]) << " " << saturate(lut.data[index + 1uz]) << " "
                     << saturate(lut.data[index + 2uz]) << "\n";

                ++index;
            }

            return true;
        case 3:
            file << "LUT_3D_SIZE " << lut.size << "\n";

            while (index < length) {
                file << saturate(lut.data[index]) << " " << saturate(lut.data[index + 1uz]) << " "
                     << saturate(lut.data[index + 2uz]) << "\n";

                index += 4uz;
            }

            return true;
        default:
            return false;
    }

    return true;
}

bool Export(const hald::LUT& lut, const std::filesystem::path& path, const std::wstring& title) {
    const uint32_t size = lut.size * lut.level;

    if (lut.level < 2u || lut.data.size() != static_cast<size_t>(size) * size) {
        throw std::runtime_error("Invalid LUT size");
    }

    if (auto parent = path.parent_path(); !parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec != std::error_code{}) {
            throw std::runtime_error("Failed to create directories");
        }
    }

    std::ofstream file(path, std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file");
    }

    auto saturate = [](Eigen::half v) -> float { return std::clamp(static_cast<float>(v), 0.0f, 1.0f); };

    file << "# Generated by ColorLUT_K.aux2\n";

    if (!title.empty()) {
        file << "TITLE \"" << string::AsString(string::ToUtf8(title)) << "\"\n";
    }

    file << "LUT_3D_SIZE " << lut.size << "\n";

    for (const auto& v : lut.data) {
        file << saturate(v.x()) << " " << saturate(v.y()) << " " << saturate(v.z()) << "\n";
    }

    return true;
}
}  // namespace cube

namespace hald {
std::optional<LUT> Load(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path)) {
        return std::nullopt;
    }

    const auto factory = wic::WIC::factory();

    if (factory == nullptr) {
        throw std::runtime_error("Failed to load WIC factory");
    }

    ComPtr<IWICBitmapDecoder> decoder;
    HR(factory->CreateDecoderFromFilename(path.wstring().c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand,
                                          &decoder));

    ComPtr<IWICBitmapFrameDecode> frame;
    HR(decoder->GetFrame(0u, &frame));

    WICPixelFormatGUID format;
    HR(frame->GetPixelFormat(&format));

    ComPtr<IWICComponentInfo> info;
    HR(factory->CreateComponentInfo(format, &info));

    ComPtr<IWICPixelFormatInfo2> pixel_info;
    HR(info.As(&pixel_info));

    WICPixelFormatNumericRepresentation representation;
    HR(pixel_info->GetNumericRepresentation(&representation));

    const bool is_decimal = representation == WICPixelFormatNumericRepresentationFloat ||
                            representation == WICPixelFormatNumericRepresentationFixed;

    ComPtr<IWICFormatConverter> converter;
    HR(factory->CreateFormatConverter(&converter));
    HR(converter->Initialize(frame.Get(), is_decimal ? GUID_WICPixelFormat64bppRGBHalf : GUID_WICPixelFormat64bppRGB,
                             WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom));

    uint32_t w, h;
    HR(converter->GetSize(&w, &h));
    const uint32_t level = static_cast<uint32_t>(std::round(std::cbrt(static_cast<double>(w))));
    const uint32_t size = level * level;

    if (w != h || w < 8u || w != size * level) {
        return std::nullopt;
    }

    std::vector<RGBAF16> data(static_cast<size_t>(w) * h);
    const auto indices = std::views::iota(0uz, data.size());

    if (is_decimal) {
        const uint32_t stride = w * sizeof(RGBAF16);
        HR(converter->CopyPixels(nullptr, stride, stride * h, reinterpret_cast<BYTE*>(data.data())));

        std::for_each(std::execution::par_unseq, indices.begin(), indices.end(), [&](size_t i) {
            data[i] = (data[i].cwiseMin(static_cast<Eigen::half>(1.0f))).cwiseMax(static_cast<Eigen::half>(0.0f));
            data[i].w() = static_cast<Eigen::half>(1.0f);
        });
    } else {
        std::vector<RGBA16> tmp(data.size());
        const uint32_t stride = w * sizeof(RGBA16);
        HR(converter->CopyPixels(nullptr, stride, stride * h, reinterpret_cast<BYTE*>(tmp.data())));

        std::for_each(std::execution::par_unseq, indices.begin(), indices.end(), [&](size_t i) {
            data[i] = (tmp[i].cast<float>() / 65535.0f).cast<Eigen::half>();
            data[i].w() = static_cast<Eigen::half>(1.0f);
        });
    }

    return LUT{level, size, std::move(data)};
}

bool Export(const LUT& lut, const std::filesystem::path& path, const std::wstring& title) {
    WICPixelFormatGUID kFormat = GUID_WICPixelFormat64bppRGBA;

    const uint32_t size = lut.size * lut.level;

    if (lut.level < 2u || lut.data.size() != static_cast<size_t>(size) * size) {
        throw std::runtime_error("Invalid LUT size");
    }

    if (auto parent = path.parent_path(); !parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec != std::error_code{}) {
            throw std::runtime_error("Failed to create directories");
        }
    }

    const auto factory = wic::WIC::factory();

    if (factory == nullptr) {
        throw std::runtime_error("Failed to load WIC factory");
    }

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
    HR(frame->SetPixelFormat(&kFormat));

    ComPtr<IWICMetadataQueryWriter> writer;
    HR(frame->GetMetadataQueryWriter(&writer));

    // 所有権はこちら側なので解放NG
    PROPVARIANT value;
    PropVariantInit(&value);

    value.vt = VT_LPSTR;
    value.pszVal = const_cast<LPSTR>("Software");
    writer->SetMetadataByName(L"/[0]iTXt/Keyword", &value);

    value.vt = VT_UI1;
    value.bVal = 0;
    writer->SetMetadataByName(L"/[0]iTXt/CompressionFlag", &value);

    value.vt = VT_LPWSTR;
    value.pwszVal = const_cast<LPWSTR>(L"ColorLUT_K.aux2");
    writer->SetMetadataByName(L"/[0]iTXt/TextEntry", &value);

    if (!title.empty()) {
        value.vt = VT_LPSTR;
        value.pszVal = const_cast<LPSTR>("Title");
        writer->SetMetadataByName(L"/[1]iTXt/Keyword", &value);

        value.vt = VT_UI1;
        value.bVal = 0;
        writer->SetMetadataByName(L"/[1]iTXt/CompressionFlag", &value);

        value.vt = VT_LPWSTR;
        value.pwszVal = const_cast<LPWSTR>(title.c_str());
        writer->SetMetadataByName(L"/[1]iTXt/TextEntry", &value);
    }

    std::vector<RGBA16> tmp(lut.data.size());

    const auto indices = std::views::iota(0uz, lut.data.size());
    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(), [&](size_t i) {
        tmp[i] = (lut.data[i].cast<float>().cwiseMin(1.0f).cwiseMax(0.0f) * 65535.0f).cast<uint16_t>();
    });

    const uint32_t stride = size * sizeof(RGBA16);
    HR(frame->WritePixels(size, stride, stride * size, reinterpret_cast<BYTE*>(tmp.data())));
    HR(frame->Commit());
    HR(encoder->Commit());

    return true;
}
}  // namespace hald
}  // namespace lut
