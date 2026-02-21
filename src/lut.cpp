#include "lut.hpp"

#include <immintrin.h>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <cwctype>
#include <execution>
#include <fstream>
#include <memory>
#include <ranges>
#include <sstream>
#include <system_error>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <shobjidl.h>
#include <wincodec.h>
#include <windows.h>

#include "shader.hpp"
#include "utility.hpp"

#define HR(expr)                             \
    do {                                     \
        HRESULT hr__ = (expr);               \
        if (FAILED(hr__))                    \
            throw std::runtime_error(#expr); \
    } while (0)

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
    if (level < 2u || w != h || w * h != data.size() || w != level * level * level)
        return false;

    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec)
            return false;
    }

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open())
        return false;

    auto saturate = [](float v) -> float { return std::clamp(v, 0.0f, 1.0f); };

    file << "# Generated by ColorLUT_K.aux2\n";
    if (!title.empty())
        file << "TITLE \"" << std::string(title.begin(), title.end()) << "\"\n";

    file << "LUT_3D_SIZE " << level * level << "\n";

    const size_t size = w * h;
    for (size_t i = 0; i < size; ++i) {
        const auto &pixel = data[i];
        file << saturate(pixel.r) << " " << saturate(pixel.g) << " " << saturate(pixel.b) << "\n";
    }

    return true;
}

ColorLUT::ColorLUT() { HR(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(&d2d.factory))); }

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

    HR(d2d.factory->CreateDevice(dxgi_device.Get(), d2d.device.ReleaseAndGetAddressOf()));
    HR(d2d.device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, d2d.context.ReleaseAndGetAddressOf()));
    HR(d2d.context->CreateEffect(CLSID_D2D1CrossFade, cross_fade.ReleaseAndGetAddressOf()));

    std::unordered_map<std::filesystem::path, ComPtr<ID2D1Effect>>{}.swap(cache);
}

