#pragma once

#include <windows.h>

#include <logger2.h>
#include <plugin2.h>

namespace lut::io {
void
init(HOST_APP_TABLE *host, LOG_HANDLE *logger) noexcept;
}  // namespace lut::io
