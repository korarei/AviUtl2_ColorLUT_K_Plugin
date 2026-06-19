#pragma once

#include <windows.h>

#include <string>

#include <logger2.h>

namespace lut::aviutl {
class Logger {
  public:
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    static void Init(LOG_HANDLE* logger);

    static void Log(const std::wstring& message);
    static void Debug(const std::wstring& message);
    static void Info(const std::wstring& message);
    static void Warning(const std::wstring& message);
    static void Error(const std::wstring& message);

  private:
    constexpr Logger() = default;
    constexpr ~Logger() = default;

    [[nodiscard]] static Logger& Instance();

    LOG_HANDLE* logger_ = nullptr;
};
}  // namespace lut::aviutl
