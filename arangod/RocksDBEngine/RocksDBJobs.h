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

#ifndef ARANGOD_ROCKSDB_ENGINE_ROCKSDB_JOBS_H
#define ARANGOD_ROCKSDB_ENGINE_ROCKSDB_JOBS_H 1

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Basics/Result.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"

#include <deque>
#include <memory>
#include <string>

namespace arangodb {
namespace application_features {
class ApplicationServer;
}

class RocksDBEngine;

class RocksDBJob {
 public:
  RocksDBJob(RocksDBJob const& other) = delete;
  RocksDBJob& operator=(RocksDBJob const& other) = delete;

  explicit RocksDBJob(std::string const& label);
  virtual ~RocksDBJob();

  virtual bool needsRecovery() const = 0;

  virtual Result run(RocksDBEngine& engine) = 0;

  std::string const& label() const { return _label; }

 private:
  std::string const _label;
};

class RocksDBCollectionDropJob : public RocksDBJob {
 public:
  RocksDBCollectionDropJob(
      std::string const& database, std::string const& collection, RocksDBKeyBounds bounds, 
      bool prefixSameAsStart, bool useRangeDelete, bool scheduleCompaction);

  bool needsRecovery() const override { return true; }
  Result run(RocksDBEngine& engine) override;
 
 private:
  std::string const _database;
  std::string const _collection;
  RocksDBKeyBounds const _bounds;
  bool const _prefixSameAsStart;
  bool const _useRangeDelete;
  bool const _scheduleCompaction;
};

class RocksDBCollectionCompactJob : public RocksDBJob {
 public:
  RocksDBCollectionCompactJob(std::string const& database, std::string const& collection, RocksDBKeyBounds bounds);

  bool needsRecovery() const override { return false; }
  Result run(RocksDBEngine& engine) override;
 
 private:
  std::string const _database;
  std::string const _collection;
  RocksDBKeyBounds const _bounds;
};

class RocksDBIndexDropJob : public RocksDBJob {
 public:
  RocksDBIndexDropJob(
      std::string const& database, std::string const& collection, std::string const& index, RocksDBKeyBounds bounds, 
      bool prefixSameAsStart, bool useRangeDelete, bool scheduleCompaction);

  bool needsRecovery() const override { return true; }
  Result run(RocksDBEngine& engine) override;
 
 private:
  std::string const _database;
  std::string const _collection;
  std::string const _index;
  RocksDBKeyBounds const _bounds;
  bool const _prefixSameAsStart;
  bool const _useRangeDelete;
  bool const _scheduleCompaction;
};

class RocksDBIndexCompactJob : public RocksDBJob {
 public:
  RocksDBIndexCompactJob(std::string const& database, std::string const& collection, std::string const& index, RocksDBKeyBounds bounds);

  bool needsRecovery() const override { return false; }
  Result run(RocksDBEngine& engine) override;
 
 private:
  std::string const _database;
  std::string const _collection;
  std::string const _index;
  RocksDBKeyBounds const _bounds;
};

class RocksDBJobScheduler {
 public:
  RocksDBJobScheduler(
      arangodb::application_features::ApplicationServer& server, RocksDBEngine& engine, uint64_t maxConcurrentJobs);
  ~RocksDBJobScheduler();

  Result queueJob(std::unique_ptr<RocksDBJob> job);
  void dispatchJobs();

  void beginShutdown();

 private:
  arangodb::application_features::ApplicationServer& _server;
  RocksDBEngine& _engine;
  uint64_t const _maxConcurrentJobs;

  Mutex _mutex;
  std::deque<std::unique_ptr<RocksDBJob>> _pendingJobs;
  uint64_t _startedJobs;
};

}  // namespace arangodb

#endif
