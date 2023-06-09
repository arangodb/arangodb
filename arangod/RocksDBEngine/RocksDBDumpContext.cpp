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

#include <utility>

using namespace arangodb;

RocksDBDumpContext::CollectionInfo::CollectionInfo(TRI_vocbase_t& vocbase,
                                                   std::string const& name)
    : guard(&vocbase, name),
      rcoll(static_cast<RocksDBCollection const*>(
          guard.collection()->getPhysical())),
      bounds(RocksDBKeyBounds::CollectionDocuments(rcoll->objectId())),
      lower(bounds.start()),
      upper(bounds.end()) {}

RocksDBDumpContext::CollectionInfo::~CollectionInfo() = default;

void RocksDBDumpContext::WorkItems::push(RocksDBDumpContext::WorkItem item) {
  std::unique_lock guard(_lock);
  _work.push_back(std::move(item));
  if (_waitingWorkers > 0) {
    _cv.notify_one();
  }
}

RocksDBDumpContext::WorkItem RocksDBDumpContext::WorkItems::pop() {
  std::unique_lock guard(_lock);
  while (!_completed) {
    if (!_work.empty()) {
      WorkItem top = std::move(_work.back());
      _work.pop_back();
      return top;
    }

    ++_waitingWorkers;
    if (_waitingWorkers == _numWorkers) {
      // everything is done
      _completed = true;
      _cv.notify_all();
      break;
    } else {
      _cv.wait(guard);
      --_waitingWorkers;
    }
  }

  return {};
}

void RocksDBDumpContext::WorkItems::setError(Result res) {
  TRI_ASSERT(res.fail());

  std::unique_lock guard(_lock);
  if (_result.ok()) {
    // only track first error, but don't clobber an existing error
    _result = std::move(res);
  }
  _completed = true;
  _cv.notify_all();
}

Result RocksDBDumpContext::WorkItems::result() const {
  std::unique_lock guard(_lock);
  return _result;
}

RocksDBDumpContext::WorkItems::WorkItems(size_t worker) : _numWorkers(worker) {}

void RocksDBDumpContext::WorkItems::stop() {
  std::unique_lock guard(_lock);
  _completed = true;
  _cv.notify_all();
}

RocksDBDumpContext::RocksDBDumpContext(RocksDBEngine& engine,
                                       DatabaseFeature& databaseFeature,
                                       std::string id,
                                       RocksDBDumpContextOptions options,
                                       std::string user, std::string database)
    : _engine(engine),
      _id(std::move(id)),
      _user(std::move(user)),
      _database(std::move(database)),
      _options(std::move(options)),
      _expires(TRI_microtime() + _options.ttl),
      _workItems(_options.parallelism),
      _channel(_options.prefetchCount) {
  // this DatabaseGuard will protect the database object from being deleted
  // while the context is in use. that way we only have to ensure once that the
  // database is there. creating this guard will throw if the database cannot be
  // found.
  _databaseGuard = std::make_unique<DatabaseGuard>(databaseFeature, _database);

  TRI_vocbase_t& vocbase = _databaseGuard->database();

  // acquire RocksDB snapshot
  _snapshot =
      std::make_shared<rocksdb::ManagedSnapshot>(_engine.db()->GetRootDB());
  TRI_ASSERT(_snapshot->snapshot() != nullptr);

  // build CollectionInfo objects for each collection/shard.
  // the guard objects inside will protect the collection/shard objects from
  // being deleted while the context is in use. that we we only have to ensure
  // once that the collections are there. creating the guards will throw if any
  // of the collections/shards cannot be found.
  for (auto const& it : _options.shards) {
    auto ci = std::make_shared<CollectionInfo>(vocbase, it);

    _collections.emplace(it, ci);

    // full key range for LocalDocumentId values
    uint64_t min = 0;
    uint64_t max = UINT64_MAX;
    {
      // determine actual key range
      auto rocksIt = buildIterator(*ci);
      // check effective lower bound key.
      rocksIt->Seek(ci->lower);
      if (rocksIt->Valid()) {
        min = RocksDBKey::documentId(rocksIt->key()).id();

        // check effective upper bound key.
        rocksIt->SeekForPrev(ci->upper);
        if (rocksIt->Valid() && rocksIt->key().compare(ci->lower) >= 0) {
          // only push a work item if the collection/shard actually contains
          // documents. no need to push a work item if there is no data
          TRI_ASSERT(rocksIt->key().compare(ci->upper) < 0);
          max = RocksDBKey::documentId(rocksIt->key()).id() + 1;

          TRI_ASSERT(min < max);
          _workItems.push({std::move(ci), min, max});
        }
      }
    }
  }

  _resolver = std::make_unique<CollectionNameResolver>(vocbase);

  // create a custom type handler for translating numeric collection ids in
  // velocypack "custom" types into collection name strings
  _customTypeHandler =
      transaction::Context::createCustomTypeHandler(vocbase, *_resolver);

  // start all the threads
  for (size_t i = 0; i < _options.parallelism; i++) {
    _threads.emplace_back([&, guard = BoundedChannelProducerGuard(_channel)] {
      try {
        while (true) {
          // will block until all workers wait, i.e. no work is left
          auto workItem = _workItems.pop();
          if (workItem.empty()) {
            break;
          }

          handleWorkItem(std::move(workItem));
        }
      } catch (basics::Exception const& ex) {
        _workItems.setError(Result(ex.code(), ex.what()));
      } catch (std::exception const& ex) {
        // must not let exceptions escape from the thread's lambda
        _workItems.setError(Result(TRI_ERROR_INTERNAL, ex.what()));
      }
    });
  }
}

