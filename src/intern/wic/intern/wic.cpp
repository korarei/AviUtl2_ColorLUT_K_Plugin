#include "../wic.hpp"

#include <wincodec.h>

namespace lut::wic {
IWICImagingFactory2* WIC::factory() { return Instance().factory_.Get(); }

void WIC::Deinit() { Instance().factory_.Reset(); }

WIC::WIC() { CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory_)); }

WIC& WIC::Instance() {
    static WIC inst;
    return inst;
}
}  // namespace lut::wic
