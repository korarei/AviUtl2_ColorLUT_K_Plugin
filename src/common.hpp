#pragma once

#include <cstdint>  // IWYU pragma: keep

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#pragma warning(push)
#pragma warning(disable : 4201)  // 非標準の無名構造体 (filter2.h FILTER_ITEM_COLOR)
#include <filter2.h>
#include <logger2.h>
#include <plugin2.h>
#pragma warning(pop)