// will automatically delete the RocksDB snapshot and all guards
RocksDBDumpContext::~RocksDBDumpContext() {
  _workItems.stop();
  _channel.stop();

  // join all worker threads
  for (auto& thrd : _threads) {
    thrd.join();
  }
  _threads.clear();
}

std::string const& RocksDBDumpContext::id() const noexcept { return _id; }

std::string const& RocksDBDumpContext::database() const noexcept {
  return _database;
}

std::string const& RocksDBDumpContext::user() const noexcept { return _user; }

double RocksDBDumpContext::ttl() const noexcept { return _options.ttl; }

double RocksDBDumpContext::expires() const noexcept {
  return _expires.load(std::memory_order_relaxed);
}

bool RocksDBDumpContext::canAccess(std::string const& database,
                                   std::string const& user) const noexcept {
  return database == _database && user == _user;
}

void RocksDBDumpContext::extendLifetime() noexcept {
  _expires.fetch_add(_options.ttl, std::memory_order_relaxed);
}

std::shared_ptr<RocksDBDumpContext::Batch const> RocksDBDumpContext::next(
    std::uint64_t batchId, std::optional<std::uint64_t> lastBatch) {
  std::unique_lock guard(_batchesMutex);
  if (lastBatch.has_value()) {
    _batches.erase(*lastBatch);
  }

  // check if an error occurred in any of the threads.
  Result res = _workItems.result();
  if (res.fail()) {
    // if yes, simply return this error from now on.
    // this will lead to the clients aborting.
    THROW_ARANGO_EXCEPTION(res);
  }

  if (auto it = _batches.find(batchId); it != _batches.end()) {
    return it->second;
  }

  // get the next batch from the channel
  auto [batch, blocked] = _channel.pop();
  if (blocked) {
    _blockCounter.fetch_add(1);
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

void RocksDBDumpContext::handleWorkItem(WorkItem item) {
  TRI_ASSERT(!item.empty());
  TRI_ASSERT(item.lowerBound < item.upperBound);

  CollectionInfo const& ci = *item.collection;

  LOG_TOPIC("98dfe", DEBUG, Logger::DUMP)
      << "handling dump work item for collection '"
      << ci.guard.collection()->name() << "', lower bound: " << item.lowerBound
      << ", upper bound: " << item.upperBound;

  RocksDBKey lowerBound;
  lowerBound.constructDocument(ci.rcoll->objectId(),
                               LocalDocumentId{item.lowerBound});
  RocksDBKey upperBound;
  upperBound.constructDocument(ci.rcoll->objectId(),
                               LocalDocumentId{item.upperBound});

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

  for (it->Seek(lowerBound.string()); it->Valid(); it->Next()) {
    TRI_ASSERT(it->key().compare(ci.upper) < 0);

    // check if we have reached our current end position
    if (it->key().compare(upperBound.string()) >= 0) {
      break;
    }

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

    if (batch->content.size() >= _options.batchSize) {
      auto [stopped, blocked] = _channel.push(std::move(batch));
      if (blocked) {
        _blockCounter.fetch_sub(1);
      }
      if (stopped) {
        LOG_TOPIC("09878", DEBUG, Logger::DUMP)
            << "worker thread exits, channel stopped";
        break;
      }
      TRI_ASSERT(batch == nullptr);
      ++batchesProduced;

      // we have produced a batch, cut the interval in half
      auto current = RocksDBKey::documentId(it->key()).id();
      TRI_ASSERT(current < item.upperBound);
      if (item.upperBound - current > 5000) {
        // split in half
        auto mid = current / 2 + item.upperBound / 2;
        TRI_ASSERT(mid > current);
        // create a work item starting from mid
        TRI_ASSERT(mid < item.upperBound);
        WorkItem newItem{item.collection, mid, item.upperBound};
        _workItems.push(std::move(newItem));
        // make mid the new upper bound
        upperBound.constructDocument(ci.rcoll->objectId(),
                                     LocalDocumentId{mid});
        item.upperBound = mid;
      }
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

int64_t RocksDBDumpContext::getBlockCounts() noexcept {
  return _blockCounter.exchange(0);
}
