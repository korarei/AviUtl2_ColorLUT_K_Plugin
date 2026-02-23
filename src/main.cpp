#include "common.hpp"

#include "exporter.hpp"
#include "filter.hpp"
#include "identity.hpp"
// #include "importer.hpp"

#ifndef VERSION
#define VERSION L"0.1.0"
#endif

extern "C" {
void
InitializeLogger(LOG_HANDLE *log) {
    color_lut::initialize_logger(log);
    identity::initialize_logger(log);
    exporter::initialize_logger(log);
    // importer::initialize_logger(log);
}

DWORD
RequiredVersion() { return 2003400; }

bool
InitializePlugin(DWORD ver) {
    return ver >= RequiredVersion();
}

void
RegisterPlugin(HOST_APP_TABLE *host) {
    // auto *edit_handle = host->create_edit_handle();

    host->set_plugin_information(L"ColorLUT_K v" VERSION L" by Korarei");

    host->register_filter_plugin(&color_lut::info);
    host->register_clear_cache_handler([]([[maybe_unused]] EDIT_SECTION *edit) { color_lut::clear_cache(); });

    host->register_filter_plugin(&identity::info);

    host->register_output_plugin(&exporter::info);

    // host->register_input_plugin(&importer::info);
    // importer::initialize_edit_handle(edit_handle);
}
}
