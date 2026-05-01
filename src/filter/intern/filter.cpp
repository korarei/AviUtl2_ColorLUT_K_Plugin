#include "filter.hpp"

#include "identity.hpp"
#include "transform.hpp"

namespace lut::filter {
void
init(HOST_APP_TABLE *host, LOG_HANDLE *logger) noexcept {
    identity::init(logger);
    transform::init(logger);

    host->register_filter_plugin(&identity::info);
    host->register_filter_plugin(&transform::info);

    host->register_clear_cache_handler(
            []([[maybe_unused]] EDIT_SECTION *edit) { transform::reset(); });
}

void
deinit() {
    transform::deinit();
    identity::deinit();
}
}  // namespace lut::filter
