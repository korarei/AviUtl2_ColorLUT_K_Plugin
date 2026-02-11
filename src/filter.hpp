#pragma once

#include "common.hpp"

namespace color_lut {
extern FILTER_PLUGIN_TABLE info;

void
clear_cache();

void
initialize_logger(LOG_HANDLE *log);
}  // namespace color_lut
