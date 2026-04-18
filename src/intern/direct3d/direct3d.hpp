#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

#include <d3d11.h>
#include <wrl/client.h>

#include "../pixel/pixel.hpp"

#include <fullscreen.h>

namespace lut::direct3d {
template <size_t N, size_t M>
class Direct3D {
public:
    class Ctrl {
    public:
        class PixelShader {
        public:
            using SRVs = std::vector<ID3D11ShaderResourceView *>;

            constexpr ~PixelShader() = default;
            constexpr PixelShader(Direct3D &owner, size_t ps, size_t cb, float w, float h) :
                ps(ps), cb(cb), w(w), h(h), owner(owner) {}

            constexpr PixelShader(const PixelShader &) = delete;
            constexpr PixelShader &operator=(const PixelShader &) = delete;
            constexpr PixelShader(PixelShader &&) = delete;
            constexpr PixelShader &operator=(PixelShader &&) = delete;

            template <typename T>
            constexpr void operator()(ID3D11Texture2D *dst, const SRVs &srvs, const T &data) const {
                constexpr ID3D11RenderTargetView *null_rtv = nullptr;
                constexpr ID3D11ShaderResourceView *null_views[16] = {nullptr};

                const size_t size = sizeof(T);
                const uint32_t views = static_cast<uint32_t>(srvs.size());
                const bool has_srv = views > 0u;

                if (size > owner.cb_sizes[cb])
                    throw std::runtime_error("Constant buffer size mismatch");

                ComPtr<ID3D11RenderTargetView> rtv;
                HRESULT hr = owner.device->CreateRenderTargetView(dst, nullptr, &rtv);
                if (FAILED(hr))
                    throw std::runtime_error("Failed to create render target view");

                D3D11_MAPPED_SUBRESOURCE mapped{};
                hr = owner.ctx->Map(owner.cb[cb].Get(), 0u, D3D11_MAP_WRITE_DISCARD, 0u, &mapped);
                if (FAILED(hr))
                    throw std::runtime_error("Failed to map constant buffer");

                std::memcpy(mapped.pData, &data, size);
                owner.ctx->Unmap(owner.cb[cb].Get(), 0u);

                D3D11_VIEWPORT vp{0.0f, 0.0f, w, h, 0.0f, 1.0f};

                owner.ctx->IASetInputLayout(nullptr);
                owner.ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                owner.ctx->VSSetShader(owner.vs.Get(), nullptr, 0u);
                owner.ctx->PSSetShader(owner.ps[ps].Get(), nullptr, 0u);
                owner.ctx->PSSetSamplers(0u, 1u, owner.smp.GetAddressOf());
                owner.ctx->PSSetConstantBuffers(0u, 1u, owner.cb[cb].GetAddressOf());
                if (has_srv)
                    owner.ctx->PSSetShaderResources(0u, views, srvs.data());

                owner.ctx->RSSetViewports(1u, &vp);
                owner.ctx->OMSetRenderTargets(1u, rtv.GetAddressOf(), nullptr);

                owner.ctx->Draw(3u, 0u);

                owner.ctx->OMSetRenderTargets(1u, &null_rtv, nullptr);
                if (has_srv)
                    owner.ctx->PSSetShaderResources(0u, views, null_views);
            }

        private:
            size_t ps, cb;
            float w, h;
            Direct3D &owner;
        };

        constexpr ~Ctrl() = default;
        constexpr Ctrl(Direct3D &owner) : owner(owner) {}

        constexpr Ctrl(const Ctrl &) = delete;
        constexpr Ctrl &operator=(const Ctrl &) = delete;
        constexpr Ctrl(Ctrl &&) = delete;
        constexpr Ctrl &operator=(Ctrl &&) = delete;

