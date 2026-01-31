#include "lut.hpp"

#include <wincodec.h>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cwctype>
#include <execution>
#include <fstream>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <system_error>

#define HR(expr)                             \
    do {                                     \
        HRESULT hr__ = (expr);               \
        if (FAILED(hr__))                    \
            throw std::runtime_error(#expr); \
    } while (0)

void
ColorLUT::setup(ID3D11Texture2D *texture) {
    ComPtr<ID3D11Device> d3d_device;
    texture->GetDevice(&d3d_device);
    texture->GetDesc(&desc);

    if (d3d.device != nullptr && d3d.device == d3d_device)
        return;

    d3d.device = d3d_device;
    d3d.device->GetImmediateContext(d3d.context.ReleaseAndGetAddressOf());

    ComPtr<IDXGIDevice> dxgi_device;
    HR(d3d.device.As(&dxgi_device));

    ComPtr<ID2D1Factory3> d2d_factory;
    HR(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(&d2d_factory)));

    HR(d2d_factory->CreateDevice(dxgi_device.Get(), d2d.device.ReleaseAndGetAddressOf()));
    HR(d2d.device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, d2d.context.ReleaseAndGetAddressOf()));
    HR(d2d.context->CreateEffect(CLSID_D2D1CrossFade, cross_fade.ReleaseAndGetAddressOf()));

    std::unordered_map<std::filesystem::path, ComPtr<ID2D1Effect>>{}.swap(cache);
}

void
ColorLUT::create_texture2d(ID3D11Texture2D **texture) const {
    constexpr float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    HR(d3d.device->CreateTexture2D(&desc, nullptr, texture));

    ComPtr<ID3D11RenderTargetView> rtv;
    HR(d3d.device->CreateRenderTargetView(*texture, nullptr, &rtv));
    d3d.context->ClearRenderTargetView(rtv.Get(), clear);
}

void
ColorLUT::create_bitmap(ID3D11Texture2D *texture, D2D1_BITMAP_OPTIONS options, ID2D1Bitmap1 **bmp) const {
    ComPtr<IDXGISurface> surface;
    HR(texture->QueryInterface(IID_PPV_ARGS(&surface)));

    D2D1_BITMAP_PROPERTIES1 props{
            .pixelFormat = {desc.Format, D2D1_ALPHA_MODE_PREMULTIPLIED},
            .dpiX = 96.0f,
            .dpiY = 96.0f,
            .bitmapOptions = options,
    };

    HR(d2d.context->CreateBitmapFromDxgiSurface(surface.Get(), &props, bmp));
}

bool
ColorLUT::create_effect(const std::filesystem::path &path, float mix, ID2D1Bitmap1 *bmp, ID2D1Effect **fx) {
    ComPtr<ID2D1Effect> lut;
    if (!load(path, &lut))
        return false;

    lut->SetInput(0, bmp);
    cross_fade->SetInputEffect(0, lut.Get());
    cross_fade->SetInput(1, bmp);
    HR(cross_fade->SetValue(D2D1_CROSSFADE_PROP_WEIGHT, mix));
    HR(cross_fade.CopyTo(fx));

    return true;
}

void
ColorLUT::draw(ID2D1Image *target, ID2D1Effect *fx) const {
    d2d.context->SetTarget(target);
    d2d.context->BeginDraw();
    d2d.context->DrawImage(fx);
    HR(d2d.context->EndDraw());
}

void
ColorLUT::copy(ID3D11Resource *dst, ID3D11Resource *src) const noexcept {
    d3d.context->CopyResource(dst, src);
}

