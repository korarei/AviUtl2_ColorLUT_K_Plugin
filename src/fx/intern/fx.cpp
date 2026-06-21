#include "../fx.hpp"

#include "grading.hpp"
#include "neutral.hpp"

namespace lut::fx {
void Init(HOST_APP_TABLE* host) {
    grading::Init(host);
    neutral::Init(host);
}

void Deinit() { grading::Deinit(); }
}  // namespace lut::fx
