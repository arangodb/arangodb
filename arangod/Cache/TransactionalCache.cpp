////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#include "Cache/TransactionalCache.h"
#include "Basics/Common.h"
#include "Cache/Cache.h"
#include "Cache/CachedValue.h"
#include "Cache/FrequencyBuffer.h"
#include "Cache/Metadata.h"
#include "Cache/TransactionalBucket.h"

#include <stdint.h>
#include <atomic>
#include <chrono>
#include <list>

using namespace arangodb::cache;

Cache::Finding TransactionalCache::find(void const* key, uint32_t keySize) {
  // TODO: implement this;
  return Cache::Finding(nullptr);
}

bool TransactionalCache::insert(CachedValue* value) {
  // TODO: implement this
  return false;
}

bool TransactionalCache::remove(void const* key, uint32_t keySize) {
  // TODO: implement this
  return false;
}

void TransactionalCache::blackList(void const* key, uint32_t keySize) {
  // TODO: implement this
}

std::shared_ptr<Cache> TransactionalCache::create(Manager* manager,
                                                  uint64_t requestedSize,
                                                  bool allowGrowth,
                                                  bool enableWindowedStats) {
  TransactionalCache* cache = new TransactionalCache(
      manager, requestedSize, allowGrowth, enableWindowedStats);

  if (cache == nullptr) {
    return std::shared_ptr<Cache>(nullptr);
  }

  cache->metadata()->lock();
  auto result = cache->metadata()->cache();
  cache->metadata()->unlock();

  return result;
}

TransactionalCache::TransactionalCache(Manager* manager,
                                       uint64_t requestedLimit,
                                       bool allowGrowth,
                                       bool enableWindowedStats)
    : Cache(manager, requestedLimit, allowGrowth, enableWindowedStats,
            [](Cache* p) -> void {
              delete reinterpret_cast<TransactionalCache*>(p);
            },
            sizeof(TransactionalCache)) {
  // TODO: implement this
}

TransactionalCache::~TransactionalCache() {
  // TODO: implement this
}

bool TransactionalCache::freeMemory() {
  // TODO: implement this
  return false;
}

bool TransactionalCache::migrate() {
  // TODO: implement this
  return false;
}

void TransactionalCache::clearTables() {
  // TODO: implement this
}
