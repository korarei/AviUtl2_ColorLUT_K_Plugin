#include "../filter.hpp"

#include "grading.hpp"
#include "neutral.hpp"

namespace lut::filter {
void Init(HOST_APP_TABLE* host) {
    grading::Init(host);
    neutral::Init(host);
}

void Deinit() { grading::Deinit(); }
}  // namespace lut::filter
