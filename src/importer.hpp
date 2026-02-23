#pragma once

#include "common.hpp"

namespace importer {
extern INPUT_PLUGIN_TABLE info;

void
initialize_logger(LOG_HANDLE *log);

void
initialize_edit_handle(EDIT_HANDLE *edit);
}  // namespace importer