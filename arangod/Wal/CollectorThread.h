////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_WAL_COLLECTOR_THREAD_H
#define ARANGOD_WAL_COLLECTOR_THREAD_H 1

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Mutex.h"
#include "Basics/Thread.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/datafile.h"
#include "VocBase/Ditch.h"
#include "VocBase/voc-types.h"
#include "Wal/CollectorCache.h"
#include "Wal/Logfile.h"

namespace arangodb {
class LogicalCollection;

namespace wal {

class LogfileManager;
class Logfile;

class CollectorThread final : public Thread {
  CollectorThread(CollectorThread const&) = delete;
  CollectorThread& operator=(CollectorThread const&) = delete;

 public:
  explicit CollectorThread(LogfileManager*);
  ~CollectorThread() { shutdown(); }

 public:
  void beginShutdown() override final;

 public:
  /// @brief wait for the collector result
  int waitForResult(uint64_t);

  /// @brief signal the thread that there is something to do
  void signal();

  /// @brief check whether there are queued operations left
  bool hasQueuedOperations();

  /// @brief check whether there are queued operations left for the given
  /// collection
  bool hasQueuedOperations(TRI_voc_cid_t);

 protected:
  /// @brief main loop
  void run() override;

 private:
  /// @brief process a single marker in collector step 2
  void processCollectionMarker(
      arangodb::SingleCollectionTransaction&,
      arangodb::LogicalCollection*, CollectorCache*, CollectorOperation const&);

  /// @brief return the number of queued operations
  size_t numQueuedOperations();

  /// @brief step 1: perform collection of a logfile (if any)
  int collectLogfiles(bool&);

  /// @brief step 2: process all still-queued collection operations
  int processQueuedOperations(bool&);

  /// @brief process all operations for a single collection
  int processCollectionOperations(CollectorCache*);

  /// @brief collect one logfile
  int collect(Logfile*);

  /// @brief transfer markers into a collection
  int transferMarkers(arangodb::wal::Logfile*, TRI_voc_cid_t, TRI_voc_tick_t,
                      int64_t, OperationsType const&);

  /// @brief insert the collect operations into a per-collection queue
  int queueOperations(arangodb::wal::Logfile*, std::unique_ptr<CollectorCache>&);

  /// @brief update a collection's datafile information
  int updateDatafileStatistics(LogicalCollection*, CollectorCache*);

 private:
  /// @brief the logfile manager
  LogfileManager* _logfileManager;

  /// @brief condition variable for the collector thread
  basics::ConditionVariable _condition;

  /// @brief operations lock
  arangodb::Mutex _operationsQueueLock;

  /// @brief operations to collect later
  std::unordered_map<TRI_voc_cid_t, std::vector<CollectorCache*>>
      _operationsQueue;

  /// @brief whether or not the queue is currently in use
  bool _operationsQueueInUse;

  /// @brief number of pending operations in collector queue
  uint64_t _numPendingOperations;

  /// @brief condition variable for the collector thread result
  basics::ConditionVariable _collectorResultCondition;

  /// @brief last collector result
  int _collectorResult;

  /// @brief wait interval for the collector thread when idle
  static uint64_t const Interval;
};
}
}

#endif
