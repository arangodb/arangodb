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

#ifndef ARANGOD_TRANSACTION_COUNT_CACHE_H
#define ARANGOD_TRANSACTION_COUNT_CACHE_H 1

#include "Basics/Common.h"

#include <atomic>
#include <limits>

namespace arangodb {
namespace transaction {

enum class CountType {
  // actual and accurate result. always returns the collection's actual count
  // value
  Normal,
  // potentially return a cached result, if the cache value has not yet expired
  // may return an outdated value, but may save querying the collection
  TryCache,
  // always return cached result. will always return a stale result, but will
  // never query the collection's actual count
  ForceCache,
  // per-shard detailed results. will always query the actual counts
  Detailed
};

/// @brief a simple cache for the "number of documents in a collection" value
/// the cache is initially populated with a count value of UINT64_MAX
/// this indicates that no count value has been queried/stored yet
struct CountCache {
  static constexpr uint64_t NotPopulated = std::numeric_limits<uint64_t>::max();

  /// @brief construct a cache with the specified TTL value
  explicit CountCache(double ttl);

  /// @brief get current value from cache, regardless if expired or not.
  /// will return whatever has been stored. if nothing was stored yet, will
  /// return NotPopulated.
  uint64_t get() const;

  /// @brief get current value from cache if not yet expired
  /// if expired or never populated, returns NotPopulated
  uint64_t getWithTtl() const;

  /// @brief stores value in the cache and bumps the TTL into the future
  void store(uint64_t value);

 private:
  std::atomic<uint64_t> count;
  std::atomic<double> expireStamp;
  double const ttl;
};

}  // namespace transaction
}  // namespace arangodb

#endif