        constexpr void create_texture(ID3D11Texture1D **dst, uint32_t w, uint32_t n, const float *data) const {
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

        constexpr void create_texture(ID3D11Texture3D **dst, uint32_t w, uint32_t h, uint32_t d,
                                      const pixel::RGBAF32 *data) const {
            constexpr uint32_t size = static_cast<uint32_t>(sizeof(pixel::RGBAF32));

            D3D11_TEXTURE3D_DESC desc{
                    .Width = w,
                    .Height = h,
                    .Depth = d,
                    .MipLevels = 1u,
                    .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
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

        constexpr void create_texture(ID3D11Texture3D **dst, uint32_t w, uint32_t h, uint32_t d,
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

        constexpr void create_srv(ID3D11ShaderResourceView **srv, ID3D11Resource *resource) const {
            if (FAILED(owner.device->CreateShaderResourceView(resource, nullptr, srv)))
                throw std::runtime_error("Failed to create shader resource view");
        }

        constexpr void duplicate(ID3D11Texture2D **dst, ID3D11Texture2D *src) const {
            D3D11_TEXTURE2D_DESC desc;
            src->GetDesc(&desc);

            if (FAILED(owner.device->CreateTexture2D(&desc, nullptr, dst)))
                throw std::runtime_error("Failed to create texture2d");

            owner.ctx->CopyResource(*dst, src);
        }

        constexpr PixelShader as_ps(size_t ps, size_t cb, int w, int h) const {
            return PixelShader(owner, ps, cb, static_cast<float>(w), static_cast<float>(h));
        }

    private:
        Direct3D &owner;
    };

    constexpr ~Direct3D() = default;
    constexpr Direct3D(const std::array<size_t, N> &cb_sizes, const std::array<std::span<const BYTE>, M> &ps_data) :
        cb_sizes(cb_sizes), ps_data(ps_data) {}

    constexpr Direct3D(const Direct3D &) = delete;
    constexpr Direct3D &operator=(const Direct3D &) = delete;
    constexpr Direct3D(Direct3D &&) = delete;
    constexpr Direct3D &operator=(Direct3D &&) = delete;

    [[nodiscard]] constexpr Ctrl init(ID3D11Texture2D *tex, void (*reset)()) {
        ComPtr<ID3D11Device> d;
        tex->GetDevice(&d);

        if (device != nullptr && SUCCEEDED(device->GetDeviceRemovedReason()) && device == d)
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
        hr = device->CreateVertexShader(vs_data.data(), vs_data.size_bytes(), nullptr, &vs);
        if (FAILED(hr)) {
            release();
            throw std::runtime_error("Failed to create vertex shader");
        }

        D3D11_BUFFER_DESC buf_desc{
                .ByteWidth = 0u,
                .Usage = D3D11_USAGE_DYNAMIC,
                .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
                .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
                .MiscFlags = 0u,
                .StructureByteStride = 0u,
        };

        for (size_t i = 0; i < N; ++i) {
            buf_desc.ByteWidth = static_cast<uint32_t>(cb_sizes[i]);
            hr = device->CreateBuffer(&buf_desc, nullptr, &cb[i]);
            if (FAILED(hr)) {
                release();
                throw std::runtime_error("Failed to create constant buffer");
            }
        }

        for (size_t i = 0; i < M; ++i) {
            hr = device->CreatePixelShader(ps_data[i].data(), ps_data[i].size_bytes(), nullptr, &ps[i]);
            if (FAILED(hr)) {
                release();
                throw std::runtime_error("Failed to create pixel shader");
            }
        }

        return Ctrl(*this);
    }

    constexpr void release() {
        for (auto &p : ps) p.Reset();
        for (auto &c : cb) c.Reset();
        vs.Reset();
        smp.Reset();
        ctx.Reset();
        device.Reset();
    }

private:
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    std::array<size_t, N> cb_sizes;
    std::array<std::span<const BYTE>, M> ps_data;

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> ctx;

    ComPtr<ID3D11SamplerState> smp;
    ComPtr<ID3D11VertexShader> vs;
    std::array<ComPtr<ID3D11Buffer>, N> cb;
    std::array<ComPtr<ID3D11PixelShader>, M> ps;
};
}  // namespace lut::direct3d
