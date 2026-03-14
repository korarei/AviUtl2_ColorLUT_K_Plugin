#pragma once

#include <cstdint>

#include <d2d1_3.h>
#include <d2d1effectauthor.h>
#include <d2d1effecthelpers.h>
#include <guiddef.h>
#include <windows.h>
#include <winnt.h>
#include <wrl/client.h>

// pwsh: [Guid]::NewGuid()
DEFINE_GUID(CLSID_Blend, 0x976b0ab6, 0x5baf, 0x4d4d, 0xb7, 0xe3, 0xeb, 0x5f, 0x34, 0x21, 0x2b, 0xd7);

class Blend final : public ID2D1EffectImpl, public ID2D1DrawTransform {
public:
    enum Property : uint32_t {
        Mode,
        Opacity,
        Clamp,
    };

    // x64ならstdcallいらない気がするが一応
    static HRESULT _stdcall CreateEffect(IUnknown **effect);
    static HRESULT Register(ID2D1Factory3 *factory);

    IFACEMETHODIMP_(ULONG) AddRef() noexcept override;
    IFACEMETHODIMP_(ULONG) Release() noexcept override;
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) noexcept override;

    IFACEMETHODIMP Initialize(ID2D1EffectContext *ctx, ID2D1TransformGraph *graph) noexcept override;
    IFACEMETHODIMP PrepareForRender(D2D1_CHANGE_TYPE change) noexcept override;
    IFACEMETHODIMP SetGraph(ID2D1TransformGraph *graph) noexcept override;

    IFACEMETHODIMP_(uint32_t) GetInputCount() const noexcept override;
    IFACEMETHODIMP MapOutputRectToInputRects(const D2D1_RECT_L *out_rect, D2D1_RECT_L *in_rects,
                                             uint32_t count) const noexcept override;
    IFACEMETHODIMP MapInputRectsToOutputRect(const D2D1_RECT_L *in_rects, const D2D1_RECT_L *in_opqs, uint32_t count,
                                             D2D1_RECT_L *out_rect, D2D1_RECT_L *out_opq) noexcept override;
    IFACEMETHODIMP MapInvalidRect(uint32_t idx, D2D1_RECT_L in_rect, D2D1_RECT_L *out_rect) const noexcept override;
    IFACEMETHODIMP SetDrawInfo(ID2D1DrawInfo *info) noexcept override;

    int GetMode() const noexcept;
    float GetOpacity() const noexcept;
    BOOL GetClamp() const noexcept;
    HRESULT SetMode(int v) noexcept;
    HRESULT SetOpacity(float v) noexcept;
    HRESULT SetClamp(BOOL v) noexcept;

private:
    template <class T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    Blend() = default;

    struct alignas(16) Params {
        int mode = 0;
        float opacity = 1.0f;
        BOOL clamp = FALSE;
        int _padding;
    } params;

    ComPtr<ID2D1DrawInfo> draw_info;
    ULONG ref_count = 1u;
};
