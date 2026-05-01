#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

#include <d3d11.h>
#include <wrl/client.h>

#include "pixel.hpp"

#include <blit.h>
#include <fullscreen.h>

namespace lut::direct3d {
template <size_t NTemp, size_t NCache, size_t NCB, size_t NPS>
class Direct3D {
public:
    class Ctrl {
    public:
        template <size_t PS, size_t CB, size_t NSRV>
        class PixelShader {
        public:
            constexpr ~PixelShader() = default;
            constexpr PixelShader(Direct3D &owner) : owner(owner) {}

            constexpr PixelShader(const PixelShader &) = delete;
            constexpr PixelShader &operator=(const PixelShader &) = delete;
            constexpr PixelShader(PixelShader &&) = delete;
            constexpr PixelShader &operator=(PixelShader &&) = delete;

            template <typename T>
            constexpr void operator()(
                    ID3D11RenderTargetView *const *dst,
                    int w,
                    int h,
                    const std::array<ID3D11ShaderResourceView *, NSRV> &srvs,
                    const T &data) const
                requires(NCB > 0 && NSRV > 0)
            {
                constexpr ID3D11ShaderResourceView *null_views[NSRV] = {nullptr};
                constexpr size_t size = sizeof(T);

                if (size > owner.cb[CB].size)
                    throw std::runtime_error("Constant buffer size mismatch");

                const auto &cbuffer = owner.cb[CB].handle;

                D3D11_MAPPED_SUBRESOURCE mapped{};
                HRESULT hr = owner.ctx->Map(
                        cbuffer.Get(), 0u, D3D11_MAP_WRITE_DISCARD, 0u, &mapped);
                if (FAILED(hr))
                    throw std::runtime_error("Failed to map constant buffer");

                std::memcpy(mapped.pData, &data, size);
                owner.ctx->Unmap(cbuffer.Get(), 0u);

                D3D11_VIEWPORT vp{
                        0.0f,
                        0.0f,
                        static_cast<float>(w),
                        static_cast<float>(h),
                        0.0f,
                        1.0f};

                owner.ctx->IASetInputLayout(nullptr);
                owner.ctx->IASetPrimitiveTopology(
                        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                owner.ctx->VSSetShader(owner.vs.Get(), nullptr, 0u);
                owner.ctx->PSSetShader(owner.ps[PS].handle.Get(), nullptr, 0u);
                owner.ctx->PSSetSamplers(0u, 1u, owner.smp.GetAddressOf());
                owner.ctx->PSSetConstantBuffers(0u, 1u, cbuffer.GetAddressOf());
                owner.ctx->PSSetShaderResources(0u, NSRV, srvs.data());

                owner.ctx->RSSetViewports(1u, &vp);
                owner.ctx->OMSetRenderTargets(1u, dst, nullptr);

                owner.ctx->Draw(3u, 0u);

                owner.ctx->OMSetRenderTargets(1u, &null_rtv, nullptr);
                owner.ctx->PSSetShaderResources(0u, NSRV, null_views);
            }

            template <typename T>
            constexpr void operator()(
                    ID3D11RenderTargetView *const *dst,
                    int w,
                    int h,
                    const T &data) const
                requires(NCB > 0 && NSRV == 0)
            {
                constexpr size_t size = sizeof(T);

                if (size > owner.cb[CB].size)
                    throw std::runtime_error("Constant buffer size mismatch");

                const auto &cbuffer = owner.cb[CB].handle;

                D3D11_MAPPED_SUBRESOURCE mapped{};
                HRESULT hr = owner.ctx->Map(
                        cbuffer.Get(), 0u, D3D11_MAP_WRITE_DISCARD, 0u, &mapped);
                if (FAILED(hr))
                    throw std::runtime_error("Failed to map constant buffer");

                std::memcpy(mapped.pData, &data, size);
                owner.ctx->Unmap(cbuffer.Get(), 0u);

                D3D11_VIEWPORT vp{
                        0.0f,
                        0.0f,
                        static_cast<float>(w),
                        static_cast<float>(h),
                        0.0f,
                        1.0f};

                owner.ctx->IASetInputLayout(nullptr);
                owner.ctx->IASetPrimitiveTopology(
                        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                owner.ctx->VSSetShader(owner.vs.Get(), nullptr, 0u);
                owner.ctx->PSSetShader(owner.ps[PS].handle.Get(), nullptr, 0u);
                owner.ctx->PSSetSamplers(0u, 1u, owner.smp.GetAddressOf());
                owner.ctx->PSSetConstantBuffers(0u, 1u, cbuffer.GetAddressOf());

                owner.ctx->RSSetViewports(1u, &vp);
                owner.ctx->OMSetRenderTargets(1u, dst, nullptr);

                owner.ctx->Draw(3u, 0u);

                owner.ctx->OMSetRenderTargets(1u, &null_rtv, nullptr);
            }

        private:
            static constexpr ID3D11RenderTargetView *null_rtv = nullptr;

            Direct3D &owner;
        };

        struct Cache {
            ID3D11ShaderResourceView *srv = nullptr;
            ID3D11RenderTargetView *rtv = nullptr;
        };

        struct Temp {
            ID3D11ShaderResourceView *srv = nullptr;
            ID3D11UnorderedAccessView *uav = nullptr;
        };

        constexpr ~Ctrl() = default;
        constexpr Ctrl(Direct3D &owner) : owner(owner) {}

        constexpr Ctrl(const Ctrl &) = delete;
        constexpr Ctrl &operator=(const Ctrl &) = delete;
        constexpr Ctrl(Ctrl &&) = delete;
        constexpr Ctrl &operator=(Ctrl &&) = delete;

        constexpr void create_texture(
                ID3D11Texture1D **dst,
                uint32_t w,
                uint32_t n,
                const float *data) const {
            D3D11_TEXTURE1D_DESC desc{
                    .Width = w,
                    .MipLevels = 1u,
                    .ArraySize = n,
                    .Format = DXGI_FORMAT_R32_FLOAT,
                    .Usage = D3D11_USAGE_DEFAULT,
                    .BindFlags = D3D11_BIND_SHADER_RESOURCE,
                    .CPUAccessFlags = 0u,
                    .MiscFlags = 0u,
            };

            if (data == nullptr) {
                if (FAILED(owner.device->CreateTexture1D(&desc, nullptr, dst)))
                    throw std::runtime_error("Failed to create texture1d");

                return;
            }

            std::vector<D3D11_SUBRESOURCE_DATA> init(n);
            for (uint32_t i = 0u; i < n; ++i) {
                init[i].pSysMem = data + i * w;
                init[i].SysMemPitch = 0u;
                init[i].SysMemSlicePitch = 0u;
            }

            if (FAILED(owner.device->CreateTexture1D(&desc, init.data(), dst)))
                throw std::runtime_error("Failed to create texture1d");
        }

        constexpr void create_texture(
                ID3D11Texture3D **dst,
                uint32_t w,
                uint32_t h,
                uint32_t d,
                const pixel::RGBF32 *data) const {
            constexpr uint32_t size = static_cast<uint32_t>(sizeof(pixel::RGBF32));

            D3D11_TEXTURE3D_DESC desc{
                    .Width = w,
                    .Height = h,
                    .Depth = d,
                    .MipLevels = 1u,
                    .Format = DXGI_FORMAT_R32G32B32_FLOAT,
                    .Usage = D3D11_USAGE_DEFAULT,
                    .BindFlags = D3D11_BIND_SHADER_RESOURCE,
                    .CPUAccessFlags = 0u,
                    .MiscFlags = 0u,
            };

            if (data == nullptr) {
                if (FAILED(owner.device->CreateTexture3D(&desc, nullptr, dst)))
                    throw std::runtime_error("Failed to create texture3d");

                return;
            }

            D3D11_SUBRESOURCE_DATA init{
                    .pSysMem = data,
                    .SysMemPitch = w * size,
                    .SysMemSlicePitch = w * h * size,
            };

            if (FAILED(owner.device->CreateTexture3D(&desc, &init, dst)))
                throw std::runtime_error("Failed to create texture3d");
        }

        constexpr void create_shader_resource_view(
                ID3D11ShaderResourceView **srv, ID3D11Resource *resource) const {
            if (FAILED(owner.device->CreateShaderResourceView(resource, nullptr, srv)))
                throw std::runtime_error("Failed to create shader resource view");
        }

        template <size_t Cache>
        [[nodiscard]] constexpr Ctrl::Cache fetch_cache(ID3D11Texture2D *tex) const
            requires(NCache > 0)
        {
            auto &cache = owner.caches[Cache];

            if (cache.tex == nullptr || cache.tex.Get() != tex) {
                cache.tex = tex;

                HRESULT hr = owner.device->CreateShaderResourceView(
                        tex, nullptr, cache.srv.ReleaseAndGetAddressOf());
                if (FAILED(hr))
                    throw std::runtime_error("Failed to create shader resource view");

                hr = owner.device->CreateRenderTargetView(
                        tex, nullptr, cache.rtv.ReleaseAndGetAddressOf());
                if (FAILED(hr))
                    throw std::runtime_error("Failed to create render target view");
            }

            return {cache.srv.Get(), cache.rtv.Get()};
        }

        template <size_t Temp>
        [[nodiscard]] constexpr Ctrl::Temp blit(
                ID3D11ShaderResourceView *const *src, uint32_t w, uint32_t h) const
            requires(NTemp > 0)
        {
            constexpr ID3D11UnorderedAccessView *null_uav = nullptr;
            constexpr ID3D11ShaderResourceView *null_srv = nullptr;

            auto &temp = owner.temps[Temp];

            owner.ctx->CSSetShader(owner.blit.Get(), nullptr, 0u);
            owner.ctx->CSSetUnorderedAccessViews(
                    0u, 1u, temp.uav.GetAddressOf(), nullptr);
            owner.ctx->CSSetShaderResources(0u, 1u, src);
            owner.ctx->Dispatch((w + 31u) / 32u, (h + 31u) / 32u, 1u);

            owner.ctx->CSSetUnorderedAccessViews(0u, 1u, &null_uav, nullptr);
            owner.ctx->PSSetShaderResources(0u, 1u, &null_srv);

            return {temp.srv.Get(), temp.uav.Get()};
        }

        template <size_t PS, size_t CB, size_t NSRV>
        constexpr PixelShader<PS, CB, NSRV> pixel_shader() const
            requires(NPS > 0 && NCB > 0)
        {
            return PixelShader<PS, CB, NSRV>(owner);
        }

    private:
        Direct3D &owner;
    };

