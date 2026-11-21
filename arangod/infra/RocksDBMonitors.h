#pragma once

#include "infra/ResourceMonitorScope.h"
#include "infra/StaticResourceMonitor.h"

namespace arangodb::infra {

inline StaticResourceMonitor& rocksdbMonitor() noexcept {
  static StaticResourceMonitor monitor;
  return monitor;
}

[[nodiscard]] inline ResourceMonitorScope rocksdbScope() noexcept {
  return ResourceMonitorScope(ResourceMonitorRef(rocksdbMonitor()));
}

}  // namespace arangodb::infra
