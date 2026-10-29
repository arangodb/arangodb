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

#include "MonitorRegistry.h"

#include "infra/ResourceMonitor.h"

namespace arangodb::infra {

// Skip the head node during cleanup to avoid ABA with concurrent
// lock-free inserts that CAS on _head.
auto MonitorRegistry::cleanup() -> std::size_t {
  basics::SpinLocker lock(basics::SpinLocker::Mode::Write, _lock);
  std::size_t count = 0;
  auto* prev = _head.load(std::memory_order_relaxed);
  if (prev == nullptr) {
    return 0;
  }
  auto* cur = prev->next();
  while (cur != nullptr) {
    auto* next = cur->next();
    if (!cur->isStatic()) {
      auto* mon = static_cast<ResourceMonitor*>(cur);
      if (mon->isReclaimable()) {
        prev->setNext(next);
        delete mon;
        ++count;
        cur = next;
        continue;
      }
    }
    prev = cur;
    cur = next;
  }
  return count;
}

auto MonitorRegistry::size() const -> std::size_t {
  basics::SpinLocker lock(basics::SpinLocker::Mode::Read, _lock);
  std::size_t count = 0;
  for (auto* cur = _head.load(std::memory_order_acquire); cur != nullptr;
       cur = cur->next()) {
    ++count;
  }
  return count;
}

}  // namespace arangodb::infra
