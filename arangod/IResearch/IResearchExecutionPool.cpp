////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchExecutionPool.h"

#include "Assertions/Assert.h"

namespace arangodb::iresearch {

uint64_t IResearchExecutionPool::allocateThreads(uint64_t deltaActive,
                                                 uint64_t deltaDemand) {
  TRI_ASSERT(deltaActive > 0);
  TRI_ASSERT(deltaDemand <= deltaActive);
  auto curr = _active.load(std::memory_order_relaxed);
  uint64_t newval;
  do {
    newval = std::min(curr + deltaActive, _limit);
  } while (!_active.compare_exchange_weak(curr, newval));
  auto add = newval - curr;
  if (add > 0) {
    // TODO: add a single call to thread_pool to change both
    _pool.max_idle_delta(static_cast<int>(add));
    _pool.max_threads_delta(static_cast<int>(add));
  }
  fetch_add(deltaDemand);
  return add;
}

void IResearchExecutionPool::releaseThreads(uint64_t active, uint64_t demand) {
  TRI_ASSERT(active > 0 || demand > 0);
  TRI_ASSERT(_active.load() >= active);
  TRI_ASSERT(load() >= demand);

  TRI_ASSERT(active < std::numeric_limits<int>::max());
  if (active) {
    int delta = static_cast<int>(active);
    _pool.max_idle_delta(-delta);
    _pool.max_threads_delta(-delta);
    _active.fetch_sub(active);
  }
  fetch_sub(demand);
}

}  // namespace arangodb::iresearch
