#include "infra/ResourceMonitorRef.h"
#include "infra/ResourceMonitor.h"

namespace arangodb::infra {

ResourceMonitorRef::ResourceMonitorRef(StaticResourceMonitor& m) noexcept
    : _ptr(&m), _refCounted(false) {}

ResourceMonitorRef::ResourceMonitorRef(ResourceMonitor& m) noexcept
    : _ptr(&m), _refCounted(true) {
  m.acquireRef();
}

ResourceMonitorRef::ResourceMonitorRef(ResourceMonitorRef const& other) noexcept
    : _ptr(other._ptr), _refCounted(other._refCounted) {
  if (_refCounted && _ptr) {
    static_cast<ResourceMonitor*>(_ptr)->acquireRef();
  }
}

auto ResourceMonitorRef::operator=(ResourceMonitorRef const& other) noexcept
    -> ResourceMonitorRef& {
  if (this != &other) {
    releaseIfNeeded();
    _ptr = other._ptr;
    _refCounted = other._refCounted;
    if (_refCounted && _ptr) {
      static_cast<ResourceMonitor*>(_ptr)->acquireRef();
    }
  }
  return *this;
}

void ResourceMonitorRef::releaseIfNeeded() noexcept {
  if (_refCounted && _ptr) {
    static_cast<ResourceMonitor*>(_ptr)->releaseRef();
  }
}

}  // namespace arangodb::infra