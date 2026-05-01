#pragma once

#include <windows.h>

#include <logger2.h>
#include <plugin2.h>

namespace lut::filter {
void
init(HOST_APP_TABLE *host, LOG_HANDLE *logger) noexcept;

void
deinit();
}  // namespace lut::filter
