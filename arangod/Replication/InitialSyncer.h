////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_REPLICATION_INITIAL_SYNCER_H
#define ARANGOD_REPLICATION_INITIAL_SYNCER_H 1

#include "Basics/Common.h"
#include "Basics/Result.h"
#include "Replication/ReplicationApplierConfiguration.h"
#include "Replication/Syncer.h"
#include "Replication/utilities.h"
#include "Scheduler/Scheduler.h"

#include <velocypack/Slice.h>

struct TRI_vocbase_t;

namespace arangodb {

struct InitialSyncerDumpStats {
  // total number of requests to /_api/replication/dump
  uint64_t numDumpRequests = 0;
  // total time spent waiting for responses to /_api/replication/dump
  double waitedForDump = 0.0;
  // total time spent for locally applying dump markers
  double waitedForApply = 0.0;
};

struct InitialSyncerIncrementalSyncStats {
  // total number of requests to /_api/replication/keys?type=keys
  uint64_t numKeysRequests = 0;
  // total number of requests to /_api/replication/keys?type=docs
  uint64_t numDocsRequests = 0;
  // total number of documents that for which document data were requested
  uint64_t numDocsRequested = 0;
  // total number of insert operations performed during sync
  uint64_t numDocsInserted = 0;
  // total number of remove operations performed during sync
  uint64_t numDocsRemoved = 0;
  // total time spent waiting on response for initial call to
  // /_api/replication/keys
  double waitedForInitial = 0.0;
  // total time spent waiting for responses to /_api/replication/keys?type=keys
  double waitedForKeys = 0.0;
  // total time spent waiting for responses to /_api/replication/keys?type=docs
  double waitedForDocs = 0.0;
  double waitedForInsertions = 0.0;
  double waitedForRemovals = 0.0;
  double waitedForKeyLookups = 0.0;
};

class InitialSyncer : public Syncer {
 public:
  explicit InitialSyncer(ReplicationApplierConfiguration const&,
                         replutils::ProgressInfo::Setter s = [](std::string const&) -> void {});

  ~InitialSyncer();

 public:
  virtual Result run(bool incremental, char const* context = nullptr) = 0;

  /// @brief return the last log tick of the master at start
  TRI_voc_tick_t getLastLogTick() const { return _state.master.lastLogTick; }

  /// @brief return the collections that were synced
  std::map<TRI_voc_cid_t, std::string> const& getProcessedCollections() const {
    return _progress.processedCollections;
  }

  std::string progress() const { return _progress.message; }

 protected:
  /// @brief start a recurring task to extend the batch
  void startRecurringBatchExtension();

 protected:
  replutils::BatchInfo _batch;
  replutils::ProgressInfo _progress;
  
  /// recurring task to keep the batch alive
  Scheduler::WorkHandle _batchPingTimer;
};
}  // namespace arangodb

#endif
