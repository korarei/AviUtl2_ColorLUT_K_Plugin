#pragma once

#include <windows.h>

#include <logger2.h>
#include <plugin2.h>

namespace importer {
constexpr wchar_t name[] = L"LUTフィルタをレイヤーに追加";
constexpr wchar_t filefilter[] = L"Cube LUT File (*.cube)\0*.cube\0\0";

void
initialize_logger(LOG_HANDLE *log);

void
add_filter(EDIT_SECTION *edit, const wchar_t *file);
}  // namespace importer
