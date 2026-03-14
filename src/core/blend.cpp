#define INITGUID
#include "blend.hpp"

#include <new>

#include "shader.hpp"

#define HR(expr)               \
    do {                       \
        HRESULT hr__ = (expr); \
        if (FAILED(hr__))      \
            return hr__;       \
    } while (0)

namespace {
constexpr wchar_t xml[] =
        L"<?xml version='1.0'?>"
        L"<Effect>"
        L"  <Property name='DisplayName' type='string' value='Blend'/>"
        L"  <Property name='Author' type='string' value='Korarei'/>"
        L"  <Property name='Category' type='string' value='Compositing'/>"
        L"  <Property name='Description' type='string' value='HDR Compatible Blend Effect'/>"
        L"  <Inputs>"
        L"    <Input name='Foreground'/>"
        L"    <Input name='Background'/>"
        L"  </Inputs>"
        L"  <Property name='Mode' type='int32'>"
        L"    <Property name='DisplayName' type='string' value='Mode'/>"
        L"    <Property name='Default' type='int32' value='0'/>"
        L"  </Property>"
        L"  <Property name='Opacity' type='float'>"
        L"    <Property name='DisplayName' type='string' value='Opacity'/>"
        L"    <Property name='Default' type='float' value='1.0'/>"
        L"  </Property>"
        L"  <Property name='Clamp' type='bool'>"
        L"    <Property name='DisplayName' type='string' value='Clamp'/>"
        L"    <Property name='Default' type='bool' value='false'/>"
        L"  </Property>"
        L"</Effect>";
}  // namespace

// pwsh: [Guid]::NewGuid()
DEFINE_GUID(GUID_BlendPS, 0xace224ae, 0x323f, 0x4015, 0x96, 0xe9, 0x47, 0xed, 0x7e, 0x77, 0x91, 0x04);

// MSのチュートリアルによると1つのスレッドからしか呼ばれないのでInterlockedは不要
IFACEMETHODIMP_(ULONG)
Blend::AddRef() noexcept { return ++ref_count; }

IFACEMETHODIMP_(ULONG)
Blend::Release() noexcept {
    if (--ref_count == 0u) {
        delete this;
        return 0u;
    }

    return ref_count;
}

IFACEMETHODIMP
Blend::QueryInterface(REFIID riid, void **ppv) noexcept {
    if (!ppv)
        return E_POINTER;

    *ppv = nullptr;

    if (riid == __uuidof(ID2D1EffectImpl)) {
        *ppv = reinterpret_cast<ID2D1EffectImpl *>(this);
    } else if (riid == __uuidof(ID2D1DrawTransform)) {
        *ppv = static_cast<ID2D1DrawTransform *>(this);
    } else if (riid == __uuidof(ID2D1Transform)) {
        *ppv = static_cast<ID2D1Transform *>(this);
    } else if (riid == __uuidof(ID2D1TransformNode)) {
        *ppv = static_cast<ID2D1TransformNode *>(this);
    } else if (riid == __uuidof(IUnknown)) {
        *ppv = this;
    } else {
        return E_NOINTERFACE;
    }

    if (*ppv != nullptr)
        AddRef();

    return S_OK;
}

IFACEMETHODIMP
Blend::Initialize(ID2D1EffectContext *ctx, ID2D1TransformGraph *graph) noexcept {
    HR(ctx->LoadPixelShader(GUID_BlendPS, shader::Blend::ps.data(),
                            static_cast<uint32_t>(shader::Blend::ps.size_bytes())));

    return graph->SetSingleTransformNode(this);
}

IFACEMETHODIMP
Blend::PrepareForRender(D2D1_CHANGE_TYPE change) noexcept {
    if (draw_info == nullptr)
        return E_FAIL;

    if (change == D2D1_CHANGE_TYPE_PROPERTIES || change == D2D1_CHANGE_TYPE_GRAPH)
        return draw_info->SetPixelShaderConstantBuffer(reinterpret_cast<const BYTE *>(&params), sizeof(params));

    return S_OK;
}

