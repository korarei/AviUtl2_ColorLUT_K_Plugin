#include <cstdint>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <plugin2.h>

#include "apply.hpp"

#ifndef VERSION
#define VERSION L"0.1.0"
#endif

extern "C" {
void
InitializeLogger(LOG_HANDLE *log) {
    color_lut::initialize_logger(log);
}

bool
InitializePlugin(DWORD ver) {
    return ver >= 2002600;
}

void
RegisterPlugin(HOST_APP_TABLE *host) {
    host->set_plugin_information(L"ColorLUT_K v" VERSION L" by Korarei");

    host->register_filter_plugin(&color_lut::info);
    host->register_clear_cache_handler([]([[maybe_unused]] EDIT_SECTION *edit) { color_lut::reload_lut(); });
}
}
