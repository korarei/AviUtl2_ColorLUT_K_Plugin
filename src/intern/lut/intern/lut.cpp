#include "../lut.hpp"

#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <execution>
#include <fstream>
#include <ranges>
#include <spanstream>

#include <intern/string.hpp>
#include <intern/wic/wic.hpp>
#include <vector>

#define HR(expr)                                           \
    do {                                                   \
        HRESULT hr__ = (expr);                             \
        if (FAILED(hr__)) throw std::runtime_error(#expr); \
    } while (0)

namespace {
template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;
using Float16 = lut::pixel::Float16;
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

std::optional<TexLUT> TexLUT::Load(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path)) {
        return std::nullopt;
    }

    const auto factory = lut::wic::WIC::factory();

    if (factory == nullptr) {
        throw std::runtime_error("Failed to load WIC factory");
    }

    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = factory->CreateDecoderFromFilename(path.wstring().c_str(), nullptr, GENERIC_READ,
                                                    WICDecodeMetadataCacheOnDemand, &decoder);

    if (FAILED(hr)) {
        return std::nullopt;
    }

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

    hr =
        converter->Initialize(frame.Get(), is_decimal ? GUID_WICPixelFormat64bppRGBAHalf : GUID_WICPixelFormat64bppRGBA,
                              WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);

    if (FAILED(hr)) {
        return std::nullopt;
    }

    uint32_t w, h;
    HR(converter->GetSize(&w, &h));

    if (w < 1u || h < 1u) {
        return std::nullopt;
    }

    std::atomic_bool is_invalid = false;
    std::vector<RGBAF16> data(w * h);

    if (is_decimal) {
        const uint32_t stride = w * sizeof(RGBAF16);
        HR(converter->CopyPixels(nullptr, stride, stride * h, reinterpret_cast<BYTE*>(data.data())));

        std::for_each(std::execution::par_unseq, data.begin(), data.end(), [&](auto& v) {
            if (const float alpha = static_cast<float>(v.w()); alpha > 0.999f && alpha < 1.001f) {
                v = (v.cwiseMin(static_cast<Float16>(1.0f))).cwiseMax(static_cast<Float16>(0.0f));
            } else {
                is_invalid.store(true, std::memory_order_relaxed);
            }
        });
    } else {
        std::vector<RGBA16> tmp(data.size());
        const uint32_t stride = w * sizeof(RGBA16);
        HR(converter->CopyPixels(nullptr, stride, stride * h, reinterpret_cast<BYTE*>(tmp.data())));

        const auto indices = std::views::iota(0uz, data.size());
        std::for_each(std::execution::par_unseq, indices.begin(), indices.end(), [&](size_t i) {
            auto& v = tmp[i];

            if (const uint16_t alpha = v.w(); alpha == 65535u) {
                data[i] = (v.cast<float>() / 65535.0f).cast<Float16>();
            } else {
                is_invalid.store(true, std::memory_order_relaxed);
            }
        });
    }

    if (is_invalid.load(std::memory_order_relaxed)) {
        return std::nullopt;
    }

    return TexLUT(w, h, std::move(data));
}

