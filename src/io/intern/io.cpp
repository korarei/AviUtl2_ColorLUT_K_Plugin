#include "../io.hpp"

#include "exporter.hpp"
#include "importer.hpp"

namespace lut::io {
void Init(HOST_APP_TABLE* host) {
    exporter::Init(host);
    importer::Init(host);
}
}  // namespace lut::io
