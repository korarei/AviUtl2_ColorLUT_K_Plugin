#pragma once

#include <wrl/client.h>

struct IWICImagingFactory2;

namespace lut::wic {
class WIC {
  public:
    WIC(const WIC&) = delete;
    WIC& operator=(const WIC&) = delete;
    WIC(WIC&&) = delete;
    WIC& operator=(WIC&&) = delete;

    [[nodiscard]] static IWICImagingFactory2* factory();
    static void Deinit();

  private:
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    WIC();
    ~WIC() = default;

    [[nodiscard]] static WIC& Instance();

    ComPtr<IWICImagingFactory2> factory_;
};
}  // namespace lut::wic
