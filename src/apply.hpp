#pragma once

#include <cstdint>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <filter2.h>
#include <logger2.h>

namespace color_lut {
extern FILTER_PLUGIN_TABLE info;

void
clear_cache();

void
initialize_logger(LOG_HANDLE *log);
}  // namespace color_lut
