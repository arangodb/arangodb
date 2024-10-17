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

#include "RocksDBDumpContext.h"

#include "Basics/Exceptions.h"
#include "Basics/system-functions.h"
#include "Logger/LogMacros.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBDumpManager.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "Transaction/Context.h"
#include "Utils/DatabaseGuard.h"

#include <rocksdb/db.h>
#include <rocksdb/iterator.h>
#include <rocksdb/snapshot.h>
#include <rocksdb/utilities/transaction_db.h>

#include <velocypack/Options.h>
#include <velocypack/Slice.h>

#include <algorithm>
#include <utility>

using namespace arangodb;

RocksDBDumpContext::Batch::~Batch() { manager.untrackMemoryUsage(memoryUsage); }

RocksDBDumpContext::BatchVPackArray::BatchVPackArray(
    RocksDBDumpManager& manager, std::uint64_t batchSize,
    std::string_view shard)
    : Batch(manager, batchSize, shard) {
  _builder.openArray(/*unindexed*/ true);
}

RocksDBDumpContext::BatchVPackArray::~BatchVPackArray() = default;

void RocksDBDumpContext::BatchVPackArray::add(velocypack::Slice data) {
  _builder.add(data);
  ++numDocs;
}

void RocksDBDumpContext::BatchVPackArray::close() {
  _builder.close();

  // now we calculate the actual memory usage of the batch
  auto current = _builder.bufferRef().capacity();
  if (current > memoryUsage) {
    manager.trackMemoryUsage(current - memoryUsage);
  } else if (current < memoryUsage) {
    manager.untrackMemoryUsage(memoryUsage - current);
  }
  memoryUsage = current;
}

std::string_view RocksDBDumpContext::BatchVPackArray::content() const {
  return {reinterpret_cast<char const*>(_builder.bufferRef().data()),
          _builder.bufferRef().size()};
}

size_t RocksDBDumpContext::BatchVPackArray::byteSize() const {
  return _builder.bufferRef().size();
}

RocksDBDumpContext::BatchJSONL::BatchJSONL(RocksDBDumpManager& manager,
                                           std::uint64_t batchSize,
                                           std::string_view shard,
                                           velocypack::Options const* options)
    : Batch(manager, batchSize, shard),
      _sink(&_content),
      _dumper(&_sink, options) {
  // make sink point into the string of the current batch

  // save at least a few small (re)allocations
  constexpr size_t smallAllocation = 8 * 1024;
  _content.reserve(smallAllocation);
}

RocksDBDumpContext::BatchJSONL::~BatchJSONL() = default;

void RocksDBDumpContext::BatchJSONL::add(velocypack::Slice data) {
  _dumper.dump(data);
  // always add a newline after each document, as we must produce
  // JSONL output format
  _content.push_back('\n');
  ++numDocs;
}

void RocksDBDumpContext::BatchJSONL::close() {
  // now we calculate the actual memory usage of the batch
  auto current = _content.capacity();
  if (current > memoryUsage) {
    manager.trackMemoryUsage(current - memoryUsage);
  } else if (current < memoryUsage) {
    manager.untrackMemoryUsage(memoryUsage - current);
  }
  memoryUsage = current;
}

std::string_view RocksDBDumpContext::BatchJSONL::content() const {
  return _content;
}

