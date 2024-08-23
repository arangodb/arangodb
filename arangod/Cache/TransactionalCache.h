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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

#include "Basics/ErrorCode.h"
#include "Cache/Cache.h"
#include "Cache/CachedValue.h"
#include "Cache/Common.h"
#include "Cache/Finding.h"
#include "Cache/Manager.h"
#include "Cache/ManagerTasks.h"
#include "Cache/Metadata.h"
#include "Cache/Table.h"
#include "Cache/TransactionalBucket.h"

namespace arangodb::cache {

////////////////////////////////////////////////////////////////////////////////
/// @brief A transactional, LRU-ish cache.
///
/// To create a cache, see Manager class. Once created, the class has a simple
/// API mostly following that of the base Cache class. For any non-pure-virtual
/// functions, see Cache.h for documentation. The only additional functions
/// exposed on the API of the transactional cache are those dealing with the
/// banishing of keys.
///
/// To operate correctly, whenever a key is about to be written to the backing
/// store, it must be banished in any corresponding transactional caches.
/// This will prevent the cache from serving stale or potentially incorrect
/// values and allow for clients to fall through to the backing transactional
/// store.
////////////////////////////////////////////////////////////////////////////////

template<typename Hasher>
class TransactionalCache final : public Cache {
 public:
  TransactionalCache(Cache::ConstructionGuard guard, Manager* manager,
                     std::uint64_t id, Metadata&& metadata,
                     std::shared_ptr<Table> table, bool enableWindowedStats);
  ~TransactionalCache();

  TransactionalCache() = delete;
  TransactionalCache(TransactionalCache const&) = delete;
  TransactionalCache& operator=(TransactionalCache const&) = delete;

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
  /// Returns ok if inserted, error otherwise. Will not insert if the key is
  /// (or its corresponding hash) is banished. Will not insert value if this
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
  /// before quitting, so may block for longer than find or insert. Client may
  /// re-try.
  //////////////////////////////////////////////////////////////////////////////
  ::ErrorCode remove(void const* key, std::uint32_t keySize) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Attempts to banish the given key.
  ///
  /// Returns ok if the key was banished and is guaranteed not to be in the
  /// cache, error otherwise. May not banish the key if it fails to
  /// acquire a lock in a timely fashion. Makes more attempts to acquire a lock
  /// before quitting, so may block for longer than find or insert. Client
  /// should re-try.
  //////////////////////////////////////////////////////////////////////////////
  ::ErrorCode banish(void const* key, std::uint32_t keySize) override;

  /// @brief returns the name of the hasher
  std::string_view hasherName() const noexcept;

  static constexpr uint64_t allocationSize() {
    return sizeof(TransactionalCache);
  }

 private:
  // friend class manager and tasks
  friend class FreeMemoryTask;
  friend class Manager;
  friend class MigrateTask;

  static std::shared_ptr<Cache> create(Manager* manager, std::uint64_t id,
                                       Metadata&& metadata,
                                       std::shared_ptr<Table> table,
                                       bool enableWindowedStats);

  bool freeMemoryWhile(std::function<bool(std::uint64_t)> const& cb) override;
  void migrateBucket(Table* table, void* sourcePtr,
                     std::unique_ptr<Table::Subtable> targets,
                     Table& newTable) override;

  // helpers
  std::tuple<::ErrorCode, Table::BucketLocker> getBucket(
      Table::HashOrId bucket, std::uint64_t maxTries,
      bool singleOperation = true);

  // simplified version of getBucket(). does not report access to the
  // manager and does not update the bucket's term.
  std::tuple<::ErrorCode, Table::BucketLocker> getBucketSimple(
      Table* table, Table::HashOrId bucket, std::uint64_t maxTries);

  static Table::BucketClearer bucketClearer(Cache* cache, Metadata* metadata);
};

}  // end namespace arangodb::cache
