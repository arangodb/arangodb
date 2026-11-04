#pragma once

#include "infra/ResourceMonitorScope.h"
#include "infra/StaticResourceMonitor.h"

namespace arangodb::infra {

inline StaticResourceMonitor& rocksdbWriteMonitor() noexcept {
  static StaticResourceMonitor monitor;
  return monitor;
}

[[nodiscard]] inline ResourceMonitorScope rocksdbWriteScope() noexcept {
  return ResourceMonitorScope(ResourceMonitorRef(rocksdbWriteMonitor()));
}

}  // namespace arangodb::infra
