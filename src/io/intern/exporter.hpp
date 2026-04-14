#pragma once

#include <windows.h>

#include <logger2.h>
#include <output2.h>

namespace exporter {
extern OUTPUT_PLUGIN_TABLE info;

void
initialize_logger(LOG_HANDLE *log);
}  // namespace exporter
