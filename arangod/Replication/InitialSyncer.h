////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
#include "Replication/ReplicationApplierConfiguration.h"
#include "Replication/Syncer.h"
#include "Utils/SingleCollectionTransaction.h"

#include <velocypack/Slice.h>

struct TRI_vocbase_t;

namespace arangodb {

struct DumpStats {
  // number of requests to /dump
  uint64_t numDumpRequests = 0;

  double waitedForDump = 0.0;
  double waitedForApply = 0.0;
};

struct IncrementalSyncStats {
  // number of requests to /keys?type=keys
  uint64_t numKeysRequests = 0;
  // number of requests to /keys?type=docs
  uint64_t numDocsRequests = 0;
  // number of documents we want details for
  uint64_t numDocsRequested = 0;

  uint64_t numDocsInserted = 0;
  uint64_t numDocsRemoved = 0;

  double waitedForInitial = 0.0;
  double waitedForKeys = 0.0;
  double waitedForDocs = 0.0;
};

class InitialSyncer : public Syncer {
 public:
  static constexpr double defaultBatchTimeout = 7200.0; // two hours

 public:
  explicit InitialSyncer(ReplicationApplierConfiguration const&);

  ~InitialSyncer();

 public:
  
  virtual Result run(bool incremental) = 0;
  
  /// @brief return the last log tick of the master at start
  TRI_voc_tick_t getLastLogTick() const { return _masterInfo._lastLogTick; }

  /// @brief return the collections that were synced
  std::map<TRI_voc_cid_t, std::string> const& getProcessedCollections() const {
    return _processedCollections;
  }

  std::string progress() const { return _progress; }
  
 protected:
  
  /// @brief set a progress message
  virtual void setProgress(std::string const& msg) {}

  /// @brief send a "start batch" command
  /// @param patchCount try to patch count of this collection
  ///        only effective with the incremental sync
  Result sendStartBatch(std::string const& patchCount = "");

  /// @brief send an "extend batch" command
  Result sendExtendBatch();

  /// @brief send a "finish batch" command
  Result sendFinishBatch();

 protected:
  /// @brief progress message
  std::string _progress;

  /// @brief collections synced
  std::map<TRI_voc_cid_t, std::string> _processedCollections;

  /// @brief dump batch id
  uint64_t _batchId;

  /// @brief dump batch last update time
  double _batchUpdateTime;

  /// @brief ttl for batches
  double _batchTtl;

};
}

#endif
