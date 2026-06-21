#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

#include <intern/pixel/pixel.hpp>

#include <fullscreen.h>

namespace lut::direct3d {
struct PixelShaderDesc {
    std::span<const BYTE> shader{};
    size_t cbuffer = 0uz;
    std::optional<D3D11_SAMPLER_DESC> sampler = std::nullopt;
};

template <size_t NumCaches, size_t NumPixelShaders>
class Direct3D {
  public:
    using Float16 = pixel::Float16;
    using RGBAF16 = pixel::RGBAF16;
    using Tex1D = ID3D11Texture1D;
    using Tex2D = ID3D11Texture2D;
    using Tex3D = ID3D11Texture3D;
    using SRV = ID3D11ShaderResourceView;
    using RTV = ID3D11RenderTargetView;

    class Ctrl {
      public:
        explicit Ctrl(Direct3D& owner) : owner_(owner) {}
        ~Ctrl() = default;

        Ctrl(const Ctrl&) = delete;
        Ctrl& operator=(const Ctrl&) = delete;
        Ctrl(Ctrl&&) = delete;
        Ctrl& operator=(Ctrl&&) = delete;

        void CreateTexture(Tex1D** dst, uint32_t w, uint32_t n, const Float16* data) const {
            D3D11_TEXTURE1D_DESC desc{
                .Width = w,
                .MipLevels = 1u,
                .ArraySize = n,
                .Format = DXGI_FORMAT_R16_FLOAT,
                .Usage = D3D11_USAGE_DEFAULT,
                .BindFlags = D3D11_BIND_SHADER_RESOURCE,
                .CPUAccessFlags = 0u,
                .MiscFlags = 0u,
            };

            if (data == nullptr) {
                if (FAILED(owner_.device_->CreateTexture1D(&desc, nullptr, dst))) {
                    throw std::runtime_error("Failed to create texture1d");
                }

                return;
            }

            std::vector<D3D11_SUBRESOURCE_DATA> init(n);
            for (uint32_t i = 0u; i < n; ++i) {
                init[i].pSysMem = data + i * w;
                init[i].SysMemPitch = 0u;
                init[i].SysMemSlicePitch = 0u;
            }

            if (FAILED(owner_.device_->CreateTexture1D(&desc, init.data(), dst))) {
                throw std::runtime_error("Failed to create texture1d");
            }
        }

        void CreateTexture(Tex3D** dst, uint32_t w, uint32_t h, uint32_t d, const RGBAF16* data) const {
            constexpr uint32_t kPixelSize = static_cast<uint32_t>(sizeof(RGBAF16));

            D3D11_TEXTURE3D_DESC desc{
                .Width = w,
                .Height = h,
                .Depth = d,
                .MipLevels = 1u,
                .Format = DXGI_FORMAT_R16G16B16A16_FLOAT,
                .Usage = D3D11_USAGE_DEFAULT,
                .BindFlags = D3D11_BIND_SHADER_RESOURCE,
                .CPUAccessFlags = 0u,
                .MiscFlags = 0u,
            };

            if (data == nullptr) {
                if (FAILED(owner_.device_->CreateTexture3D(&desc, nullptr, dst))) {
                    throw std::runtime_error("Failed to create texture3d");
                }

                return;
            }

            D3D11_SUBRESOURCE_DATA init{
                .pSysMem = data,
                .SysMemPitch = w * kPixelSize,
                .SysMemSlicePitch = w * h * kPixelSize,
            };

            if (FAILED(owner_.device_->CreateTexture3D(&desc, &init, dst))) {
                throw std::runtime_error("Failed to create texture3d");
            }
        }

        void CreateTexture(Tex3D** dst, uint32_t w, const RGBAF16* data) const { CreateTexture(dst, w, w, w, data); }

        void CreateShaderResourceView(SRV** srv, ID3D11Resource* resource) const {
            if (FAILED(owner_.device_->CreateShaderResourceView(resource, nullptr, srv))) {
                throw std::runtime_error("Failed to create shader resource view");
            }
        }

