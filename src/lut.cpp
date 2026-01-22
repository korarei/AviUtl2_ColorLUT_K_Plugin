#include "lut.hpp"

#include <charconv>
#include <execution>
#include <fstream>
#include <ranges>
#include <sstream>
#include <stdexcept>

#define HR(expr)                             \
    do {                                     \
        HRESULT hr__ = (expr);               \
        if (FAILED(hr__))                    \
            throw std::runtime_error(#expr); \
    } while (0)

void
ColorLUT::setup(ID3D11Texture2D *texture) {
    ComPtr<ID3D11Device> device;
    texture->GetDevice(&device);
    texture->GetDesc(&desc);

    if (d3d_device && device.Get() == d3d_device.Get())
        return;

    d3d_device = device;

    ComPtr<IDXGIDevice> dxgi_device;
    HR(d3d_device.As(&dxgi_device));

    ComPtr<ID2D1Factory3> d2d_factory;
    HR(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(&d2d_factory)));

    HR(d2d_factory->CreateDevice(dxgi_device.Get(), d2d_device.ReleaseAndGetAddressOf()));
    HR(d2d_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, d2d_context.ReleaseAndGetAddressOf()));
    HR(d2d_context->CreateEffect(CLSID_D2D1CrossFade, cross_fade.ReleaseAndGetAddressOf()));

    std::unordered_map<std::wstring, LUT>().swap(cache);
}

void
ColorLUT::create_texture2d(ID3D11Texture2D **texture) const {
    constexpr float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    HR(d3d_device->CreateTexture2D(&desc, nullptr, texture));

    ComPtr<ID3D11RenderTargetView> rtv;
    HR(d3d_device->CreateRenderTargetView(*texture, nullptr, &rtv));

    ComPtr<ID3D11DeviceContext> d3d_context;
    d3d_device->GetImmediateContext(&d3d_context);
    d3d_context->ClearRenderTargetView(rtv.Get(), clear);
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

    HR(d2d_context->CreateBitmapFromDxgiSurface(surface.Get(), &props, bmp));
}

bool
ColorLUT::create_effect(const std::wstring &path, float mix, ID2D1Bitmap1 *bmp, ID2D1Effect **effect) {
    LUT lut{};
    if (!load(path, lut))
        return false;

    switch (lut.dimension) {
        case 1:
            lut._1d->SetInput(0, bmp);
            cross_fade->SetInputEffect(0, lut._1d.Get());
            break;
        case 3:
            lut._3d->SetInput(0, bmp);
            cross_fade->SetInputEffect(0, lut._3d.Get());
            break;
        default:
            return false;
    }

    cross_fade->SetInput(1, bmp);
    HR(cross_fade->SetValue(D2D1_CROSSFADE_PROP_WEIGHT, mix));
    HR(cross_fade.CopyTo(effect));

    return true;
}

void
ColorLUT::draw(ID2D1Image *target, ID2D1Effect *effect) const {
    d2d_context->SetTarget(target);
    d2d_context->BeginDraw();
    d2d_context->DrawImage(effect);
    HR(d2d_context->EndDraw());
}

void
ColorLUT::copy(ID3D11Resource *dst, ID3D11Resource *src) const noexcept {
    ComPtr<ID3D11DeviceContext> d3d_context;
    d3d_device->GetImmediateContext(&d3d_context);
    d3d_context->CopyResource(dst, src);
}