bool TexLUT::Save(const std::filesystem::path& path, const std::wstring& title) const {
    WICPixelFormatGUID kFormat = GUID_WICPixelFormat64bppRGBA;

    if (auto parent = path.parent_path(); !parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec != std::error_code{}) {
            return false;
        }
    }

    const auto factory = lut::wic::WIC::factory();

    if (factory == nullptr) {
        throw std::runtime_error("Failed to load WIC factory");
    }

    ComPtr<IWICBitmapEncoder> encoder;
    HR(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder));

    ComPtr<IWICStream> stream;
    HR(factory->CreateStream(&stream));

    HRESULT hr = stream->InitializeFromFilename(path.wstring().c_str(), GENERIC_WRITE);

    if (FAILED(hr)) {
        return false;
    }

    HR(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache));

    ComPtr<IWICBitmapFrameEncode> frame;
    HR(encoder->CreateNewFrame(&frame, nullptr));
    HR(frame->Initialize(nullptr));

    HR(frame->SetSize(w_, h_));
    HR(frame->SetPixelFormat(&kFormat));

    ComPtr<IWICMetadataQueryWriter> writer;
    HR(frame->GetMetadataQueryWriter(&writer));

    // 所有権はこちら側なので解放NG
    PROPVARIANT value;
    PropVariantInit(&value);

    value.vt = VT_LPSTR;
    value.pszVal = const_cast<LPSTR>("Software");
    hr = writer->SetMetadataByName(L"/[0]iTXt/Keyword", &value);

    if (SUCCEEDED(hr)) {
        value.vt = VT_UI1;
        value.bVal = 0;
        hr = writer->SetMetadataByName(L"/[0]iTXt/CompressionFlag", &value);

        if (SUCCEEDED(hr)) {
            value.vt = VT_LPWSTR;
            value.pwszVal = const_cast<LPWSTR>(L"ColorLUT_K.aux2");
            writer->SetMetadataByName(L"/[0]iTXt/TextEntry", &value);
        }
    }

    if (!title.empty()) {
        value.vt = VT_LPSTR;
        value.pszVal = const_cast<LPSTR>("Title");
        hr = writer->SetMetadataByName(L"/[1]iTXt/Keyword", &value);

        if (SUCCEEDED(hr)) {
            value.vt = VT_UI1;
            value.bVal = 0;
            hr = writer->SetMetadataByName(L"/[1]iTXt/CompressionFlag", &value);

            if (SUCCEEDED(hr)) {
                value.vt = VT_LPWSTR;
                value.pwszVal = const_cast<LPWSTR>(title.c_str());
                writer->SetMetadataByName(L"/[1]iTXt/TextEntry", &value);
            }
        }
    }

    std::vector<RGBA16> tmp(data_.size());

    const auto indices = std::views::iota(0uz, data_.size());
    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(), [&](size_t i) {
        tmp[i] = (data_[i].cast<float>().cwiseMin(1.0f).cwiseMax(0.0f) * 65535.0f).cast<uint16_t>();
    });

    const uint32_t stride = w_ * sizeof(RGBA16);
    HR(frame->WritePixels(w_, stride, stride * h_, reinterpret_cast<BYTE*>(tmp.data())));
    HR(frame->Commit());
    HR(encoder->Commit());

    return true;
}

TexLUT::TexLUT(uint32_t w, uint32_t h, const std::vector<RGBAF16>& data) : w_(w), h_(h), data_(std::move(data)) {}

std::optional<CubeLUT> CubeLUT::Init(int dimension, uint32_t size, std::vector<Float16>&& data) {
    switch (dimension) {
        case 1:
            if (size < 2u || size > 65536u) {
                return std::nullopt;
            }

            if (data.size() != size * 3uz) {
                return std::nullopt;
            }
            break;
        case 3:
            if (size < 2u || size > 256u) {
                return std::nullopt;
            }

            if (data.size() != 4uz * size * static_cast<size_t>(size) * size) {
                return std::nullopt;
            }

            break;
        default:
            return std::nullopt;
    }

    return CubeLUT(dimension, size, std::move(data));
}

CubeLUT CubeLUT::Init(const HaldLUT& lut) {
    std::vector<Float16> data(lut.data_.size() * 4uz);
    std::memcpy(data.data(), lut.data_.data(), data.size() * sizeof(Float16));
    return CubeLUT(3, lut.size_, std::move(data));
}

CubeLUT CubeLUT::Init(const StripLUT& lut) {
    std::vector<Float16> data(lut.data_.size() * 4uz);

    const auto indices = std::views::iota(0uz, lut.data_.size());
    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(), [&](size_t i) {
        const auto r = i % lut.size_;
        const auto g = i / (lut.size_ * lut.size_);
        const auto b = (i / lut.size_) % lut.size_;

        const auto& v = lut.data_[r + g * lut.size_ + b * lut.size_ * lut.size_];

        const auto j = i * 4uz;
        data[j] = v.x();
        data[j + 1uz] = v.y();
        data[j + 2uz] = v.z();
        data[j + 3uz] = v.w();
    });

    return CubeLUT(3, lut.size_, std::move(data));
}

