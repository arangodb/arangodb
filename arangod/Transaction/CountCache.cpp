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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "CountCache.h"

#include "Basics/debugging.h"
#include "Basics/system-functions.h"

using namespace arangodb;
using namespace arangodb::transaction;

CountCache::CountCache(double ttl) noexcept
    : count(CountCache::kNotPopulated), expireStamp(0.0), ttl(ttl) {}

uint64_t CountCache::get() const noexcept {
  return count.load(std::memory_order_relaxed);
}

double CountCache::getTime() const noexcept { return TRI_microtime(); }

uint64_t CountCache::getWithTtl() const noexcept {
  // (1) - this acquire-load synchronizes with the release-store (2)
  double ts = expireStamp.load(std::memory_order_acquire);
  if (ts >= getTime()) {
    // not yet expired
    return get();
  }
  return CountCache::kNotPopulated;
}

bool CountCache::bumpExpiry() noexcept {
  double now = getTime();
  double ts = expireStamp.load(std::memory_order_acquire);
  if (ts < now) {
    // expired
    double newTs = now + ttl;
    if (expireStamp.compare_exchange_strong(ts, newTs,
                                            std::memory_order_release)) {
      // we were able to update the expiry time ourselves
      return true;
    }
    // fallthrough to returning false
  }
  return false;
}

void CountCache::store(uint64_t value) noexcept {
  TRI_ASSERT(value != CountCache::kNotPopulated);
  count.store(value, std::memory_order_relaxed);
  // (2) - this release-store synchronizes with the acquire-load (1)
  expireStamp.store(getTime() + ttl, std::memory_order_release);
}

#ifdef ARANGODB_USE_GOOGLE_TESTS
void CountCache::storeWithoutTtlBump(uint64_t value) noexcept {
  count.store(value, std::memory_order_relaxed);
}

bool CountCache::isExpired() const noexcept {
  return expireStamp.load(std::memory_order_acquire) < getTime();
}

#endif