void
ColorLUT::create_texture(ID3D11Texture2D **texture) const {
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
            .colorContext = nullptr,
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
    d2d.context->SetTarget(nullptr);
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

    auto make_lut3d = [&](uint32_t size, const std::vector<RGBAF32> &data) {
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

                make_lut3d(cube.size, data);
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

        make_lut3d(size, data);
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

void
Hald2Cube::setup(ID3D11Texture2D *texture) {
    ComPtr<ID3D11Device> d3d_device;
    texture->GetDevice(&d3d_device);
    texture->GetDesc(&desc);

    if (device != nullptr && device == d3d_device)
        return;

    device = d3d_device;
    device->GetImmediateContext(context.ReleaseAndGetAddressOf());
}

bool
Hald2Cube::draw_identity(ID3D11Texture2D *texture) {
    struct alignas(16) Params {
        uint32_t level;
        uint32_t _padding[3];
    };

    setup(texture);

    if (desc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT)
        return false;

    const auto level = static_cast<uint32_t>(std::round(std::cbrt(static_cast<double>(desc.Width))));

    if (desc.Width != desc.Height || desc.Width < 8u || desc.Width != level * level * level)
        return false;

    const Params params{
            .level = level,
            ._padding = {},
    };

    D3D11_BUFFER_DESC buffer_desc{};
    buffer_desc.ByteWidth = sizeof(Params);
    buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    D3D11_SUBRESOURCE_DATA data{};
    data.pSysMem = &params;

    ComPtr<ID3D11Buffer> buffer;
    HR(device->CreateBuffer(&buffer_desc, &data, &buffer));

    D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
    uav_desc.Format = desc.Format;
    uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    uav_desc.Texture2D.MipSlice = 0u;

    ComPtr<ID3D11UnorderedAccessView> uav;
    HR(device->CreateUnorderedAccessView(texture, &uav_desc, &uav));

    ComPtr<ID3D11ComputeShader> cs;
    HR(device->CreateComputeShader(shader::identity.data(), shader::identity.size(), nullptr, &cs));

    context->CSSetShader(cs.Get(), nullptr, 0u);
    context->CSSetConstantBuffers(0u, 1u, buffer.GetAddressOf());
    context->CSSetUnorderedAccessViews(0u, 1u, uav.GetAddressOf(), nullptr);
    context->Dispatch((desc.Width + 15u) / 16u, (desc.Height + 15u) / 16u, 1u);

    ID3D11Buffer *null_buffer = nullptr;
    ID3D11UnorderedAccessView *null_uav = nullptr;
    context->CSSetConstantBuffers(0u, 1u, &null_buffer);
    context->CSSetUnorderedAccessViews(0u, 1u, &null_uav, nullptr);
    context->CSSetShader(nullptr, nullptr, 0u);
    return true;
}

bool
Hald2Cube::load(ID3D11Texture2D *texture) {
    setup(texture);

    if (desc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT)
        return false;

    const auto level = static_cast<uint32_t>(std::round(std::cbrt(static_cast<double>(desc.Width))));

    if (desc.Width != desc.Height || desc.Width < 8u || desc.Width != level * level * level) {
        lut.level = 0u;
        lut.w = 0u;
        lut.h = 0u;
        std::vector<RGBAF32>{}.swap(lut.data);
        return false;
    }

    D3D11_TEXTURE2D_DESC staging_desc = desc;
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.BindFlags = 0u;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    staging_desc.MiscFlags = 0u;

    ComPtr<ID3D11Texture2D> staging_texture;
    HR(device->CreateTexture2D(&staging_desc, nullptr, &staging_texture));

    context->CopyResource(staging_texture.Get(), texture);

    D3D11_MAPPED_SUBRESOURCE mapped;
    HR(context->Map(staging_texture.Get(), 0u, D3D11_MAP_READ, 0u, &mapped));

    lut.level = level;
    lut.w = desc.Width;
    lut.h = desc.Height;
    lut.data.resize(desc.Width * desc.Height);

    const auto idx = std::views::iota(0u, desc.Height);
    std::for_each(std::execution::par, idx.begin(), idx.end(), [&](uint32_t y) {
        const std::byte *row = static_cast<const std::byte *>(mapped.pData) + y * mapped.RowPitch;
        const RGBAF16 *src = reinterpret_cast<const RGBAF16 *>(row);
        RGBAF32 *dst = &lut.data[y * desc.Width];

        size_t x = 0;
        while (x + 2 <= desc.Width) {
            __m128i h = _mm_loadu_si128(reinterpret_cast<const __m128i *>(src + x));
            __m256 f = _mm256_cvtph_ps(h);
            _mm256_storeu_ps(reinterpret_cast<float *>(dst + x), f);
            x += 2;
        }

        while (x < desc.Width) {
            __m128i h = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(&src[x]));
            __m128 f = _mm_cvtph_ps(h);
            _mm_storeu_ps(reinterpret_cast<float *>(&dst[x]), f);
            ++x;
        }
    });

    context->Unmap(staging_texture.Get(), 0u);
    return true;
}

void
Hald2Cube::save(const std::u8string &title, void (*callback)(bool success, const wchar_t *msg)) noexcept {
    if (pending.valid() && pending.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;

    // UIスレッド以外で呼ぶの良くはなさそう
    pending = std::async(std::launch::async, [title, callback, lut = lut, owner = owner]() {
        try {
            HR(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE));

            std::shared_ptr<void> guard(nullptr, [](void *) { CoUninitialize(); });  // 手抜きガード

            ComPtr<IFileSaveDialog> dialog;
            HR(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)));

            FILEOPENDIALOGOPTIONS options;
            HR(dialog->GetOptions(&options));
            HR(dialog->SetOptions(options | FOS_NOCHANGEDIR));

            COMDLG_FILTERSPEC filters[] = {{L"Cube LUT File (*.cube)", L"*.cube"}};
            dialog->SetFileTypes(ARRAYSIZE(filters), filters);
            dialog->SetDefaultExtension(L"cube");

            if (owner == nullptr || !IsWindow(owner))
                throw std::runtime_error("The owner window handle is invalid");

            auto hr = dialog->Show(owner);
            if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
                return;
            else if (FAILED(hr))
                throw std::runtime_error("dialog->Show(owner)");

            ComPtr<IShellItem> result;
            HR(dialog->GetResult(&result));

            wchar_t *path = nullptr;
            HR(result->GetDisplayName(SIGDN_FILESYSPATH, &path));

            const bool success = lut.save(std::filesystem::path(path), title);

            CoTaskMemFree(path);

            if (callback)
                callback(success, success ? L"Success to save .cube file" : L"Failed to save .cube file");
        } catch (const std::exception &e) {
            if (callback) {
                const auto msg = string::to_wstr(reinterpret_cast<const char8_t *>(e.what()));
                callback(false, msg.c_str());
            }
        }
    });
}
