////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "infra/ResourceMonitor.h"

namespace arangodb::infra {

struct ResourceMonitorRef {
  ResourceMonitorRef() noexcept = default;

  explicit ResourceMonitorRef(StaticResourceMonitor& m) noexcept
      : _ptr(&m), _refCounted(false) {}

  explicit ResourceMonitorRef(ResourceMonitor& m) noexcept
      : _ptr(&m), _refCounted(true) {
    m.acquireRef();
  }

  ResourceMonitorRef(ResourceMonitorRef&& other) noexcept
      : _ptr(other._ptr), _refCounted(other._refCounted) {
    other._ptr = nullptr;
    other._refCounted = false;
  }

  ResourceMonitorRef(ResourceMonitorRef const& other) noexcept
      : _ptr(other._ptr), _refCounted(other._refCounted) {
    if (_refCounted && _ptr) {
      static_cast<ResourceMonitor*>(_ptr)->acquireRef();
    }
  }

  auto operator=(ResourceMonitorRef&& other) noexcept -> ResourceMonitorRef& {
    if (this != &other) {
      releaseIfNeeded();
      _ptr = other._ptr;
      _refCounted = other._refCounted;
      other._ptr = nullptr;
      other._refCounted = false;
    }
    return *this;
  }

  auto operator=(ResourceMonitorRef const& other) noexcept
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

  ~ResourceMonitorRef() { releaseIfNeeded(); }

  [[nodiscard]] auto get() const noexcept -> ResourceMonitorBase* {
    return _ptr;
  }

  explicit operator bool() const noexcept { return _ptr != nullptr; }

  void reset() noexcept {
    releaseIfNeeded();
    _ptr = nullptr;
    _refCounted = false;
  }

 private:
  void releaseIfNeeded() noexcept {
    if (_refCounted && _ptr) {
      static_cast<ResourceMonitor*>(_ptr)->releaseRef();
    }
  }

  ResourceMonitorBase* _ptr = nullptr;
  bool _refCounted = false;
};

}  // namespace arangodb::infra
