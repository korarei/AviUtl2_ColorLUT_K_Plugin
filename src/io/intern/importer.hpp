#pragma once

#include <windows.h>

#include <logger2.h>
#include <plugin2.h>

namespace lut::io::importer {
constexpr wchar_t name[] = L"LUTフィルタをレイヤーに追加";
constexpr wchar_t filefilter[] = L"Cube LUT File (*.cube)\0*.cube\0\0";

void
init(LOG_HANDLE *logger) noexcept;

void
on_drop(EDIT_SECTION *edit, const wchar_t *file);
}  // namespace lut::io::importer
