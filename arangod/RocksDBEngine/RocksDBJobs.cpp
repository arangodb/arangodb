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

#include "RocksDBJobs.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/MutexLocker.h"
#include "Basics/system-functions.h"
#include "Basics/voc-errors.h"
#include "Logger/LogMacros.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "Scheduler/SchedulerFeature.h"
#include "Scheduler/SupervisedScheduler.h"
#include "StorageEngine/EngineSelectorFeature.h"

using namespace arangodb;

RocksDBJob::RocksDBJob(std::string const& label)
    : _label(label) {}

RocksDBJob::~RocksDBJob() = default;

RocksDBCollectionDropJob::RocksDBCollectionDropJob(
    std::string const& database, std::string const& collection,
    RocksDBKeyBounds bounds, bool prefixSameAsStart, bool useRangeDelete, bool scheduleCompaction) 
    : RocksDBJob("dropping collection " + database + "/" + collection),
      _database(database),
      _collection(collection),
      _bounds(std::move(bounds)), 
      _prefixSameAsStart(prefixSameAsStart),
      _useRangeDelete(useRangeDelete),
      _scheduleCompaction(scheduleCompaction) {}

Result RocksDBCollectionDropJob::run(RocksDBEngine& engine) {
  Result res = rocksutils::removeLargeRange(engine.db(), _bounds,
                                            _prefixSameAsStart, _useRangeDelete);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // check if documents have been deleted
  size_t numDocs =
      rocksutils::countKeyRange(engine.db(), _bounds, _prefixSameAsStart);
  if (numDocs > 0) {
    std::string errorMsg(
        "deletion check in collection drop failed - not all documents "
        "have been deleted. remaining: ");
    errorMsg.append(std::to_string(numDocs));
    res.reset(TRI_ERROR_INTERNAL, errorMsg);
  }
#endif

  if (res.ok() && _scheduleCompaction) {
    auto compactionJob = std::make_unique<RocksDBCollectionCompactJob>(_database, _collection, _bounds);
    res = engine.queueBackgroundJob(std::move(compactionJob));
    if (res.is(TRI_ERROR_SHUTTING_DOWN)) {
      res.reset();
    }
  }

  return res;
}

RocksDBCollectionCompactJob::RocksDBCollectionCompactJob(
    std::string const& database, std::string const& collection, RocksDBKeyBounds bounds)
    : RocksDBJob("compacting collection range " + database + "/" + collection),
      _database(database),
      _collection(collection),
      _bounds(std::move(bounds)) {} 

Result RocksDBCollectionCompactJob::run(RocksDBEngine& engine) {
  rocksdb::TransactionDB* db = engine.db();
  rocksdb::CompactRangeOptions opts;
  auto* cf = _bounds.columnFamily();
  TRI_ASSERT(cf == RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::Documents));
  rocksdb::Slice b = _bounds.start(), e = _bounds.end();
  return rocksutils::convertStatus(db->CompactRange(opts, cf, &b, &e));
}

RocksDBIndexDropJob::RocksDBIndexDropJob(
    std::string const& database, std::string const& collection, std::string const& index, 
    RocksDBKeyBounds bounds, bool prefixSameAsStart, bool useRangeDelete, bool scheduleCompaction) 
    : RocksDBJob("dropping index " + database + "/" + collection + "/" + index),
      _database(database),
      _collection(collection),
      _index(index),
      _bounds(std::move(bounds)), 
      _prefixSameAsStart(prefixSameAsStart),
      _useRangeDelete(useRangeDelete),
      _scheduleCompaction(scheduleCompaction) {}

Result RocksDBIndexDropJob::run(RocksDBEngine& engine) {
  Result res = rocksutils::removeLargeRange(engine.db(), _bounds,
                                            _prefixSameAsStart, _useRangeDelete);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // check if documents have been deleted
  size_t numDocs =
      rocksutils::countKeyRange(engine.db(), _bounds, _prefixSameAsStart);
  if (numDocs > 0) {
    std::string errorMsg(
        "deletion check in index drop failed - not all documents in the index "
        "have been deleted. remaining: ");
    errorMsg.append(std::to_string(numDocs));
    res.reset(TRI_ERROR_INTERNAL, errorMsg);
  }
#endif

  if (res.ok() && _scheduleCompaction) {
    auto compactionJob = std::make_unique<RocksDBIndexCompactJob>(_database, _collection, _index, _bounds);
    res = engine.queueBackgroundJob(std::move(compactionJob));
    if (res.is(TRI_ERROR_SHUTTING_DOWN)) {
      res.reset();
    }
  }

  return res;
}

