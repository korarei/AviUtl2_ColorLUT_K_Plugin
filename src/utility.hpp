#pragma once

#include <filesystem>
#include <string>

namespace string {
inline std::wstring
to_wstr(const std::u8string &src) {
    return std::filesystem::path(src).wstring();
}

inline std::u8string
to_u8str(const std::wstring &src) {
    return std::filesystem::path(src).u8string();
}
}  // namespace string
