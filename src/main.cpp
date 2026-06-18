#include <windows.h>

#include <logger2.h>
#include <plugin2.h>

#include <filter/filter.hpp>
#include <intern/aviutl/aviutl.hpp>
#include <intern/lut/lut.hpp>
#include <intern/wic/wic.hpp>
#include <io/io.hpp>
#include "intern/lut/lut.hpp"

#ifndef VERSION
#define VERSION L"0.1.0"
#endif

#ifndef REQUIRES_AVIUTL2
#define REQUIRES_AVIUTL2 2000100u
#endif

namespace {
using namespace lut;

constinit COMMON_PLUGIN_TABLE info = {
    .name = L"ColorLUT_K",
    .information = L"ColorLUT_K v" VERSION L" by Korarei",
};
}  // namespace

extern "C" {
DWORD RequiredVersion() { return REQUIRES_AVIUTL2; }

void InitializeLogger(LOG_HANDLE* logger) { lut::aviutl::Logger::Init(logger); }

bool InitializePlugin(DWORD version) { return version >= RequiredVersion(); }

void UninitializePlugin() {
    lut::filter::Deinit();
    lut::LUTCache::Reset();
    lut::wic::WIC::Deinit();
}

COMMON_PLUGIN_TABLE* GetCommonPluginTable() { return &info; };

void RegisterPlugin(HOST_APP_TABLE* host) {
    lut::filter::Init(host);
    lut::io::Init(host);
}
}