RocksDBIndexCompactJob::RocksDBIndexCompactJob(
    std::string const& database, std::string const& collection, std::string const& index, RocksDBKeyBounds bounds)
    : RocksDBJob("compacting index range " + database + "/" + collection + "/" + index),
      _database(database),
      _collection(collection),
      _index(index),
      _bounds(std::move(bounds)) {} 

Result RocksDBIndexCompactJob::run(RocksDBEngine& engine) {
  Result res;

  rocksdb::TransactionDB* db = engine.db();
  rocksdb::CompactRangeOptions opts;
  auto* cf = _bounds.columnFamily();
  if (cf != RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::Invalid) &&
      cf != RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::Definitions)) {
    rocksdb::Slice b = _bounds.start(), e = _bounds.end();
    res.reset(rocksutils::convertStatus(db->CompactRange(opts, cf, &b, &e)));
  }

  return res;
}

RocksDBJobScheduler::RocksDBJobScheduler(
    arangodb::application_features::ApplicationServer& server, RocksDBEngine& engine, uint64_t maxConcurrentJobs)
    : _server(server),
      _engine(engine), 
      _maxConcurrentJobs(maxConcurrentJobs),
      _startedJobs(0) {}

RocksDBJobScheduler::~RocksDBJobScheduler() = default;

void RocksDBJobScheduler::beginShutdown() {
  // currently not doing anything...
}

Result RocksDBJobScheduler::queueJob(std::unique_ptr<RocksDBJob> job) {
  if (_server.isStopping()) {
    return {TRI_ERROR_SHUTTING_DOWN};
  }

  {
    MUTEX_LOCKER(lock, _mutex);
    _pendingJobs.emplace_back(std::move(job));
  }

  dispatchJobs();
  return {};
}

void RocksDBJobScheduler::dispatchJobs() {
  auto* scheduler = arangodb::SchedulerFeature::SCHEDULER;
  TRI_ASSERT(scheduler != nullptr);

  if (scheduler == nullptr) {
    return;
  }
  
  MUTEX_LOCKER(lock, _mutex);

  if (_pendingJobs.empty()) {
    // nothing to do
    return;
  }
  
  if (_startedJobs < _maxConcurrentJobs) {
    // we are still holding the lock here
    ++_startedJobs;
    
    bool queued = scheduler->queue(arangodb::RequestLane::INTERNAL_LOW, [this]() {
      size_t jobsExecuted = 0;

      while (true) {
        MUTEX_LOCKER(lock, _mutex);
        
        if (_pendingJobs.empty() || jobsExecuted == 5) {
          // no pending job. quick exit
          TRI_ASSERT(_startedJobs > 0);
          --_startedJobs;
          return;
        }
        
        ++jobsExecuted;

        try {
          // pop first element from queue
          auto job = std::move(_pendingJobs.front());
          _pendingJobs.pop_front();
            
          lock.unlock();
     
          double start = TRI_microtime();
          LOG_TOPIC("19ddf", INFO, Logger::ENGINES) 
              << "starting rocksdb background job '" << job->label() << "'";

          Result res;
          try {
            res = job->run(_engine);
          } catch (basics::Exception const& ex) {
            res.reset(ex.code(), ex.what());
          } catch (std::exception const& ex) {
            res.reset(TRI_ERROR_INTERNAL, ex.what());
          } catch (...) {
            res.reset(TRI_ERROR_INTERNAL, "unknown exception");
          }

          if (res.ok()) {
            LOG_TOPIC("0a5a0", DEBUG, Logger::ENGINES) 
                << "successfully completed rocksdb background job " << job->label() 
                << " after " << Logger::FIXED(TRI_microtime() - start, 6) << " s";
          } else {
            LOG_TOPIC("75e5d", WARN, Logger::ENGINES) 
                << "rocksdb background job " << job->label() << " failed after "
                << Logger::FIXED(TRI_microtime() - start, 6) << " s: " 
                << res.errorMessage(); 
          }
        } catch (...) {
          // we must catch and smile away all errors here, because we need to 
          // properly count down _startedJobs above when we exit
          LOG_TOPIC("109aa", WARN, Logger::ENGINES)
            << "caught unknown exception during rocksdb background job execution";
        }
      } // next job
    });

    if (!queued) {
      // still holding the lock
      --_startedJobs;
    }
  }
}
