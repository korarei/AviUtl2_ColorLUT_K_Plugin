#include "../filter.hpp"

#include "grading.hpp"
#include "identity.hpp"

namespace lut::filter {
void Init(HOST_APP_TABLE* host) {
    grading::Init(host);
    identity::Init(host);
}

void Deinit() {
    grading::Deinit();
    identity::Deinit();
}
}  // namespace lut::filter
