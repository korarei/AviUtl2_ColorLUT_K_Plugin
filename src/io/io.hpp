#pragma once

#include <windows.h>

#include <plugin2.h>

namespace lut::io {
void Init(HOST_APP_TABLE* host);

void Deinit();
}  // namespace lut::io