IFACEMETHODIMP
Blend::SetGraph([[maybe_unused]] ID2D1TransformGraph *graph) noexcept { return S_OK; }

IFACEMETHODIMP_(uint32_t)
Blend::GetInputCount() const noexcept { return 2u; }

IFACEMETHODIMP
Blend::MapOutputRectToInputRects(const D2D1_RECT_L *out_rect, D2D1_RECT_L *in_rects, uint32_t count) const noexcept {
    if (count != 2u)
        return E_INVALIDARG;

    in_rects[0] = *out_rect;
    in_rects[1] = *out_rect;
    return S_OK;
}

IFACEMETHODIMP
Blend::MapInputRectsToOutputRect(const D2D1_RECT_L *in_rects, [[maybe_unused]] const D2D1_RECT_L *in_opqs,
                                 uint32_t count, D2D1_RECT_L *out_rect, D2D1_RECT_L *out_opq) noexcept {
    if (count != 2u)
        return E_INVALIDARG;

    out_rect->left = std::min(in_rects[0].left, in_rects[1].left);
    out_rect->top = std::min(in_rects[0].top, in_rects[1].top);
    out_rect->right = std::max(in_rects[0].right, in_rects[1].right);
    out_rect->bottom = std::max(in_rects[0].bottom, in_rects[1].bottom);

    ZeroMemory(out_opq, sizeof(*out_opq));
    return S_OK;
}

IFACEMETHODIMP
Blend::MapInvalidRect(uint32_t idx, D2D1_RECT_L in_rect, D2D1_RECT_L *out_rect) const noexcept {
    if (idx >= 2u)
        return E_INVALIDARG;

    *out_rect = in_rect;
    return S_OK;
}

IFACEMETHODIMP
Blend::SetDrawInfo(ID2D1DrawInfo *info) noexcept {
    draw_info = info;

    HR(draw_info->SetPixelShader(GUID_BlendPS));
    HR(draw_info->SetInputDescription(0, {D2D1_FILTER_MIN_MAG_MIP_LINEAR, 0u}));
    HR(draw_info->SetInputDescription(1, {D2D1_FILTER_MIN_MAG_MIP_LINEAR, 0u}));

    return S_OK;
}

int
Blend::GetMode() const noexcept {
    return params.mode;
}

float
Blend::GetOpacity() const noexcept {
    return params.opacity;
}

BOOL
Blend::GetClamp() const noexcept {
    return params.clamp;
}

HRESULT
Blend::SetMode(int v) noexcept {
    params.mode = v;
    return S_OK;
}

HRESULT
Blend::SetOpacity(float v) noexcept {
    params.opacity = v;
    return S_OK;
}

HRESULT
Blend::SetClamp(BOOL v) noexcept {
    params.clamp = v;
    return S_OK;
}

HRESULT _stdcall Blend::CreateEffect(IUnknown **effect) {
    *effect = static_cast<ID2D1EffectImpl *>(new (std::nothrow) Blend());
    return (*effect != nullptr) ? S_OK : E_OUTOFMEMORY;
}

HRESULT
Blend::Register(ID2D1Factory3 *factory) {
    static const D2D1_PROPERTY_BINDING bindings[] = {
            D2D1_VALUE_TYPE_BINDING(L"Mode", &Blend::SetMode, &Blend::GetMode),
            D2D1_VALUE_TYPE_BINDING(L"Opacity", &Blend::SetOpacity, &Blend::GetOpacity),
            D2D1_VALUE_TYPE_BINDING(L"Clamp", &Blend::SetClamp, &Blend::GetClamp),
    };

    return factory->RegisterEffectFromString(CLSID_Blend, xml, bindings, ARRAYSIZE(bindings), Blend::CreateEffect);
}