bool
ColorLUT::load(const std::filesystem::path &path, ID2D1Effect **lut) {
    if (auto it = cache.find(path); it != cache.end()) {
        HR(it->second.CopyTo(lut));
        return true;
    }

    ComPtr<ID2D1Effect> fx;

    auto create_lut3d_effect = [&](uint32_t size, const std::vector<RGBAF32> &data) {
        constexpr uint32_t bytes = sizeof(RGBAF32);

        ComPtr<ID2D1LookupTable3D> lut3d;
        const uint32_t extents[3] = {size, size, size};
        const uint32_t strides[2] = {size * bytes, size * size * bytes};

        HR(d2d.context->CreateLookupTable3D(D2D1_BUFFER_PRECISION_32BPC_FLOAT, extents,
                                            reinterpret_cast<const uint8_t *>(data.data()),
                                            static_cast<uint32_t>(data.size()) * bytes, strides, &lut3d));

        HR(d2d.context->CreateEffect(CLSID_D2D1LookupTable3D, &fx));
        HR(fx->SetValue(D2D1_LOOKUPTABLE3D_PROP_LUT, lut3d.Get()));
        HR(fx->SetValue(D2D1_LOOKUPTABLE3D_PROP_ALPHA_MODE, D2D1_ALPHA_MODE_PREMULTIPLIED));
    };

    auto ext = path.extension().wstring();
    std::ranges::for_each(ext, [](wchar_t &c) { c = std::towlower(c); });

    if (ext == L".cube") {
        CubeLUT cube{};

        if (!cube.load(path))
            return false;

        switch (cube.dimension) {
            case 1: {
                constexpr uint32_t bytes = sizeof(float);

                std::vector<float> r(cube.capacity), g(cube.capacity), b(cube.capacity);

                const auto idx = std::views::iota(0u, cube.capacity);
                std::for_each(std::execution::par_unseq, idx.begin(), idx.end(), [&](uint32_t i) {
                    const auto rgb = (cube.data[i] - cube.domain_min) * cube.scale;
                    r[i] = rgb.r;
                    g[i] = rgb.g;
                    b[i] = rgb.b;
                });

                HR(d2d.context->CreateEffect(CLSID_D2D1TableTransfer, &fx));

                const uint32_t size = cube.capacity * bytes;
                auto set_table = [&](D2D1_TABLETRANSFER_PROP prop, const std::vector<float> &table) {
                    HR(fx->SetValue(prop, reinterpret_cast<const uint8_t *>(table.data()), size));
                };

                set_table(D2D1_TABLETRANSFER_PROP_RED_TABLE, r);
                set_table(D2D1_TABLETRANSFER_PROP_GREEN_TABLE, g);
                set_table(D2D1_TABLETRANSFER_PROP_BLUE_TABLE, b);

                break;
            }
            case 3: {
                std::vector<RGBAF32> data(cube.capacity);

                const uint32_t area = cube.size * cube.size;

                const auto idx = std::views::iota(0u, cube.capacity);
                std::for_each(std::execution::par_unseq, idx.begin(), idx.end(), [&](uint32_t i) {
                    const uint32_t z = i % cube.size;
                    const uint32_t y = (i / cube.size) % cube.size;
                    const uint32_t x = i / area;

                    const auto rgb = (cube.data[i] - cube.domain_min) * cube.scale;
                    data[x + y * cube.size + z * area] = RGBAF32(rgb);
                });

                create_lut3d_effect(cube.size, data);

                break;
            }
            default:
                return false;
        }
    } else if (ext == L".bmp" || ext == L".png" || ext == L".tiff" || ext == L".tif") {
        HaldLUT hald{};

        if (!hald.load(path))
            return false;

        const uint32_t size = hald.level * hald.level;
        const uint32_t area = size * size;

        std::vector<RGBAF32> data(size * area);

        const auto idx = std::views::iota(0u, static_cast<uint32_t>(data.size()));
        std::for_each(std::execution::par_unseq, idx.begin(), idx.end(), [&](uint32_t i) {
            const uint32_t z = i % size;
            const uint32_t y = (i / size) % size;
            const uint32_t x = i / area;

            data[x + y * size + z * area] = hald.data[i];
        });

        create_lut3d_effect(size, data);
    } else {
        return false;
    }

    cache.emplace(path, fx);
    HR(fx.CopyTo(lut));
    return true;
}