size_t RocksDBDumpContext::BatchJSONL::byteSize() const {
  return _content.size();
}

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
                                       RocksDBDumpManager& manager,
                                       DatabaseFeature& databaseFeature,
                                       std::string id,
                                       RocksDBDumpContextOptions options,
                                       std::string user, std::string database,
                                       bool useVPack)
    : _engine(engine),
      _manager(manager),
      _id(std::move(id)),
      _user(std::move(user)),
      _database(std::move(database)),
      _useVPack(useVPack),
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
    std::uint64_t min = 0;
    std::uint64_t max = UINT64_MAX;
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
    _threads.emplace_back(
        [this, guard = BoundedChannelProducerGuard(_channel)] {
          try {
            while (!stopped()) {
              // will block until all workers wait, i.e. no work is left
              auto workItem = _workItems.pop();
              if (workItem.empty()) {
                break;
              }

              do {
                try {
                  handleWorkItem(workItem);
                  break;
                } catch (basics::Exception const& ex) {
                  if (ex.code() != TRI_ERROR_RESOURCE_LIMIT) {
                    throw;
                  }
                }
              } while (!stopped());
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
  stop();

  _workItems.stop();
  _channel.stop();

  // join all worker threads
  for (auto& thrd : _threads) {
    thrd.join();
  }
  _threads.clear();
}

void RocksDBDumpContext::stop() noexcept {
  _stopped.store(true, std::memory_order_relaxed);
}

bool RocksDBDumpContext::stopped() const noexcept {
  return _stopped.load(std::memory_order_relaxed);
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
  auto const now = TRI_microtime();
  _expires.store(now + _options.ttl);
}

bool RocksDBDumpContext::applyFilter(
    velocypack::Slice const& documentSlice) const {
  return std::ranges::all_of(_options.filters.conditions, [&](auto&& filter) {
    return basics::VelocyPackHelper::equal(documentSlice.get(filter.path),
                                           filter.value.slice(), true);
  });
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

  velocypack::Options vpackOptions;
  vpackOptions.customTypeHandler = _customTypeHandler.get();

  auto it = buildIterator(ci);
  TRI_ASSERT(it != nullptr);

  std::uint64_t docsProduced = 0;
  std::uint64_t batchesProduced = 0;
  std::uint64_t batchSize = _options.batchSize;

  VPackBuilder projectionsBuilder;
  VPackBuffer<uint8_t> sanitizationBuffer;

  for (it->Seek(lowerBound.string()); it->Valid(); it->Next()) {
    TRI_ASSERT(it->key().compare(ci.upper) < 0);

    // check if we have reached our current end position
    if (it->key().compare(upperBound.string()) >= 0) {
      break;
    }

    if (batch == nullptr) {
      batchSize = _options.batchSize;
      // note: this call can block, and it can modify batchSize!
      batch = _manager.requestBatch(*this, ci.guard.collection()->name(),
                                    batchSize, _useVPack, &vpackOptions);
    }

    TRI_ASSERT(batch != nullptr);

    auto documentSlice = velocypack::Slice(
        reinterpret_cast<std::uint8_t const*>(it->value().data()));

    if (!applyFilter(documentSlice)) {
      continue;
    }

    // Sanitize document
    VPackValueLength const inputLength = documentSlice.byteSize();
    if (basics::VelocyPackHelper::hasNonClientTypes(documentSlice)) {
      sanitizationBuffer.clear();
      sanitizationBuffer.reserve(inputLength +
                                 64);  // reserve more space since sanitization
                                       // will make documentSlice bigger
      VPackBuilder builder(sanitizationBuffer, &vpackOptions);
      basics::VelocyPackHelper::sanitizeNonClientTypes(
          documentSlice, VPackSlice::noneSlice(), builder, vpackOptions);
      documentSlice = VPackSlice(sanitizationBuffer.data());
    }

    auto storedSlice = std::invoke([&]() -> VPackSlice {
      if (_options.projections) {
        projectionsBuilder.clear();
        {
          VPackObjectBuilder ob(&projectionsBuilder);
          for (auto const& [projKey, path] : *_options.projections) {
            auto value = documentSlice.get(path);
            projectionsBuilder.add(projKey, value);
          }
        }
        return projectionsBuilder.slice();
      }

      return documentSlice;
    });

    batch->add(storedSlice);
    ++docsProduced;

    if (batch->byteSize() >= batchSize ||
        batch->count() >= _options.docsPerBatch) {
      batch->close();

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

  if (batch != nullptr && batch->count() > 0) {
    // push remainder out
    batch->close();
    TRI_ASSERT(batch->byteSize() > 0);
    // we can ignore the last one. Going to exit anyway.
    std::ignore = _channel.push(std::move(batch));
    ++batchesProduced;
    batch.reset();
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
