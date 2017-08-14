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

#ifndef ARANGOD_MMFILES_COLLECTOR_THREAD_H
#define ARANGOD_MMFILES_COLLECTOR_THREAD_H 1

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Mutex.h"
#include "Basics/Thread.h"
#include "MMFiles/MMFilesCollectorCache.h"
#include "MMFiles/MMFilesDatafile.h"
#include "MMFiles/MMFilesDitch.h"
#include "VocBase/voc-types.h"

namespace arangodb {
class LogicalCollection;
class MMFilesLogfileManager;
class MMFilesWalLogfile;
class SingleCollectionTransaction;

class MMFilesCollectorThread final : public Thread {
  MMFilesCollectorThread(MMFilesCollectorThread const&) = delete;
  MMFilesCollectorThread& operator=(MMFilesCollectorThread const&) = delete;

 public:
  explicit MMFilesCollectorThread(MMFilesLogfileManager*);
  ~MMFilesCollectorThread() { shutdown(); }

 public:
  void beginShutdown() override final;

 public:
  /// @brief wait for the collector result
  int waitForResult(uint64_t);

  /// @brief signal the thread that there is something to do
  void signal();
  
  /// @brief force the shutdown by setting _forcedStopIterations
  void forceStop();

  /// @brief check whether there are queued operations left
  bool hasQueuedOperations();

  /// @brief check whether there are queued operations left for the given
  /// collection
  bool hasQueuedOperations(TRI_voc_cid_t);

  // execute a callback during a phase in which the collector has nothing
  // queued. This is used in the DatabaseManagerThread when dropping
  // a database to avoid existence of ditches of type DOCUMENT.
  bool executeWhileNothingQueued(std::function<void()> const& cb);

 protected:
  /// @brief main loop
  void run() override;

 private:
  /// @brief process a single marker in collector step 2
  void processCollectionMarker(
      arangodb::SingleCollectionTransaction&,
      arangodb::LogicalCollection*, MMFilesCollectorCache*, MMFilesCollectorOperation const&);

  /// @brief return the number of queued operations
  size_t numQueuedOperations();

  /// @brief step 1: perform collection of a logfile (if any)
  int collectLogfiles(bool&);

  /// @brief step 2: process all still-queued collection operations
  int processQueuedOperations(bool&);

  /// @brief process all operations for a single collection
  int processCollectionOperations(MMFilesCollectorCache*);

  /// @brief collect one logfile
  int collect(MMFilesWalLogfile*);

  /// @brief transfer markers into a collection
  int transferMarkers(MMFilesWalLogfile*, TRI_voc_cid_t, TRI_voc_tick_t,
                      int64_t, MMFilesOperationsType const&);

  /// @brief insert the collect operations into a per-collection queue
  int queueOperations(MMFilesWalLogfile*, std::unique_ptr<MMFilesCollectorCache>&);

  /// @brief update a collection's datafile information
  int updateDatafileStatistics(LogicalCollection*, MMFilesCollectorCache*);

  void broadcastCollectorResult(int res); 

 private:
  /// @brief the logfile manager
  MMFilesLogfileManager* _logfileManager;

  /// @brief condition variable for the collector thread
  basics::ConditionVariable _condition;

  /// @brief used for counting the number of iterations during
  /// forcedIterations. defaults to -1
  int _forcedStopIterations;

  /// @brief operations lock
  arangodb::Mutex _operationsQueueLock;

  /// @brief operations to collect later
  std::unordered_map<TRI_voc_cid_t, std::vector<MMFilesCollectorCache*>>
      _operationsQueue;

  /// @brief whether or not the queue is currently in use
  bool _operationsQueueInUse;

  /// @brief number of pending operations in collector queue
  std::atomic<uint64_t> _numPendingOperations;

  /// @brief condition variable for the collector thread result
  basics::ConditionVariable _collectorResultCondition;

  /// @brief last collector result
  int _collectorResult;

  /// @brief wait interval for the collector thread when idle
  static uint64_t const Interval;
};

}

#endif