    constexpr ~Direct3D() = default;
    constexpr Direct3D(
            const std::array<size_t, NCB> &cb,
            const std::array<std::span<const BYTE>, NPS> &ps) noexcept
        requires(NCB > 0 && NPS > 0)
    {
        for (size_t i = 0u; i < NCB; ++i) this->cb[i].size = cb[i];
        for (size_t i = 0u; i < NPS; ++i) this->ps[i].data = ps[i];
    }

    constexpr Direct3D(const Direct3D &) = delete;
    constexpr Direct3D &operator=(const Direct3D &) = delete;
    constexpr Direct3D(Direct3D &&) = delete;
    constexpr Direct3D &operator=(Direct3D &&) = delete;

    [[nodiscard]] constexpr Ctrl init(ID3D11Texture2D *tex, void (*reset)()) {
        ComPtr<ID3D11Device> d;
        tex->GetDevice(&d);

        if (device != nullptr && SUCCEEDED(device->GetDeviceRemovedReason())
            && device == d)
            return Ctrl(*this);

        release();
        if (reset != nullptr)
            reset();

        device = d;
        device->GetImmediateContext(&ctx);

        D3D11_SAMPLER_DESC smp_desc{
                .Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
                .AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
                .AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
                .AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
                .MipLODBias = 0.0f,
                .MaxAnisotropy = 0u,
                .ComparisonFunc = D3D11_COMPARISON_NEVER,
                .BorderColor = {0.0f, 0.0f, 0.0f, 0.0f},
                .MinLOD = 0.0f,
                .MaxLOD = D3D11_FLOAT32_MAX,
        };

        HRESULT hr = device->CreateSamplerState(&smp_desc, &smp);
        if (FAILED(hr)) {
            release();
            throw std::runtime_error("Failed to create sampler state");
        }

        std::span<const BYTE> vs_data(g_fullscreen);
        hr = device->CreateVertexShader(
                vs_data.data(), vs_data.size_bytes(), nullptr, &vs);
        if (FAILED(hr)) {
            release();
            throw std::runtime_error("Failed to create vertex shader");
        }

        if constexpr (NCB > 0) {
            D3D11_BUFFER_DESC buf_desc{
                    .ByteWidth = 0u,
                    .Usage = D3D11_USAGE_DYNAMIC,
                    .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
                    .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
                    .MiscFlags = 0u,
                    .StructureByteStride = 0u,
            };

            for (auto &c : cb) {
                buf_desc.ByteWidth = static_cast<uint32_t>(c.size);
                hr = device->CreateBuffer(&buf_desc, nullptr, &c.handle);
                if (FAILED(hr)) {
                    release();
                    throw std::runtime_error("Failed to create constant buffer");
                }
            }
        }

        if constexpr (NPS > 0) {
            for (auto &p : ps) {
                hr = device->CreatePixelShader(
                        p.data.data(), p.data.size_bytes(), nullptr, &p.handle);
                if (FAILED(hr)) {
                    release();
                    throw std::runtime_error("Failed to create pixel shader");
                }
            }
        }

        if constexpr (NTemp > 0) {
            std::span<const BYTE> blit_data(g_blit);
            hr = device->CreateComputeShader(
                    blit_data.data(), blit_data.size_bytes(), nullptr, &blit);
            if (FAILED(hr)) {
                release();
                throw std::runtime_error("Failed to create compute shader");
            }

            constexpr D3D11_TEXTURE2D_DESC tex_desc{
                    .Width = 16384u,
                    .Height = 16384u,
                    .MipLevels = 1u,
                    .ArraySize = 1u,
                    .Format = DXGI_FORMAT_R16G16B16A16_FLOAT,
                    .SampleDesc = {.Count = 1u, .Quality = 0u},
                    .Usage = D3D11_USAGE_DEFAULT,
                    .BindFlags =
                            D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
                    .CPUAccessFlags = 0u,
                    .MiscFlags = 0u,
            };

            for (auto &t : temps) {
                hr = device->CreateTexture2D(&tex_desc, nullptr, &t.tex);
                if (FAILED(hr)) {
                    release();
                    throw std::runtime_error("Failed to create texture2d");
                }

                hr = device->CreateShaderResourceView(t.tex.Get(), nullptr, &t.srv);
                if (FAILED(hr)) {
                    release();
                    throw std::runtime_error("Failed to create shader resource view");
                }

                hr = device->CreateUnorderedAccessView(t.tex.Get(), nullptr, &t.uav);
                if (FAILED(hr)) {
                    release();
                    throw std::runtime_error("Failed to create unordered access view");
                }
            }
        }

        return Ctrl(*this);
    }

