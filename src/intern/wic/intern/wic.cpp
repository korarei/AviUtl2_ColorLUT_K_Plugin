#include "wic.hpp"

#include <stdexcept>

#include <wincodec.h>

namespace lut::wic {
IWICImagingFactory2 *
WIC::get_factory() {
    auto *f = instance().factory.Get();
    if (f == nullptr)
        throw std::runtime_error("Failed to load WIC factory");

    return f;
}

void
WIC::release() {
    instance().factory.Reset();
}

WIC::WIC() { CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)); }

WIC &
WIC::instance() {
    static WIC inst;
    return inst;
}
};  // namespace lut::wic
