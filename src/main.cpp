#include <cstdint>

#include "common.hpp"

#include "baker.hpp"
#include "filter.hpp"

#ifndef VERSION
#define VERSION L"0.1.0"
#endif

extern "C" {
void
InitializeLogger(LOG_HANDLE *log) {
    color_lut::initialize_logger(log);
    hald2cube::initialize_logger(log);
}

bool
InitializePlugin(DWORD ver) {
    return ver >= 2003000;
}

void
RegisterPlugin(HOST_APP_TABLE *host) {
    host->set_plugin_information(L"ColorLUT_K v" VERSION L" by Korarei");

    host->register_filter_plugin(&color_lut::info);
    host->register_clear_cache_handler([]([[maybe_unused]] EDIT_SECTION *edit) { color_lut::clear_cache(); });

    host->register_filter_plugin(&hald2cube::info);
}
}
