#pragma once

#include <windows.h>

#include <logger2.h>
#include <output2.h>

namespace lut::io::exporter {
extern OUTPUT_PLUGIN_TABLE info;

void
init(LOG_HANDLE *logger) noexcept;
}  // namespace lut::io::exporter
