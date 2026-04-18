#include <windows.h>

#include <logger2.h>
#include <plugin2.h>

#include "filter.hpp"
#include "io.hpp"
#include "wic.hpp"

#ifndef VERSION
#define VERSION L"0.1.0"
#endif

namespace {
using namespace lut;

constinit LOG_HANDLE *logger = nullptr;
constinit COMMON_PLUGIN_TABLE info = {
        .name = L"ColorLUT_K",
        .information = L"ColorLUT_K v" VERSION L" by Korarei",
};
}  // namespace

extern "C" {
DWORD
RequiredVersion() { return 2003700; }

void
InitializeLogger(LOG_HANDLE *handle) {
    logger = handle;
}

bool
InitializePlugin(DWORD version) {
    return version >= RequiredVersion();
}

void
UninitializePlugin() {
    filter::deinit();
    wic::WIC::release();
}

COMMON_PLUGIN_TABLE *
GetCommonPluginTable() {
    return &info;
};

void
RegisterPlugin(HOST_APP_TABLE *host) {
    filter::init(host, logger);
    io::init(host, logger);
}
}
