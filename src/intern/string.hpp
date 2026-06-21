#pragma once

#ifdef _WIN32
#include <windows.h>
#else
#include <filesystem>
#endif

#include <charconv>
#include <string>
#include <string_view>

namespace lut::string {
inline std::wstring ToWstring(std::u8string_view string) {
#ifdef _WIN32
    if (string.empty()) {
        return {};
    }

    const char* str = reinterpret_cast<const char*>(string.data());
    const int size = static_cast<int>(string.size());

    const int wsize = MultiByteToWideChar(CP_UTF8, 0, str, size, NULL, 0);

    if (wsize == 0) {
        return {};
    }

    std::wstring wstr(wsize, 0);
    MultiByteToWideChar(CP_UTF8, 0, str, size, wstr.data(), wsize);

    return wstr;
#else
    return std::filesystem::path(string).wstring();
#endif
}

inline std::u8string ToUtf8(std::wstring_view string) {
#ifdef _WIN32
    if (string.empty()) {
        return {};
    }

    const int wsize = static_cast<int>(string.size());

    const int size = WideCharToMultiByte(CP_UTF8, 0, string.data(), wsize, NULL, 0, NULL, NULL);

    if (size == 0) {
        return {};
    }

    std::u8string utf8(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, string.data(), wsize, reinterpret_cast<char*>(utf8.data()), size, NULL, NULL);

    return utf8;
#else
    return std::filesystem::path(string).u8string();
#endif
}

inline std::string AsString(std::u8string_view string) {
    return std::string(reinterpret_cast<const char*>(string.data()), string.size());
}

inline std::u8string AsUtf8(std::string_view string) {
    return std::u8string(reinterpret_cast<const char8_t*>(string.data()), string.size());
}

[[nodiscard]] inline bool ToNumber(std::string_view s, float& v) noexcept {
    const char* first = s.data();
    const char* last = first + s.size();
    const auto [ptr, ec] = std::from_chars(first, last, v);
    return ptr == last && ec == std::errc{};
}

[[nodiscard]] inline bool ToNumber(std::string_view s, double& v) noexcept {
    const char* first = s.data();
    const char* last = first + s.size();
    const auto [ptr, ec] = std::from_chars(first, last, v);
    return ptr == last && ec == std::errc{};
}

[[nodiscard]] inline bool ToNumber(std::string_view s, int& v, int base = 10) noexcept {
    const char* first = s.data();
    const char* last = first + s.size();
    const auto [ptr, ec] = std::from_chars(first, last, v, base);
    return ptr == last && ec == std::errc{};
}
}  // namespace lut::string
