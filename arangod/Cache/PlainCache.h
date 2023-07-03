////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

#include "Basics/ErrorCode.h"
#include "Cache/Cache.h"
#include "Cache/CachedValue.h"
#include "Cache/Common.h"
#include "Cache/Finding.h"
#include "Cache/FrequencyBuffer.h"
#include "Cache/Manager.h"
#include "Cache/ManagerTasks.h"
#include "Cache/Metadata.h"
#include "Cache/PlainBucket.h"
#include "Cache/Table.h"

namespace arangodb::cache {

////////////////////////////////////////////////////////////////////////////////
/// @brief A simple, LRU-ish cache.
///
/// To create a cache, see Manager class. Once created, the class has a very
/// simple API following that of the base Cache class. For any non-pure-virtual
/// functions, see Cache.h for documentation.
////////////////////////////////////////////////////////////////////////////////

template<typename Hasher>
class PlainCache final : public Cache {
 public:
  PlainCache(Cache::ConstructionGuard guard, Manager* manager, std::uint64_t id,
             Metadata&& metadata, std::shared_ptr<Table> table,
             bool enableWindowedStats);
  ~PlainCache();

  PlainCache() = delete;
  PlainCache(PlainCache const&) = delete;
  PlainCache& operator=(PlainCache const&) = delete;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Looks up the given key.
  ///
  /// May report a false negative if it fails to acquire a lock in a timely
  /// fashion. The Result contained in the return value should report an error
  /// code in this case. Should not block for long.
  //////////////////////////////////////////////////////////////////////////////
  Finding find(void const* key, std::uint32_t keySize) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Attempts to insert the given value.
  ///
  /// Returns ok if inserted, error otherwise. Will not insert value if this
  /// would cause the total usage to exceed the limits. May also not insert
  /// value if it fails to acquire a lock in a timely fashion. Should not block
  /// for long.
  //////////////////////////////////////////////////////////////////////////////
  ::ErrorCode insert(CachedValue* value) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Attempts to remove the given key.
  ///
  /// Returns ok if the key guaranteed not to be in the cache, error if the
  /// key may remain in the cache. May leave the key in the cache if it fails to
  /// acquire a lock in a timely fashion. Makes more attempts to acquire a lock
  /// before quitting, so may block for longer than find or insert.
  //////////////////////////////////////////////////////////////////////////////
  ::ErrorCode remove(void const* key, std::uint32_t keySize) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Does nothing; convenience method inheritance compliance
  //////////////////////////////////////////////////////////////////////////////
  ::ErrorCode banish(void const* key, std::uint32_t keySize) override;

  /// @brief returns the name of the hasher
  std::string_view hasherName() const noexcept;

 private:
  // friend class manager and tasks
  friend class FreeMemoryTask;
  friend class Manager;
  friend class MigrateTask;

  static constexpr uint64_t allocationSize(bool enableWindowedStats) {
    return sizeof(PlainCache) +
           (enableWindowedStats
                ? (sizeof(StatBuffer) +
                   StatBuffer::allocationSize(findStatsCapacity))
                : 0);
  }

  static std::shared_ptr<Cache> create(Manager* manager, std::uint64_t id,
                                       Metadata&& metadata,
                                       std::shared_ptr<Table> table,
                                       bool enableWindowedStats);

  bool freeMemoryWhile(std::function<bool(std::uint64_t)> const& cb) override;
  virtual void migrateBucket(void* sourcePtr,
                             std::unique_ptr<Table::Subtable> targets,
                             Table& newTable) override;

  // helpers
  std::pair<::ErrorCode, Table::BucketLocker> getBucket(
      Table::HashOrId bucket, std::uint64_t maxTries,
      bool singleOperation = true);

  static Table::BucketClearer bucketClearer(Metadata* metadata);
};

}  // end namespace arangodb::cache
