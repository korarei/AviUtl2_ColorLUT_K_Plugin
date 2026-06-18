#include "../filter.hpp"

#include "grading.hpp"
#include "object.hpp"

namespace lut::filter {
void Init(HOST_APP_TABLE* host) {
    grading::Init(host);
    object::Init(host);
}

void Deinit() { grading::Deinit(); }
}  // namespace lut::filter
