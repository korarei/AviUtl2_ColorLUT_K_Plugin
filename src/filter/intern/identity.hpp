#pragma once

#include <windows.h>

#include <plugin2.h>

namespace lut::filter::identity {
void Init(HOST_APP_TABLE* host);

void Deinit();
}  // namespace lut::filter::identity
