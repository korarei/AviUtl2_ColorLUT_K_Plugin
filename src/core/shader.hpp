#pragma once

#include <span>

using BYTE = uint8_t;

#if __has_include(<identity.cs.h>)
#include <identity.cs.h>
#else
inline const BYTE g_identity_cs[] = {0};
#endif

#if __has_include(<blend.ps.h>)
#include <blend.ps.h>
#else
inline const BYTE g_blend_ps[] = {0};
#endif

namespace shader {
struct Blend {
    static constexpr std::span<const BYTE> ps{g_blend_ps};
};

struct Identity {
    static constexpr std::span<const BYTE> cs{g_identity_cs};
};
}  // namespace shader
