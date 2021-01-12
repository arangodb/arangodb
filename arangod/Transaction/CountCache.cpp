////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "CountCache.h"

#include "Basics/debugging.h"
#include "Basics/system-functions.h"

using namespace arangodb;
using namespace arangodb::transaction;

CountCache::CountCache(double ttl) 
    : count(CountCache::NotPopulated), 
      expireStamp(0.0),
      ttl(ttl) {}

uint64_t CountCache::get() const {
  return count.load(std::memory_order_relaxed);
}

uint64_t CountCache::getWithTtl() const {
  // (1) - this acquire-load synchronizes with the release-store (2)
  double ts = expireStamp.load(std::memory_order_acquire);
  if (ts < TRI_microtime()) {
    // not yet expired
    return get();
  }
  return CountCache::NotPopulated;
}

void CountCache::store(uint64_t value) {
  TRI_ASSERT(value != CountCache::NotPopulated);
  count.store(value, std::memory_order_relaxed);
  // (2) - this release-store synchronizes with the acquire-load (1)
  expireStamp.store(TRI_microtime() + ttl, std::memory_order_release);
}