void
ColorLUT::reload() noexcept {
    std::unordered_map<std::filesystem::path, ComPtr<ID2D1Effect>>{}.swap(cache);
}

void
ColorLUT::reload(const std::filesystem::path &path) noexcept {
    cache.erase(path);
}

bool
CubeLUT::load(const std::filesystem::path &path) noexcept {
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

    domain_min = {0.0f, 0.0f, 0.0f};
    domain_max = {1.0f, 1.0f, 1.0f};
    scale = {1.0f, 1.0f, 1.0f};
    size = 0u;
    capacity = 0u;
    std::vector<RGBF32>{}.swap(data);

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
                data.reserve(capacity);
                data.push_back({r, g, b});
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
            data.push_back({r, g, b});
    }

    if (data.size() != capacity)
        return false;

    auto range = domain_max - domain_min;
    if (range.r < eps || range.g < eps || range.b < eps)
        return false;

    scale = RGBF32(1.0f, 1.0f, 1.0f) / range;

    return true;
}

bool
HaldLUT::load(const std::filesystem::path &path) {
    using Microsoft::WRL::ComPtr;

    if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path))
        return false;

    // CoInitializeExは呼ばれているはず
    // 呼ばれていない場合は以下の処理でランタイムエラー
    ComPtr<IWICImagingFactory2> factory;
    HR(CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));

    ComPtr<IWICBitmapDecoder> decoder;
    HR(factory->CreateDecoderFromFilename(path.wstring().c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand,
                                          &decoder));

    ComPtr<IWICBitmapFrameDecode> frame;
    HR(decoder->GetFrame(0u, &frame));

    // sRGBに強制的に変換されるはず
    // ColorContextがない場合はUINTならそのまま (確認済) でFloatならscRGB->sRGB変換されるはず (未確認)
    // 多くの場合はUINTかつsRGB前提なので問題ないはず
    // .tifにはFloatが存在するが，対応ソフトが少ないので考慮しない (GIMP, Blender, Clip Studio Paintなどは未対応)
    ComPtr<IWICFormatConverter> converter;
    HR(factory->CreateFormatConverter(&converter));
    HR(converter->Initialize(frame.Get(), GUID_WICPixelFormat64bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0,
                             WICBitmapPaletteTypeCustom));

    HR(converter->GetSize(&w, &h));
    level = static_cast<uint32_t>(std::round(std::cbrt(static_cast<double>(w))));

    if (w != h || w < 8u || w != level * level * level)
        return false;

    const size_t size = w * h;
    std::vector<RGBA16> buffer(size);
    const uint32_t stride = w * sizeof(RGBA16);
    HR(converter->CopyPixels(nullptr, stride, stride * h, reinterpret_cast<uint8_t *>(buffer.data())));

    data.resize(size);
    const auto idx = std::views::iota(0uz, size);
    std::for_each(std::execution::par_unseq, idx.begin(), idx.end(), [&](size_t i) { data[i] = RGBAF32(buffer[i]); });

    return true;
}

bool
HaldLUT::save(const std::filesystem::path &path, const std::u8string &title) const {
    if (w != h || w * h != data.size() || w != level * level * level)
        return false;

    if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path))
        return false;

    std::ofstream file(path / (title + u8".cube"), std::ios::binary);
    if (!file.is_open())
        return false;

    auto saturate = [](float v) -> float { return std::clamp(v, 0.0f, 1.0f); };

    file << "# Generated by ColorLUT_K.aux2\n";
    file << "TITLE \"" << std::string(title.begin(), title.end()) << "\"\n";
    file << "LUT_3D_SIZE " << level * level << "\n";

    const size_t size = w * h;
    for (size_t i = 0; i < size; ++i) {
        const auto &pixel = data[i];
        file << saturate(pixel.r) << " " << saturate(pixel.g) << " " << saturate(pixel.b) << "\n";
    }

    return true;
}