std::optional<CubeLUT> CubeLUT::Import(const std::filesystem::path& path) {
    constexpr float kEpsilon = 1.0e-4f;

    std::ifstream file(path);

    if (!file.is_open()) {
        return std::nullopt;
    }

    int dimension = 0;
    uint32_t size = 0u;
    std::vector<Float16> data;

    Eigen::Vector3f min(0.0f, 0.0f, 0.0f);
    Eigen::Vector3f max(1.0f, 1.0f, 1.0f);
    Eigen::Vector3f scale(1.0f, 1.0f, 1.0f);

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
                    data[0uz] = static_cast<Float16>(v.x());
                    data[size] = static_cast<Float16>(v.y());
                    data[size * 2u] = static_cast<Float16>(v.z());

                    ++index;
                    break;
                case 3:
                    data[0uz] = static_cast<Float16>(v.x());
                    data[1uz] = static_cast<Float16>(v.y());
                    data[2uz] = static_cast<Float16>(v.z());
                    data[3uz] = static_cast<Float16>(1.0f);

                    index += 4uz;
                    break;
                default:
                    std::unreachable();
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
                data[index] = static_cast<Float16>(v.x());
                data[index + size] = static_cast<Float16>(v.y());
                data[index + size * 2u] = static_cast<Float16>(v.z());

                ++index;
                break;
            case 3:
                data[index] = static_cast<Float16>(v.x());
                data[index + 1uz] = static_cast<Float16>(v.y());
                data[index + 2uz] = static_cast<Float16>(v.z());
                data[index + 3uz] = static_cast<Float16>(1.0f);

                index += 4uz;
                break;
            default:
                std::unreachable();
        }
    }

    if (index != length) {
        return std::nullopt;
    }

    return CubeLUT(dimension, size, std::move(data));
}

LUTView CubeLUT::View() const {
    switch (dimension_) {
        case 1:
            return {
                .size = size_,
                .data = LUTView::LUT1D(data_.data(), data_.size()),
            };
        case 3:
            return {
                .size = size_,
                .data = LUTView::LUT3D(reinterpret_cast<const RGBAF16*>(data_.data()), data_.size() / 4uz),
            };
        default:
            std::unreachable();
    }
}

bool CubeLUT::Export(const std::filesystem::path& path, const std::wstring& title) const {
    if (auto parent = path.parent_path(); !parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec != std::error_code{}) {
            return false;
        }
    }

    std::ofstream file(path, std::ios::binary);

    if (!file.is_open()) {
        return false;
    }

    auto saturate = [](Float16 v) -> float { return std::clamp(static_cast<float>(v), 0.0f, 1.0f); };

    file << "# Generated by ColorLUT_K.aux2\n";

    if (!title.empty()) {
        file << "TITLE \"" << string::AsString(string::ToUtf8(title)) << "\"\n";
    }

    switch (dimension_) {
        case 1:
            file << "LUT_1D_SIZE " << size_ << "\n";

            for (size_t i = 0uz; i < size_; ++i) {
                file << saturate(data_[i]) << " " << saturate(data_[i + size_]) << " "
                     << saturate(data_[i + size_ * 2u]) << "\n";
            }

            break;
        case 3: {
            file << "LUT_3D_SIZE " << size_ << "\n";

            size_t i = 0uz;
            while (i < data_.size()) {
                file << saturate(data_[i]) << " " << saturate(data_[i + 1uz]) << " " << saturate(data_[i + 2uz])
                     << "\n";
                i += 4uz;
            }

            break;
        }
        default:
            std::unreachable();
    }

    return true;
}

CubeLUT::CubeLUT(int dimension, uint32_t size, std::vector<Float16>&& data)
    : dimension_(dimension), size_(size), data_(std::move(data)) {}

std::optional<HaldLUT> HaldLUT::Init(uint32_t level, std::vector<RGBAF16>&& data) {
    if (level < 2u || level > 16u) {
        return std::nullopt;
    }

    const auto size = level * level;
    const auto w = size * level;

    if (data.size() != static_cast<size_t>(w) * w) {
        return std::nullopt;
    }

    std::atomic_bool is_invalid = false;

    std::for_each(std::execution::par_unseq, data.begin(), data.end(), [&](const auto& v) {
        if (const float alpha = static_cast<float>(v.w()); alpha <= 0.999f || alpha >= 1.001f) {
            is_invalid.store(true, std::memory_order_relaxed);
        }
    });

    if (is_invalid.load(std::memory_order_relaxed)) {
        return std::nullopt;
    }

    return HaldLUT(size, w, std::move(data));
}

