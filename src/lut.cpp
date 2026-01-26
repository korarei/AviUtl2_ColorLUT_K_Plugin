#include "lut.hpp"

#include <charconv>
#include <cwctype>
#include <execution>
#include <fstream>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>

#define HR(expr)                             \
    do {                                     \
        HRESULT hr__ = (expr);               \
        if (FAILED(hr__))                    \
            throw std::runtime_error(#expr); \
    } while (0)

namespace {
template <typename T>
[[nodiscard]] bool
is_number(const std::string &s, T &v) {
    const char *first = s.data();
    const char *last = first + s.size();
    auto [ptr, ec] = std::from_chars(first, last, v);
    return ptr == last && ec == std::errc{};
}
}  // namespace

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

    std::unordered_map<std::filesystem::path, ComPtr<ID2D1Effect>>().swap(cache);
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

    auto ext = path.extension().wstring();
    std::ranges::for_each(ext, [](wchar_t &c) { c = std::towlower(c); });

    if (ext == L".cube") {
        CubeLUT cube{};

        if (!cube.load(path))
            return false;

        switch (cube.dimension) {
            case 1: {
                const std::uint32_t size = cube.capacity * sizeof(float);

                std::vector<float> r{};
                std::vector<float> g{};
                std::vector<float> b{};
                r.resize(cube.capacity);
                g.resize(cube.capacity);
                b.resize(cube.capacity);

                const auto idx = std::views::iota(0u, cube.capacity);
                std::for_each(std::execution::par_unseq, idx.begin(), idx.end(), [&](std::uint32_t i) {
                    auto rgb = cube.data[i];
                    rgb = (rgb - cube.domain_min) * cube.scale;
                    r[i] = rgb.r;
                    g[i] = rgb.g;
                    b[i] = rgb.b;
                });

                HR(d2d.context->CreateEffect(CLSID_D2D1TableTransfer, &fx));
                HR(fx->SetValue(D2D1_TABLETRANSFER_PROP_RED_TABLE, reinterpret_cast<const BYTE *>(r.data()), size));
                HR(fx->SetValue(D2D1_TABLETRANSFER_PROP_GREEN_TABLE, reinterpret_cast<const BYTE *>(g.data()), size));
                HR(fx->SetValue(D2D1_TABLETRANSFER_PROP_BLUE_TABLE, reinterpret_cast<const BYTE *>(b.data()), size));

                break;
            }
            case 3: {
                constexpr std::uint32_t size = sizeof(pixel::RGBA);

                std::vector<pixel::RGBA> data{};
                data.resize(cube.capacity);

                const std::uint32_t w = cube.size;
                const std::uint32_t h = cube.size * cube.size;

                const auto idx = std::views::iota(0u, cube.capacity);
                std::for_each(std::execution::par_unseq, idx.begin(), idx.end(), [&](std::uint32_t i) {
                    const std::uint32_t z = i % w;
                    const std::uint32_t y = (i / w) % w;
                    const std::uint32_t x = i / h;

                    auto rgb = cube.data[i];
                    rgb = (rgb - cube.domain_min) * cube.scale;
                    data[x + y * w + z * h] = {rgb.r, rgb.g, rgb.b, 1.0f};
                });

                ComPtr<ID2D1LookupTable3D> lut3d;
                const std::uint32_t extents[3] = {cube.size, cube.size, cube.size};
                const std::uint32_t strides[2] = {w * size, h * size};

                HR(d2d.context->CreateLookupTable3D(D2D1_BUFFER_PRECISION_32BPC_FLOAT, extents,
                                                    reinterpret_cast<const BYTE *>(data.data()), cube.capacity * size,
                                                    strides, &lut3d));

                HR(d2d.context->CreateEffect(CLSID_D2D1LookupTable3D, &fx));
                HR(fx->SetValue(D2D1_LOOKUPTABLE3D_PROP_LUT, lut3d.Get()));
                HR(fx->SetValue(D2D1_LOOKUPTABLE3D_PROP_ALPHA_MODE, D2D1_ALPHA_MODE_PREMULTIPLIED));

                break;
            }
            default:
                return false;
        }
    } else {
        return false;
    }

    cache.emplace(path, fx);
    HR(fx.CopyTo(lut));
    return true;
}

void
ColorLUT::reload() noexcept {
    std::unordered_map<std::filesystem::path, ComPtr<ID2D1Effect>>().swap(cache);
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

    size = 0u;
    capacity = 0u;
    std::vector<pixel::RGB>().swap(data);

    std::string line;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream iss(line);
        std::string token;
        iss >> token;

        if (token == "TITLE") {
            ;
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

    scale = pixel::RGB{1.0f, 1.0f, 1.0f} / range;

    return true;
}
