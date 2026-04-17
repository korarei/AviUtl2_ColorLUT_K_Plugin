#include "io.hpp"

#include "exporter.hpp"
#include "importer.hpp"

namespace lut::io {
void
init(HOST_APP_TABLE *host, LOG_HANDLE *logger) {
    exporter::init(logger);
    importer::init(logger);

    host->register_output_plugin(&exporter::info);
    host->register_file_drop_handler(importer::name, importer::filefilter, importer::on_drop);
}
}  // namespace lut::io
