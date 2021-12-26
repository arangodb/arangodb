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
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <chrono>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// public rocksdb headers
#include <rocksdb/listener.h>

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Mutex.h"
#include "Basics/Thread.h"

namespace arangodb {
namespace application_features {
class ApplicationServer;
}

class RocksDBShaCalculatorThread : public arangodb::Thread {
 public:
  explicit RocksDBShaCalculatorThread(
      application_features::ApplicationServer& server, std::string const& name);

  ~RocksDBShaCalculatorThread();

  /// @brief called by RocksDB when a new .sst file is created
  void queueShaCalcFile(std::string const& pathName);

  /// @brief called by RocksDB when it deletes an .sst
  void queueDeleteFile(std::string const& pathName);

  // return [success, hash]
  static std::pair<bool, std::string> shaCalcFile(std::string const& filename);

  void checkMissingShaFiles(std::string const& pathname, int64_t requireAge);

  void signalLoop();

 protected:
  void run() override;

  void deleteObsoleteFiles();

  template<typename T>
  bool isSstFilename(T const& filename) const;

  /// @brief The following wrapper routine simplifies unit testing
  TEST_VIRTUAL std::string getRocksDBPath();

  basics::ConditionVariable _loopingCondvar;

  Mutex _pendingMutex;
  /// @brief use vectors to buffer all pending operations. this
  /// will mean that we do not necessarily process the operations
  /// in incoming order (LIFO), but rather in FIFO order. However,
  /// the processing order is not important here
  std::vector<std::string> _pendingCalculations;
  std::unordered_set<std::string> _pendingDeletions;

  /// @brief already calculated and memoized hash values
  std::unordered_map<std::string, std::string> _calculatedHashes;

  /// @brief time point when we ran the last full check over the
  /// entire directory
  std::chrono::time_point<std::chrono::steady_clock> _lastFullCheck;
};

class RocksDBShaCalculator : public rocksdb::EventListener {
 public:
  explicit RocksDBShaCalculator(application_features::ApplicationServer&,
                                bool startThread);
  virtual ~RocksDBShaCalculator();

  void OnFlushCompleted(rocksdb::DB* db,
                        const rocksdb::FlushJobInfo& flush_job_info) override;

  void OnTableFileDeleted(
      const rocksdb::TableFileDeletionInfo& /*info*/) override;

  void OnCompactionCompleted(rocksdb::DB* db,
                             const rocksdb::CompactionJobInfo& ci) override;

  void checkMissingShaFiles(std::string const& pathname, int64_t requireAge);

  void beginShutdown() { _shaThread.beginShutdown(); }

  void waitForShutdown();

 protected:
  /// thread to perform sha256 and file deletes in background
  basics::ConditionVariable _threadDone;
  RocksDBShaCalculatorThread _shaThread;
};

}  // namespace arangodb
