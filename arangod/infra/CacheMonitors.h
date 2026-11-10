#pragma once

#include "infra/ResourceMonitorScope.h"
#include "infra/StaticResourceMonitor.h"

namespace arangodb::infra {
inline StaticResourceMonitor& cacheMonitor() noexcept {
  static StaticResourceMonitor monitor;
  return monitor;
}

[[nodiscard]] inline ResourceMonitorScope cacheScope() noexcept {
  return ResourceMonitorScope(ResourceMonitorRef(cacheMonitor()));
}

}  // namespace arangodb::infra
