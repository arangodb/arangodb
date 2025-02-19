////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
///
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchExecutionPool.h"

#include "Assertions/Assert.h"

namespace arangodb::iresearch {

uint64_t IResearchExecutionPool::allocateThreads(uint64_t active,
                                                 uint64_t demand) {
  TRI_ASSERT(0 < active);
  TRI_ASSERT(demand <= active);
  auto curr = _active.load(std::memory_order_relaxed);
  uint64_t newval;
  do {
    newval = std::min(curr + active, _limit);
  } while (!_active.compare_exchange_weak(curr, newval));
  fetch_add(demand);
  return newval - curr;
}

void IResearchExecutionPool::releaseThreads(uint64_t active, uint64_t demand) {
  TRI_ASSERT(active > 0 || demand > 0);
  TRI_ASSERT(_active.load() >= active);
  TRI_ASSERT(load() >= demand);
  if (active) {
    _active.fetch_sub(active);
  }
  fetch_sub(demand);
}

}  // namespace arangodb::iresearch
