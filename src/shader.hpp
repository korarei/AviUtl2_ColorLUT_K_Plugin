#pragma once

#include <span>

using BYTE = uint8_t;

#if __has_include(<identity.h>)
#include <identity.h>
#else
inline const uint8_t g_identity[] = {0};
#endif

namespace shader {
inline constexpr std::span<const uint8_t> identity{g_identity};
}  // namespace shader
