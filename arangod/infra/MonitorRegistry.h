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

#include "infra/ResourceMonitorBase.h"

#include "Basics/ReadWriteSpinLock.h"
#include "Basics/SpinLocker.h"

#include <atomic>
#include <cstddef>

namespace arangodb::infra {

struct MonitorRegistry {
  MonitorRegistry() = default;

  MonitorRegistry(MonitorRegistry const&) = delete;
  MonitorRegistry& operator=(MonitorRegistry const&) = delete;

  void insert(ResourceMonitorBase& monitor) noexcept {
    auto* head = _head.load(std::memory_order_relaxed);
    do {
      monitor.setNext(head);
    } while (!_head.compare_exchange_weak(
        head, &monitor, std::memory_order_release, std::memory_order_relaxed));
  }

  template<typename Fn>
  void forEach(Fn&& fn) const {
    basics::SpinLocker lock(basics::SpinLocker::Mode::Read, _lock);
    for (auto* cur = _head.load(std::memory_order_acquire); cur != nullptr;
         cur = cur->next()) {
      fn(*cur);
    }
  }

  auto cleanup() -> std::size_t;
  auto size() const -> std::size_t;

 private:
  // Guards against cleanup only; insert() is lock-free and takes no lock. A spin
  // lock keeps the registry constant-initializable and trivially destructible,
  // which it must be to have process lifetime.
  mutable basics::ReadWriteSpinLock _lock;
  std::atomic<ResourceMonitorBase*> _head{nullptr};
};

}  // namespace arangodb::infra
