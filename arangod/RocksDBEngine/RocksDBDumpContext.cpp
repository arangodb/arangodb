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

#include "RocksDBDumpContext.h"

#include "Basics/Exceptions.h"
#include "Basics/system-functions.h"
#include "Logger/LogMacros.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "Transaction/Context.h"
#include "Utils/DatabaseGuard.h"

#include <rocksdb/db.h>
#include <rocksdb/iterator.h>
#include <rocksdb/snapshot.h>
#include <rocksdb/utilities/transaction_db.h>

#include <velocypack/Dumper.h>
#include <velocypack/Options.h>
#include <velocypack/Sink.h>
#include <velocypack/Slice.h>

using namespace arangodb;

RocksDBDumpContext::CollectionInfo::CollectionInfo(TRI_vocbase_t& vocbase,
                                                   std::string const& name)
    : guard(&vocbase, name),
      rcoll(static_cast<RocksDBCollection const*>(
          guard.collection()->getPhysical())),
      bounds(RocksDBKeyBounds::CollectionDocuments(rcoll->objectId())),
      upper(bounds.end()) {}

RocksDBDumpContext::CollectionInfo::~CollectionInfo() = default;

void RocksDBDumpContext::WorkItems::push(RocksDBDumpContext::WorkItem item) {
  std::unique_lock guard(_lock);
  _work.push_back(std::move(item));
}

RocksDBDumpContext::WorkItem RocksDBDumpContext::WorkItems::pop() {
  std::unique_lock guard(_lock);
  if (_work.empty()) {
    return WorkItem{};
  }
  WorkItem top = std::move(_work.back());
  _work.pop_back();
  return top;
}

RocksDBDumpContext::RocksDBDumpContext(
    RocksDBEngine& engine, DatabaseFeature& databaseFeature, std::string id,
    uint64_t batchSize, uint64_t prefetchCount, uint64_t parallelism,
    std::vector<std::string> const& shards, double ttl, std::string const& user,
    std::string const& database)
    : _engine(engine),
      _id(std::move(id)),
      _batchSize(batchSize),
      _prefetchCount(prefetchCount),
      _parallelism(parallelism),
      _ttl(ttl),
      _user(user),
      _database(database),
      _expires(TRI_microtime() + _ttl),
      _channel(prefetchCount) {
  // this DatabaseGuard will protect the database object from being deleted
  // while the context is in use. that way we only have to ensure once that the
  // database is there. creating this guard will throw if the database cannot be
  // found.
  _databaseGuard = std::make_unique<DatabaseGuard>(databaseFeature, database);

  TRI_vocbase_t& vocbase = _databaseGuard->database();

  // build CollectionInfo objects for each collection/shard.
  // the guard objects inside will protect the collection/shard objects from
  // being deleted while the context is in use. that we we only have to ensure
  // once that the collections are there. creating the guards will throw if any
  // of the collections/shards cannot be found.
  for (auto const& it : shards) {
    auto ci = std::make_shared<CollectionInfo>(vocbase, it);

    _collections.emplace(it, ci);

    // schedule a work item for each collection/shard, range is from min key
    // to max key initially.
    _workItems.push({std::move(ci), 0, UINT64_MAX});
  }

  _resolver = std::make_unique<CollectionNameResolver>(vocbase);

  // create a custom type handler for translating numeric collection ids in
  // velocypack "custom" types into collection name strings
  _customTypeHandler =
      transaction::Context::createCustomTypeHandler(vocbase, *_resolver);

  // acquire RocksDB snapshot
  _snapshot =
      std::make_shared<rocksdb::ManagedSnapshot>(_engine.db()->GetRootDB());
  TRI_ASSERT(_snapshot->snapshot() != nullptr);

  // start all the threads
  for (size_t i = 0; i < _parallelism; i++) {
    _threads.emplace_back(
        [&, shards, guard = BoundedChannelProducerGuard(_channel)] {
          // poor man's queue processing...
          while (true) {
            auto workItem = _workItems.pop();
            if (workItem.empty()) {
              // work item is empty. exit thread.

              // TODO: this must be fixed later if handling a work item can
              // produce additional work items. otherwise we may exit the
              // threads prematurely
              break;
            }

            handleWorkItem(workItem);
          }
        });
  }
}

// will automatically delete the RocksDB snapshot and all guards
RocksDBDumpContext::~RocksDBDumpContext() {
  _channel.stop();
  _threads.clear();
}

std::string const& RocksDBDumpContext::id() const noexcept { return _id; }

std::string const& RocksDBDumpContext::database() const noexcept {
  return _database;
}

std::string const& RocksDBDumpContext::user() const noexcept { return _user; }

double RocksDBDumpContext::ttl() const noexcept { return _ttl; }

double RocksDBDumpContext::expires() const noexcept {
  return _expires.load(std::memory_order_relaxed);
}