        [[nodiscard]] RTV* GetBackBuffer(Tex2D* tex) const {
            auto& buf = owner_.back_buffer_;

            if (buf.tex == nullptr || buf.tex.Get() != tex) {
                buf.tex = tex;

                HRESULT hr = owner_.device_->CreateRenderTargetView(tex, nullptr, buf.rtv.ReleaseAndGetAddressOf());
                if (FAILED(hr)) {
                    throw std::runtime_error("Failed to create render target view");
                }
            }

            return buf.rtv.Get();
        }

        template <size_t Index>
            requires(NumCaches > 0uz)
        [[nodiscard]] SRV* CopyBuffer(Tex2D* tex, uint32_t w, uint32_t h) const {
            static_assert(Index < NumCaches);

            auto& buf = owner_.caches_[Index];

            if (buf.tex == nullptr || buf.w != w || buf.h != h) {
                D3D11_TEXTURE2D_DESC desc;
                tex->GetDesc(&desc);

                HRESULT hr = owner_.device_->CreateTexture2D(&desc, nullptr, buf.tex.ReleaseAndGetAddressOf());
                if (FAILED(hr)) {
                    throw std::runtime_error("Failed to create texture2d");
                }

                hr = owner_.device_->CreateShaderResourceView(buf.tex.Get(), nullptr, buf.srv.ReleaseAndGetAddressOf());
                if (FAILED(hr)) {
                    throw std::runtime_error("Failed to create shader resource view");
                }

                buf.w = w, buf.h = h;
            }

            owner_.ctx_->CopyResource(buf.tex.Get(), tex);

            return buf.srv.Get();
        }

        template <size_t Index, typename T>
            requires(NumPixelShaders > 0uz)
        void Draw(RTV* const* dst, int w, int h, const T* data = nullptr) const {
            static_assert(Index < NumPixelShaders);

            constexpr RTV* kNullRTV = nullptr;

            const auto& ps = owner_.ps_[Index];

            if (data != nullptr && ps.desc.cbuffer > 0uz) {
                if (ps.desc.cbuffer != sizeof(T)) {
                    throw std::runtime_error("Constant buffer size mismatch");
                }

                D3D11_MAPPED_SUBRESOURCE mapped{};
                HRESULT hr = owner_.ctx_->Map(ps.binding.cb.Get(), 0u, D3D11_MAP_WRITE_DISCARD, 0u, &mapped);

                if (FAILED(hr)) {
                    throw std::runtime_error("Failed to map constant buffer");
                }

                std::memcpy(mapped.pData, data, sizeof(T));
                owner_.ctx_->Unmap(ps.binding.cb.Get(), 0u);

                owner_.ctx_->PSSetConstantBuffers(0u, 1u, ps.binding.cb.GetAddressOf());
            }

            if (ps.binding.smp != nullptr) {
                owner_.ctx_->PSSetSamplers(0u, 1u, ps.binding.smp.GetAddressOf());
            }

            D3D11_VIEWPORT vp{0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h), 0.0f, 1.0f};

            owner_.ctx_->IASetInputLayout(nullptr);
            owner_.ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            owner_.ctx_->VSSetShader(owner_.vs_.Get(), nullptr, 0u);
            owner_.ctx_->PSSetShader(ps.binding.ps.Get(), nullptr, 0u);

            owner_.ctx_->RSSetViewports(1u, &vp);
            owner_.ctx_->OMSetRenderTargets(1u, dst, nullptr);

            owner_.ctx_->Draw(3u, 0u);

            owner_.ctx_->OMSetRenderTargets(1u, &kNullRTV, nullptr);
        }

        template <size_t Index, typename T>
            requires(NumPixelShaders > 0uz)
        void Draw(RTV* const* dst, int w, int h, std::initializer_list<SRV*> inputs, const T* data = nullptr) const {
            static_assert(Index < NumPixelShaders);

            static constexpr SRV* kNullSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {nullptr};

            if (!inputs.empty()) {
                owner_.ctx_->PSSetShaderResources(0u, static_cast<uint32_t>(inputs.size()), inputs.data());
                Draw<Index>(dst, w, h, data);
                owner_.ctx_->PSSetShaderResources(0u, static_cast<uint32_t>(inputs.size()), kNullSRVs);
            } else {
                Draw<Index>(dst, w, h, data);
            }
        }

      private:
        Direct3D& owner_;
    };

    Direct3D(const Direct3D&) = delete;
    Direct3D& operator=(const Direct3D&) = delete;
    Direct3D(Direct3D&&) = delete;
    Direct3D& operator=(Direct3D&&) = delete;

