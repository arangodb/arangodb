#pragma once

#include "infra/ResourceMonitorScope.h"
#include "infra/StaticResourceMonitor.h"

// Registry entries are server bookkeeping and outlive the query that created
// them, so they are charged to these monitors instead of the query's.
namespace arangodb::infra {

inline StaticResourceMonitor& asyncRegistryMonitor() noexcept {
  static StaticResourceMonitor monitor;
  return monitor;
}

[[nodiscard]] inline ResourceMonitorScope asyncRegistryScope() noexcept {
  return ResourceMonitorScope(ResourceMonitorRef(asyncRegistryMonitor()));
}

inline StaticResourceMonitor& activitiesMonitor() noexcept {
  static StaticResourceMonitor monitor;
  return monitor;
}

[[nodiscard]] inline ResourceMonitorScope activitiesScope() noexcept {
  return ResourceMonitorScope(ResourceMonitorRef(activitiesMonitor()));
}

}  // namespace arangodb::infra
