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

uint64_t IResearchExecutionPool::allocateThreads(uint64_t n) {
  TRI_ASSERT(n > 0);
  auto curr = load(std::memory_order_relaxed);
  uint64_t newval;
  do {
    newval = std::min(curr + n, _limit);
  } while (!compare_exchange_weak(curr, newval));
  auto add = newval - curr;
  if (add > 0) {
    // TODO: add a single call to thread_pool to change both
    _pool.max_idle_delta(static_cast<int>(add));
    _pool.max_threads_delta(static_cast<int>(add));
  }
  return add;
}

void IResearchExecutionPool::releaseThreads(uint64_t n) {
  TRI_ASSERT(n > 0);
  TRI_ASSERT(load() >= n);
  TRI_ASSERT(n < std::numeric_limits<int>::max());
  int delta = static_cast<int>(n);
  _pool.max_idle_delta(-delta);
  _pool.max_threads_delta(-delta);
  fetch_sub(n);
}

}  // namespace arangodb::iresearch
