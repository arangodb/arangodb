////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

using namespace arangodb;
using namespace arangodb::transaction;

CountCache::CountCache() 
   : count(CountCache::NotPopulated), 
     timestamp(0.0) {}

int64_t CountCache::get() const {
  return count.load(std::memory_order_acquire);
}

int64_t CountCache::get(double ttl) const {
  int64_t count = get();
  if (count != CountCache::NotPopulated) {
    double ts = timestamp.load(std::memory_order_relaxed);
    if (ts + Ttl > TRI_microtime()) {
      // not yet expired
      return count;
    }
  }
  return CountCache::NotPopulated;
}

void CountCache::store(int64_t value) {
  TRI_ASSERT(value >= 0);
  timestamp.store(TRI_microtime(), std::memory_order_relaxed);
  count.store(value, std::memory_order_release);
}
