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

#ifndef ARANGOD_ROCKSDB_ENGINE_SYNC_THREAD_H
#define ARANGOD_ROCKSDB_ENGINE_SYNC_THREAD_H 1

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Result.h"
#include "Basics/Thread.h"

#include <rocksdb/types.h>

#include <chrono>

namespace rocksdb {
class DB;
}

namespace arangodb {

class RocksDBEngine;

class RocksDBSyncThread final : public Thread {
 public:
  RocksDBSyncThread(RocksDBEngine& engine, 
                    std::chrono::milliseconds interval,
                    std::chrono::milliseconds delayThreshold);

  ~RocksDBSyncThread();

  void beginShutdown() override;

  /// @brief updates last sync time and calls the synchronization
  /// this is the preferred method to call when trying to avoid redundant
  /// syncs by foreground work and the background sync thread
  Result syncWal();

  /// @brief unconditionally syncs the RocksDB WAL, static variant
  static Result sync(rocksdb::DB* db);

 protected:
  void run() override;

 private:
  RocksDBEngine& _engine;

  /// @brief the sync interval
  std::chrono::milliseconds const _interval;

  /// @brief last time we synced the RocksDB WAL
  std::chrono::time_point<std::chrono::steady_clock> _lastSyncTime;

  /// @brief the last definitely synced RocksDB WAL sequence number
  rocksdb::SequenceNumber _lastSequenceNumber;

  /// @brief threshold for self-observation of WAL disk syncs.
  /// if the last WAL sync happened longer ago than this configured
  /// threshold, a warning will be logged on every invocation of the
  /// sync thread
  std::chrono::milliseconds const _delayThreshold;

  /// @brief protects _lastSyncTime and _lastSequenceNumber
  arangodb::basics::ConditionVariable _condition;
};
}  // namespace arangodb

#endif
