#pragma once

#include <wrl/client.h>

struct IWICImagingFactory2;

namespace lut::wic {
class WIC {
public:
    [[nodiscard]] static IWICImagingFactory2 *get_factory();
    static void release();

private:
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    ComPtr<IWICImagingFactory2> factory;

    ~WIC() = default;
    WIC();

    WIC(const WIC &) = delete;
    WIC &operator=(const WIC &) = delete;
    WIC(WIC &&) = delete;
    WIC &operator=(WIC &&) = delete;

    [[nodiscard]] static WIC &instance();
};
};  // namespace lut::wic
