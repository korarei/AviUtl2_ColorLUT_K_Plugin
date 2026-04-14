#pragma once

#include <cstdint>  // IWYU pragma: keep

#include <windows.h>

#pragma warning(push)
#pragma warning(disable : 4201)  // 非標準の無名構造体 (filter2.h FILTER_ITEM_COLOR)
#include <filter2.h>
#pragma warning(pop)
#include <logger2.h>

namespace color_lut {
extern FILTER_PLUGIN_TABLE info;

void
clear_cache();

void
initialize_logger(LOG_HANDLE *log);
}  // namespace color_lut