bool RocksDBDumpContext::canAccess(std::string const& database,
                                   std::string const& user) const noexcept {
  return database == _database && user == _user;
}

void RocksDBDumpContext::extendLifetime() noexcept {
  _expires.fetch_add(_ttl, std::memory_order_relaxed);
}

std::shared_ptr<RocksDBDumpContext::Batch> RocksDBDumpContext::next(
    std::uint64_t batchId, std::optional<std::uint64_t> lastBatch) {
  std::unique_lock guard(_mutex);
  if (lastBatch.has_value()) {
    _batches.erase(*lastBatch);
  }

  if (auto it = _batches.find(batchId); it != _batches.end()) {
    return it->second;
  }

  // get the next batch from the channel
  // TODO detect if we blocked during pop or during push
  auto [batch, blocked] = _channel.pop();
  if (blocked) {
    ++_blockCounterPop;
  }
  if (batch == nullptr) {
    // no batches left
    return nullptr;
  }

  auto [iter, inserted] =
      _batches.try_emplace(batchId, std::shared_ptr<Batch>(batch.release()));
  if (!inserted) {
    LOG_TOPIC("72486", WARN, Logger::DUMP) << "duplicate batch id " << batchId;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "duplicate batch id");
  }

  return iter->second;
}

void RocksDBDumpContext::handleWorkItem(WorkItem const& item) {
  TRI_ASSERT(!item.empty());

  CollectionInfo const& ci = *item.collection;

  // TODO: item's bounds are not honored here.
  // instead, the full collection is dumped
  LOG_TOPIC("98dfe", DEBUG, Logger::DUMP)
      << "handling dump work item for collection '"
      << ci.guard.collection()->name() << "', lower bound: " << item.lowerBound
      << ", upper bound: " << item.upperBound;

  std::unique_ptr<Batch> batch;

  velocypack::Options options;
  options.customTypeHandler = _customTypeHandler.get();

  // create a StringSink that initially points to no buffer.
  // don't use the sink until it points to an actual buffer!
  velocypack::StringSink sink(nullptr);
  velocypack::Dumper dumper(&sink, &options);

  auto it = buildIterator(ci);
  TRI_ASSERT(it != nullptr);

  uint64_t docsProduced = 0;
  uint64_t batchesProduced = 0;

  for (it->Seek(item.collection->bounds.start()); it->Valid(); it->Next()) {
    TRI_ASSERT(it->key().compare(ci.upper) < 0);

    ++docsProduced;

    if (batch == nullptr) {
      batch = std::make_unique<Batch>();
      batch->shard = ci.guard.collection()->name();
      // make sink point into the string of the current batch
      sink.setBuffer(&batch->content);
    }

    TRI_ASSERT(sink.getBuffer() != nullptr);
    dumper.dump(velocypack::Slice(
        reinterpret_cast<uint8_t const*>(it->value().data())));
    // always add a newline after each document, as we must produce
    // JSONL output format
    sink.push_back('\n');

    if (batch->content.size() >= _batchSize) {
      auto [stopped, blocked] = _channel.push(std::move(batch));
      if (blocked) {
        ++_blockCounterPush;
      }
      if (stopped) {
        LOG_TOPIC("09878", DEBUG, Logger::DUMP)
            << "worker thread exists, channel stopped";
        break;
      }
      TRI_ASSERT(batch == nullptr);
      ++batchesProduced;
    }
  }

  if (batch != nullptr) {
    // push remainder out
    TRI_ASSERT(!batch->content.empty());
    // we can ignore the last one. Going to exit anyway.
    std::ignore = _channel.push(std::move(batch));
    ++batchesProduced;
  }

  LOG_TOPIC("49016", DEBUG, Logger::DUMP)
      << "dumped collection '" << ci.guard.collection()->name()
      << "', docs produced: " << docsProduced
      << ", batched produced: " << batchesProduced;
}

std::unique_ptr<rocksdb::Iterator> RocksDBDumpContext::buildIterator(
    CollectionInfo const& ci) const {
  rocksdb::ReadOptions ro(/*cksum*/ false, /*cache*/ false);

  TRI_ASSERT(_snapshot != nullptr);
  ro.snapshot = _snapshot->snapshot();
  ro.prefix_same_as_start = true;
  ro.iterate_upper_bound = &ci.upper;

  rocksdb::ColumnFamilyHandle* cf = RocksDBColumnFamilyManager::get(
      RocksDBColumnFamilyManager::Family::Documents);
  std::unique_ptr<rocksdb::Iterator> iterator(
      _engine.db()->GetRootDB()->NewIterator(ro, cf));

  if (iterator == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "unable to create RocksDB iterator for collection");
  }

  return iterator;
}

std::pair<int64_t, int64_t> RocksDBDumpContext::getBlockCounts() noexcept {
  return std::make_pair(_blockCounterPop.exchange(0),
                        _blockCounterPush.exchange(0));
}