bool
ColorLUT::load(const std::wstring &path, LUT &lut) {
    if (auto it = cache.find(path); it != cache.end()) {
        lut = it->second;
        return true;
    }

    CubeLUT cube{};

    if (!cube.load(path))
        return false;

    switch (cube.dimension) {
        case 1: {
            const UINT32 size = cube.capacity * sizeof(float);

            std::vector<float> r{};
            std::vector<float> g{};
            std::vector<float> b{};
            r.resize(cube.capacity);
            g.resize(cube.capacity);
            b.resize(cube.capacity);

            const auto indices = std::views::iota(0u, cube.capacity);
            std::for_each(std::execution::par_unseq, indices.begin(), indices.end(), [&](int i) {
                auto rgb = cube.data[i];
                rgb = (rgb - cube.domain_min) * cube.scale;
                r[i] = rgb.r;
                g[i] = rgb.g;
                b[i] = rgb.b;
            });

            lut.dimension = 1;
            HR(d2d_context->CreateEffect(CLSID_D2D1TableTransfer, &lut._1d));
            HR(lut._1d->SetValue(D2D1_TABLETRANSFER_PROP_RED_TABLE, reinterpret_cast<const BYTE *>(r.data()), size));
            HR(lut._1d->SetValue(D2D1_TABLETRANSFER_PROP_GREEN_TABLE, reinterpret_cast<const BYTE *>(g.data()), size));
            HR(lut._1d->SetValue(D2D1_TABLETRANSFER_PROP_BLUE_TABLE, reinterpret_cast<const BYTE *>(b.data()), size));
        } break;
        case 3: {
            constexpr UINT32 size = sizeof(RGBA);

            std::vector<RGBA> data{};
            data.resize(cube.capacity);

            UINT32 w = cube.size;
            UINT32 h = cube.size * cube.size;

            const auto indices = std::views::iota(0u, cube.capacity);
            std::for_each(std::execution::par_unseq, indices.begin(), indices.end(), [&](int i) {
                const UINT32 z = i % w;
                const UINT32 y = (i / w) % w;
                const UINT32 x = i / h;

                auto rgb = cube.data[i];
                rgb = (rgb - cube.domain_min) * cube.scale;
                data[x + y * w + z * h] = {rgb.r, rgb.g, rgb.b, 1.0f};
            });

            ComPtr<ID2D1LookupTable3D> lookup_table;
            const UINT32 extents[3] = {cube.size, cube.size, cube.size};
            const UINT32 strides[2] = {w * size, h * size};

            HR(d2d_context->CreateLookupTable3D(D2D1_BUFFER_PRECISION_32BPC_FLOAT, extents,
                                                reinterpret_cast<const BYTE *>(data.data()), cube.capacity * size,
                                                strides, &lookup_table));

            lut.dimension = 3;
            HR(d2d_context->CreateEffect(CLSID_D2D1LookupTable3D, &lut._3d));
            HR(lut._3d->SetValue(D2D1_LOOKUPTABLE3D_PROP_LUT, lookup_table.Get()));
        } break;
        default:
            return false;
    }

    cache.emplace(path, lut);
    return true;
}

void
ColorLUT::reload() noexcept {
    std::unordered_map<std::wstring, LUT>().swap(cache);
}

void
ColorLUT::reload(const std::wstring &path) noexcept {
    cache.erase(path);
}

bool
CubeLUT::load(const std::wstring &path) noexcept {
    constexpr float eps = 1.0e-4f;

    std::ifstream file(path);
    if (!file.is_open())
        return false;

    size = 0u;
    capacity = 0u;
    std::vector<RGB>{}.swap(data);

    std::string line;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream iss(line);
        std::string token;
        iss >> token;

        if (token == "LUT_1D_SIZE") {
            dimension = 1;
            iss >> size;
            if (size < 2u || size > 65536u)
                return false;

            capacity = size;
            break;
        } else if (token == "LUT_3D_SIZE") {
            dimension = 3;
            iss >> size;
            if (size < 2u || size > 256u)
                return false;

            capacity = size * size * size;
            break;
        }
    }

    if (size == 0u)
        return false;

    file.clear();
    file.seekg(0, std::ios::beg);

    data.reserve(capacity);

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream iss(line);
        std::string token;
        iss >> token;

        if (token == "DOMAIN_MIN") {
            iss >> domain_min.r >> domain_min.g >> domain_min.b;
        } else if (token == "DOMAIN_MAX") {
            iss >> domain_max.r >> domain_max.g >> domain_max.b;
        } else {
            float r, g, b;
            const char *first = token.data();
            const char *last = first + token.size();
            auto [ptr, ec] = std::from_chars(first, last, r);

            if (ptr == last && ec == std::errc{} && (iss >> g >> b))
                data.push_back({r, g, b});
        }
    }

    if (data.size() != capacity)
        return false;

    auto range = domain_max - domain_min;
    if (range.r < eps || range.g < eps || range.b < eps)
        return false;

    scale = scale / range;

    return true;
}
