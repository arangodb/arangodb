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

#pragma once

#include <atomic>
#include <cstdint>
#include <limits>

namespace arangodb::transaction {

enum class CountType {
  // actual and accurate result. always returns the collection's actual count
  // value
  kNormal,
  // potentially return a cached result, if the cache value has not yet expired
  // may return an outdated value, but may save querying the collection
  kTryCache,
  // per-shard detailed results. will always query the actual counts
  kDetailed
};

/// @brief a simple cache for the "number of documents in a collection" value
/// the cache is initially populated with a count value of UINT64_MAX
/// this indicates that no count value has been queried/stored yet
struct CountCache {
  static constexpr uint64_t kNotPopulated =
      std::numeric_limits<uint64_t>::max();

  /// @brief construct a cache with the specified TTL value
  explicit CountCache(double ttl) noexcept;

#ifdef ARANGODB_USE_GOOGLE_TESTS
  virtual ~CountCache() = default;
#endif

  /// @brief get current value from cache, regardless if expired or not.
  /// will return whatever has been stored. if nothing was stored yet, will
  /// return kNotPopulated.
  uint64_t get() const noexcept;

  /// @brief get current value from cache if not yet expired.
  /// if expired or never populated, returns kNotPopulated.
  uint64_t getWithTtl() const noexcept;

  /// @brief stores value in the cache and bumps the TTL into the future
  void store(uint64_t value) noexcept;

#ifdef ARANGODB_USE_GOOGLE_TESTS
  void storeWithoutTtlBump(uint64_t value) noexcept;

  bool isExpired() const noexcept;
#endif

  /// @brief bump expiry timestamp if necessary. returns true if timestamp
  /// was changed. return false otherwise.
  /// this method is useful so that multiple concurrent threads can call it
  /// and at most one of gets the "true" value back and update the cache's
  /// value.
  bool bumpExpiry() noexcept;

 protected:
  TEST_VIRTUAL double getTime() const noexcept;

 private:
  std::atomic<uint64_t> count;
  std::atomic<double> expireStamp;
  double const ttl;
};

}  // namespace arangodb::transaction
