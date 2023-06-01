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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <thread>

#include "Basics/BoundedChannel.h"

namespace rocksdb {
class ManagedSnapshot;
}  // namespace rocksdb

namespace arangodb {
class CollectionGuard;
class DatabaseFeature;
class DatabaseGuard;
class LogicalCollection;
class RocksDBEngine;

class RocksDBDumpContext {
 public:
  RocksDBDumpContext(RocksDBDumpContext const&) = delete;
  RocksDBDumpContext& operator=(RocksDBDumpContext const&) = delete;

  RocksDBDumpContext(RocksDBEngine& engine, DatabaseFeature& databaseFeature,
                     std::string id, uint64_t batchSize, uint64_t prefetchCount,
                     uint64_t parallelism,
                     std::vector<std::string> const& shards, double ttl,
                     std::string const& user, std::string const& database);

  ~RocksDBDumpContext();

  // return id of the context. will not change during the lifetime of the
  // context.
  std::string const& id() const noexcept;

  // return database name used by the context. will not change during the
  // lifetime of the context.
  std::string const& database() const noexcept;
  // return name of the user that created the context. can be used for access
  // permissions checking. will not change during the lifetime of the context.
  std::string const& user() const noexcept;

  // return TTL value of this context. will not change during the lifetime of
  // the context.
  double ttl() const noexcept;

  // return expire date, as a timestamp in seconds since 1970/1/1
  double expires() const noexcept;

  // check whether the context is for database <database> and was created by
  // <user>.
  bool canAccess(std::string const& database,
                 std::string const& user) const noexcept;

  // extend the contexts lifetime, by adding TTL to the current time and storing
  // it in _expires.
  void extendLifetime() noexcept;

  // Contains the data for a batch
  struct Batch {
    std::uint64_t batchId;
    std::string content;
    std::string shard;
  };

  // Returns the next batch and assigned it batchId. If lastBatch is not nullopt
  // frees the batch with the given id. This function might block, if no batch
  // is available. It returns nullptr is there is no batch left.
  std::shared_ptr<Batch> next(std::uint64_t batchId,
                              std::optional<std::uint64_t> lastBatch);

 private:
  // get a collection/shard by name. will throw if the collection/shard was not
  // initially registered. the collection object is guaranteed to stay valid as
  // long as the context exists.
  LogicalCollection& collection(std::string const& name) const;

  RocksDBEngine& _engine;

  // these parameters will not change during the lifetime of the object.

  // context id
  std::string const _id;

  [[maybe_unused]] uint64_t const _batchSize;
  [[maybe_unused]] uint64_t const _prefetchCount;
  [[maybe_unused]] uint64_t const _parallelism;

  // time-to-live value for this context. will be extended automatically
  // whenever the context is accessed.
  double const _ttl;
  std::string const _user;
  std::string const _database;

  // timestamp when this context expires and will be removed by the manager.
  // will be extended whenever the context is leased from the manager and
  // when it is returned to the manager.
  // timestamp is in seconds since 1970/1/1
  std::atomic<double> _expires;

  // a guard object that protects the underlying database from being deleted
  // while the dump is ongoing. will be populated in the constructor and then
  // be static.
  std::unique_ptr<DatabaseGuard> _databaseGuard;

  // guard objects that project the underlying collections/shards from being
  // deleted while the dump is ongoing. will be populated in the constructor and
  // then be static
  std::unordered_map<std::string, std::unique_ptr<CollectionGuard>>
      _collectionGuards;

  // the RocksDB snapshot that can be used concurrently by all operations that
  // use this context.
  std::shared_ptr<rocksdb::ManagedSnapshot> _snapshot;

  std::mutex _mutex;

  // this contains all the alive batches. We have to keep batches until they
  // are explicitly released.
  std::unordered_map<std::uint64_t, std::shared_ptr<Batch>> _batches;

  // this channel is used to exchange batches between the worker threads
  // and the actual rest handler.
  BoundedChannel<Batch> _channel;

  // This is a temporary solution for the thread pool. Later we want to use
  // the scheduler to better control the resource usage.
  std::vector<std::jthread> _threads;
};

}  // namespace arangodb