    constexpr void release() {
        if constexpr (NTemp > 0) {
            blit.Reset();

            for (auto &t : temps) {
                t.srv.Reset();
                t.uav.Reset();
                t.tex.Reset();
            }
        }

        if constexpr (NCache > 0) {
            for (auto &c : caches) {
                c.srv.Reset();
                c.rtv.Reset();
                c.tex.Reset();
            }
        }

        if constexpr (NPS > 0) {
            for (auto &p : ps) p.handle.Reset();
        }

        if constexpr (NCB > 0) {
            for (auto &c : cb) c.handle.Reset();
        }

        vs.Reset();
        smp.Reset();
        ctx.Reset();
        device.Reset();
    }

private:
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    struct ConstantBuffer {
        size_t size;
        ComPtr<ID3D11Buffer> handle;
    };

    template <typename T>
    struct Shader {
        std::span<const BYTE> data;
        ComPtr<T> handle;
    };

    struct Cache {
        ComPtr<ID3D11Texture2D> tex;
        ComPtr<ID3D11ShaderResourceView> srv;
        ComPtr<ID3D11RenderTargetView> rtv;
    };

    struct Temp {
        ComPtr<ID3D11Texture2D> tex;
        ComPtr<ID3D11ShaderResourceView> srv;
        ComPtr<ID3D11UnorderedAccessView> uav;
    };

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> ctx;

    ComPtr<ID3D11SamplerState> smp;
    ComPtr<ID3D11VertexShader> vs;
    std::array<ConstantBuffer, NCB> cb;
    std::array<Shader<ID3D11PixelShader>, NPS> ps;

    std::array<Cache, NCache> caches;
    std::array<Temp, NTemp> temps;
    ComPtr<ID3D11ComputeShader> blit;
};
}  // namespace lut::direct3d
