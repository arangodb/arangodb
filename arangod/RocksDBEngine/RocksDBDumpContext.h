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
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Basics/BoundedChannel.h"
#include "Basics/Result.h"
#include "Inspection/Types.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "Utils/CollectionGuard.h"
#include "Utils/CollectionNameResolver.h"

#include <rocksdb/slice.h>
#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Sink.h>
#include <velocypack/Slice.h>

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
class RocksDBDumpManager;
class RocksDBEngine;

struct RocksDBDumpSimpleFilter {
  std::vector<std::string> path;
  velocypack::SharedSlice value;

  template<class Inspector>
  inline friend auto inspect(Inspector& f, RocksDBDumpSimpleFilter& o) {
    return f.object(o).fields(f.field("attributePath", o.path),
                              f.field("value", o.value));
  }
};

struct RocksDBDumpFilterSpec {
  std::string type;
  std::vector<RocksDBDumpSimpleFilter> conditions;

  template<class Inspector>
  inline friend auto inspect(Inspector& f, RocksDBDumpFilterSpec& o) {
    return f.object(o).fields(
        f.field("type", o.type).invariant([](auto&& v) {
          return v == "simple";
        }),
        f.field("conditions", o.conditions)
            .fallback(std::vector<RocksDBDumpSimpleFilter>{}));
  }
};

struct RocksDBDumpContextOptions {
  std::uint64_t docsPerBatch = 10 * 1000;
  std::uint64_t batchSize = 16 * 1024;
  std::uint64_t prefetchCount = 2;
  std::uint64_t parallelism = 2;
  double ttl = 600.0;
  std::vector<std::string> shards;

  std::optional<std::unordered_map<std::string, std::vector<std::string>>>
      projections;
  RocksDBDumpFilterSpec filters;

  template<class Inspector>
  inline friend auto inspect(Inspector& f, RocksDBDumpContextOptions& o) {
    return f.object(o).fields(
        f.field("docsPerBatch", o.docsPerBatch).fallback(f.keep()),
        f.field("batchSize", o.batchSize).fallback(f.keep()),
        f.field("prefetchCount", o.prefetchCount).fallback(f.keep()),
        f.field("parallelism", o.parallelism).fallback(f.keep()),
        f.field("ttl", o.ttl).fallback(f.keep()),
        f.field("shards", o.shards).fallback(f.keep()),
        f.field("projections", o.projections),
        f.field("filters", o.filters).fallback(RocksDBDumpFilterSpec{}));
  }
};

class RocksDBDumpContext {
 public:
  RocksDBDumpContext(RocksDBDumpContext const&) = delete;
  RocksDBDumpContext& operator=(RocksDBDumpContext const&) = delete;

  RocksDBDumpContext(RocksDBEngine& engine, RocksDBDumpManager& manager,
                     DatabaseFeature& databaseFeature, std::string id,
                     RocksDBDumpContextOptions options, std::string user,
                     std::string database, bool useVPack);

  ~RocksDBDumpContext();

  // this will make the dump context stop all its threads
  void stop() noexcept;

  bool stopped() const noexcept;

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

  // determine whether the given document should be included in the
  // dump
  bool applyFilter(velocypack::Slice const& documentSlice) const;

  // Contains the data for a batch
  struct Batch {
    Batch(Batch const&) = delete;
    Batch& operator=(Batch const&) = delete;

    Batch(RocksDBDumpManager& manager, std::uint64_t batchSize,
          std::string_view shard)
        : manager(manager), shard(shard), memoryUsage(batchSize), numDocs(0) {}
    virtual ~Batch();

    virtual void add(velocypack::Slice) = 0;
    virtual void close() = 0;
    virtual std::string_view content() const = 0;
    virtual size_t byteSize() const = 0;

    size_t count() const noexcept { return numDocs; }

    RocksDBDumpManager& manager;
    std::string_view shard;
    std::uint64_t memoryUsage;
    std::size_t numDocs;
  };

  class BatchVPackArray : public Batch {
   public:
    explicit BatchVPackArray(RocksDBDumpManager& manager,
                             std::uint64_t batchSize, std::string_view shard);
    ~BatchVPackArray();

    void add(velocypack::Slice data) override;
    void close() override;
    std::string_view content() const override;
    size_t byteSize() const override;

   private:
    velocypack::Builder _builder;
  };

  class BatchJSONL : public Batch {
   public:
    explicit BatchJSONL(RocksDBDumpManager& manager, std::uint64_t batchSize,
                        std::string_view shard,
                        velocypack::Options const* options);
    ~BatchJSONL();

    void add(velocypack::Slice data) override;
    void close() override;
    std::string_view content() const override;
    size_t byteSize() const override;

   private:
    std::string _content;
    velocypack::StringSink _sink;
    velocypack::Dumper _dumper;
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
    std::uint64_t lowerBound = 0;
    std::uint64_t upperBound = UINT64_MAX;

    bool empty() const noexcept {
      return collection == nullptr && lowerBound == 0 &&
             upperBound == UINT64_MAX;
    }
  };

  void handleWorkItem(WorkItem workItem);

  class WorkItems {
   public:
    explicit WorkItems(size_t numWorkers);
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

  // Returns the next batch and assigns its batchId. If lastBatch is not nullopt
  // frees the batch with the given id. This function might block, if no batch
  // is available. It returns nullptr if there is no batch left.
  std::shared_ptr<Batch const> next(std::uint64_t batchId,
                                    std::optional<std::uint64_t> lastBatch);

  int64_t getBlockCounts() noexcept;

 private:
  // build a rocksdb::Iterator for a collection/shard
  std::unique_ptr<rocksdb::Iterator> buildIterator(
      CollectionInfo const& ci) const;

  RocksDBEngine& _engine;

  RocksDBDumpManager& _manager;

  // these parameters will not change during the lifetime of the object.

  // context id
  std::string const _id;
  std::string const _user;
  std::string const _database;
  bool const _useVPack;

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

  // collection access objects that protect the underlying collections/shards
  // from being deleted while the dump is ongoing. will be populated in the
  // constructor and then be static.
  // will also hold additional useful information about the collection/shard.
  std::unordered_map<std::string, std::shared_ptr<CollectionInfo>> _collections;

  // resolver, used to translate numeric collection ids to string during
  // dumping
  std::unique_ptr<CollectionNameResolver const> _resolver;

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

  // Used to serializes access to _batches.
  std::mutex _batchesMutex;

  // this contains all the alive batches. We have to keep batches until they
  // are explicitly released.
  std::unordered_map<std::uint64_t, std::shared_ptr<Batch>> _batches;

  // this channel is used to exchange batches between the worker threads
  // and the actual rest handler.
  BoundedChannel<Batch> _channel;

  // Thread pool for dumping. Having our own threads is much easier: we can let
  // them block.
  std::vector<std::thread> _threads;

  // Counts +1 for a block on the pop side and -1 for a block on the push side.
  std::atomic<int64_t> _blockCounter{0};

  std::atomic_bool _stopped{false};
};

}  // namespace arangodb