std::optional<HaldLUT> HaldLUT::Init(const StripLUT& lut) {
    const uint32_t level = static_cast<uint32_t>(std::lround(std::sqrt(static_cast<double>(lut.size_))));

    if (level * level != lut.size_) {
        return std::nullopt;
    }

    const uint32_t w = lut.size_ * level;

    std::vector<RGBAF16> data(lut.data_.size());

    const auto indices = std::views::iota(0uz, data.size());
    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(), [&](size_t i) {
        const auto r = i % lut.size_;
        const auto g = i / (lut.size_ * lut.size_);
        const auto b = (i / lut.size_) % lut.size_;

        data[i] = lut.data_[r + g * lut.size_ + b * lut.size_ * lut.size_];
    });

    return HaldLUT(lut.size_, w, std::move(data));
}

std::optional<HaldLUT> HaldLUT::Import(const std::filesystem::path& path) {
    auto img = Load(path);

    if (!img.has_value()) {
        return std::nullopt;
    }

    if (img->w() != img->h() || img->w() < 8u || img->w() > 4096u) {
        return std::nullopt;
    }

    const auto level = std::lround(std::cbrt(static_cast<double>(img->w())));
    const uint32_t size = level * level;

    if (img->w() != size * level) {
        return std::nullopt;
    }

    return HaldLUT(size, img->w(), std::move(img->data()));
}

LUTView HaldLUT::View() const {
    return {
        .size = size_,
        .data = LUTView::LUT3D(data_.data(), data_.size()),
    };
}

bool HaldLUT::Export(const std::filesystem::path& path, const std::wstring& title) const { return Save(path, title); }

HaldLUT::HaldLUT(uint32_t size, uint32_t w, std::vector<RGBAF16>&& data)
    : TexLUT(w, w, std::move(data)), size_(size) {};

std::optional<StripLUT> StripLUT::Init(uint32_t size, std::vector<RGBAF16>&& data) {
    if (size < 2u || size > 128u || data.size() != size * static_cast<size_t>(size) * size) {
        return std::nullopt;
    }

    std::atomic_bool is_invalid = false;

    std::for_each(std::execution::par_unseq, data.begin(), data.end(), [&](const auto& v) {
        if (const float alpha = static_cast<float>(v.w()); alpha <= 0.999f || alpha >= 1.001f) {
            is_invalid.store(true, std::memory_order_relaxed);
        }
    });

    if (is_invalid.load(std::memory_order_relaxed)) {
        return std::nullopt;
    }

    return StripLUT(size, size * size, size, std::move(data));
}

StripLUT StripLUT::Init(const HaldLUT& lut) {
    const uint32_t w = lut.size_ * lut.size_, h = lut.size_;

    std::vector<RGBAF16> data(lut.data_.size());

    const auto indices = std::views::iota(0uz, data.size());
    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(), [&](size_t i) {
        const auto r = i % lut.size_;
        const auto g = (i / lut.size_) % lut.size_;
        const auto b = i / (lut.size_ * lut.size_);

        data[i] = lut.data_[r + b * lut.size_ + g * lut.size_ * lut.size_];
    });

    return StripLUT(lut.size_, w, h, std::move(data));
}

std::optional<StripLUT> StripLUT::Import(const std::filesystem::path& path) {
    auto img = TexLUT::Load(path);

    if (!img.has_value()) {
        return std::nullopt;
    }

    const uint32_t size = img->h();

    if (size < 2u || size > 128u || img->w() != size * size) {
        return std::nullopt;
    }

    return StripLUT(size, img->w(), img->h(), std::move(img->data()));
}

bool StripLUT::Export(const std::filesystem::path& path, const std::wstring& title) const { return Save(path, title); }

StripLUT::StripLUT(uint32_t size, uint32_t w, uint32_t h, std::vector<RGBAF16>&& data)
    : TexLUT(w, h, std::move(data)), size_(size) {}

}  // namespace lut