    Direct3D(const std::array<PixelShaderDesc, NumPixelShaders>& ps_descs) {
        if constexpr (NumPixelShaders > 0uz) {
            for (size_t i = 0uz; i < NumPixelShaders; ++i) {
                ps_[i].desc = ps_descs[i];
            }
        }
    }
    ~Direct3D() = default;

    [[nodiscard]] Ctrl Init(Tex2D* tex, void (*reset)() = nullptr) {
        ComPtr<ID3D11Device> device;
        tex->GetDevice(&device);

        if (device_ != nullptr && SUCCEEDED(device_->GetDeviceRemovedReason()) && device_ == device) {
            return Ctrl(*this);
        }

        Release();
        if (reset != nullptr) {
            reset();
        }

        device_ = device;
        device_->GetImmediateContext(&ctx_);

        HRESULT hr = device_->CreateVertexShader(g_fullscreen, sizeof(g_fullscreen), nullptr, &vs_);
        if (FAILED(hr)) {
            Release();
            throw std::runtime_error("Failed to create vertex shader");
        }

        if constexpr (NumPixelShaders > 0uz) {
            D3D11_BUFFER_DESC cb_desc{
                .ByteWidth = 0u,
                .Usage = D3D11_USAGE_DYNAMIC,
                .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
                .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
                .MiscFlags = 0u,
                .StructureByteStride = 0u,
            };

            for (auto& [desc, binding] : ps_) {
                if (desc.shader.empty()) {
                    Release();
                    throw std::runtime_error("Failed to create pixel shader");
                }

                hr = device_->CreatePixelShader(desc.shader.data(), desc.shader.size(), nullptr, &binding.ps);
                if (FAILED(hr)) {
                    Release();
                    throw std::runtime_error("Failed to create pixel shader");
                }

                if (desc.cbuffer > 0uz) {
                    cb_desc.ByteWidth = static_cast<uint32_t>(desc.cbuffer);
                    hr = device_->CreateBuffer(&cb_desc, nullptr, &binding.cb);
                    if (FAILED(hr)) {
                        Release();
                        throw std::runtime_error("Failed to create constant buffer");
                    }
                }

                if (desc.sampler.has_value()) {
                    hr = device_->CreateSamplerState(&(*desc.sampler), &binding.smp);
                    if (FAILED(hr)) {
                        Release();
                        throw std::runtime_error("Failed to create sampler state");
                    }
                }
            }
        }

        return Ctrl(*this);
    }

    void Release() {
        if constexpr (NumCaches > 0uz) {
            for (auto& cache : caches_) {
                cache.w = 0u, cache.h = 0u;
                cache.srv.Reset();
                cache.tex.Reset();
            }
        }

        back_buffer_.rtv.Reset();
        back_buffer_.tex.Reset();

        if constexpr (NumPixelShaders > 0uz) {
            for (auto& [_, binding] : ps_) {
                binding.smp.Reset();
                binding.cb.Reset();
                binding.ps.Reset();
            }
        }

        vs_.Reset();

        ctx_.Reset();
        device_.Reset();
    }

  private:
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    struct PixelShaderBinding {
        ComPtr<ID3D11PixelShader> ps = nullptr;
        ComPtr<ID3D11Buffer> cb = nullptr;
        ComPtr<ID3D11SamplerState> smp = nullptr;
    };

    struct PixelShader {
        PixelShaderDesc desc{};
        PixelShaderBinding binding{};
    };

    struct BackBuffer {
        ComPtr<Tex2D> tex = nullptr;
        ComPtr<RTV> rtv = nullptr;
    };

    struct CacheBuffer {
        uint32_t w = 0u, h = 0u;
        ComPtr<Tex2D> tex = nullptr;
        ComPtr<SRV> srv = nullptr;
    };

    ComPtr<ID3D11Device> device_ = nullptr;
    ComPtr<ID3D11DeviceContext> ctx_ = nullptr;

    ComPtr<ID3D11VertexShader> vs_ = nullptr;
    std::array<PixelShader, NumPixelShaders> ps_{};

    BackBuffer back_buffer_{};
    std::array<CacheBuffer, NumCaches> caches_{};
};
}  // namespace lut::direct3d
