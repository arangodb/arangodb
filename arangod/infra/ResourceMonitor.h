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

#include "infra/StaticResourceMonitor.h"

#include "Assertions/Assert.h"

#include <atomic>
#include <cstdint>

namespace arangodb::infra {

class ResourceMonitor : public ResourceMonitorBase {
 public:
  explicit ResourceMonitor(StaticResourceMonitor* parent = nullptr)
      : _parent(parent) {}

  [[nodiscard]] bool increase(uint64_t requested, uint64_t actual) noexcept {
    if (!ResourceMonitorBase::increase(requested, actual)) {
      return false;
    }
    if (_parent != nullptr) {
      // TODO - count in chunks
      if (!_parent->increase(requested, actual)) {
        ResourceMonitorBase::decrease(requested, actual);
        return false;
      }
    }
    return true;
  }

  void decrease(uint64_t requested, uint64_t actual) noexcept {
    ResourceMonitorBase::decrease(requested, actual);
    if (_parent != nullptr) {
      // TODO - count in chunks like old monitor did
      _parent->decrease(requested, actual);
    }
  }

  void acquireRef() noexcept {
    _externalRefs.fetch_add(1, std::memory_order_relaxed);
  }

  void releaseRef() noexcept {
    _externalRefs.fetch_sub(1, std::memory_order_release);
  }

  [[nodiscard]] auto refCount() const noexcept -> uint32_t {
    return _externalRefs.load(std::memory_order_relaxed);
  }

  void retire() noexcept {
    TRI_ASSERT(_retired.load(std::memory_order_relaxed) == false);
    _retired.store(true, std::memory_order_release);
  }

  [[nodiscard]] auto isRetired() const noexcept -> bool {
    return _retired.load(std::memory_order_acquire);
  }

  [[nodiscard]] auto isReclaimable() const noexcept -> bool {
    return isRetired() && actualBytes() == 0;
  }

  [[nodiscard]] auto parent() const noexcept -> StaticResourceMonitor* {
    return _parent;
  }

 private:
  std::atomic<uint32_t> _externalRefs{0};
  std::atomic<bool> _retired{false};
  StaticResourceMonitor* const _parent;
};

}  // namespace arangodb::infra
