#pragma once

#include "common.hpp"

namespace hald2cube {
extern FILTER_PLUGIN_TABLE info;

void
initialize_logger(LOG_HANDLE *log);

void
set_owner(HWND hwnd);
}  // namespace hald2cube
