#pragma once

#include <cstdint>  // IWYU pragma: keep

#include <windows.h>

#pragma warning(push)
#pragma warning(disable : 4201)  // 非標準の無名構造体 (filter2.h FILTER_ITEM_COLOR)
#include <filter2.h>
#pragma warning(pop)
#include <logger2.h>

namespace lut::filter::transform {
extern FILTER_PLUGIN_TABLE info;

void
reset();

void
init(LOG_HANDLE *logger) noexcept;

void
deinit();
}  // namespace lut::filter::transform
