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
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Basics/BoundedChannel.h"
#include "Basics/Result.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "Utils/CollectionGuard.h"
#include "Utils/CollectionNameResolver.h"

#include <rocksdb/slice.h>

namespace rocksdb {
class Iterator;
class ManagedSnapshot;
}  // namespace rocksdb

namespace arangodb {
namespace velocypack {
struct CustomTypeHandler;
}

class CollectionGuard;
class DatabaseFeature;
class DatabaseGuard;
class LogicalCollection;
class RocksDBCollection;
class RocksDBEngine;

struct RocksDBDumpContextOptions {
  uint64_t batchSize;
  uint64_t prefetchCount;
  uint64_t parallelism;
  std::vector<std::string> shards;
  double ttl;
};

class RocksDBDumpContext {
 public:
  RocksDBDumpContext(RocksDBDumpContext const&) = delete;
  RocksDBDumpContext& operator=(RocksDBDumpContext const&) = delete;

  RocksDBDumpContext(RocksDBEngine& engine, DatabaseFeature& databaseFeature,
                     std::string id, RocksDBDumpContextOptions options,
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

  struct CollectionInfo {
    // note: can throw during creation if the collection/shard cannot be found.
    CollectionInfo(TRI_vocbase_t& vocbase, std::string const& name);
    ~CollectionInfo();

    CollectionInfo(CollectionInfo const&) = delete;
    CollectionInfo& operator=(CollectionInfo const&) = delete;

    CollectionGuard guard;
    RocksDBCollection const* rcoll;
    RocksDBKeyBounds const bounds;
    rocksdb::Slice const lower;
    rocksdb::Slice const upper;
  };

  struct WorkItem {
    std::shared_ptr<CollectionInfo> collection;
    uint64_t lowerBound = 0;
    uint64_t upperBound = UINT64_MAX;

    bool empty() const noexcept {
      return collection == nullptr && lowerBound == 0 &&
             upperBound == UINT64_MAX;
    }
  };

  void handleWorkItem(WorkItem workItem);

  class WorkItems {
   public:
    explicit WorkItems(size_t numWorker);
    void push(WorkItem item);
    WorkItem pop();
    void stop();

    void setError(Result res);
    Result result() const;

   private:
    std::mutex mutable _lock;
    std::condition_variable _cv;
    std::vector<WorkItem> _work;
    bool _completed{false};
    size_t _waitingWorkers{0};
    std::size_t const _numWorkers;
    Result _result;
  };

  // Returns the next batch and assigned it batchId. If lastBatch is not nullopt
  // frees the batch with the given id. This function might block, if no batch
  // is available. It returns nullptr is there is no batch left.
  std::shared_ptr<Batch> next(std::uint64_t batchId,
                              std::optional<std::uint64_t> lastBatch);

  int64_t getBlockCounts() noexcept;

 private:
  // build a rocksdb::Iterator for a collection/shard
  std::unique_ptr<rocksdb::Iterator> buildIterator(
      CollectionInfo const& ci) const;

  RocksDBEngine& _engine;

  // these parameters will not change during the lifetime of the object.

  // context id
  std::string const _id;
  std::string const _user;
  std::string const _database;

  RocksDBDumpContextOptions const _options;

  // timestamp when this context expires and will be removed by the manager.
  // will be extended whenever the context is leased from the manager and
  // when it is returned to the manager.
  // timestamp is in seconds since 1970/1/1
  std::atomic<double> _expires;

  // a guard object that protects the underlying database from being deleted
  // while the dump is ongoing. will be populated in the constructor and then
  // be static.
  std::unique_ptr<DatabaseGuard> _databaseGuard;

  // collection access objects that project the underlying collections/shards
  // from being deleted while the dump is ongoing. will be populated in the
  // constructor and then be static.
  // will also hold additional useful information about the collection/shard.
  std::unordered_map<std::string, std::shared_ptr<CollectionInfo>> _collections;

  // resolver, used to translate numeric collection ids to string during
  // dumping
  std::unique_ptr<CollectionNameResolver> _resolver;

  // custom type handler for translating numeric collection ids in
  // velocypack "custom" types into collection name strings
  std::unique_ptr<velocypack::CustomTypeHandler> _customTypeHandler;

  // the RocksDB snapshot that can be used concurrently by all operations that
  // use this context.
  std::shared_ptr<rocksdb::ManagedSnapshot> _snapshot;

  // items of work still to be processed.
  // initially, we insert one item per shard that covers the full key range
  // for the shard. later, additional smaller work items may be pushed for
  // the shards.
  WorkItems _workItems;

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

  // Counts +1 for a block on the pop side and -1 for a block on the push side.
  std::atomic<int64_t> _blockCounter{0};
};

}  // namespace arangodb
