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
class InitialSyncer : public Syncer {

 public:
  InitialSyncer(ReplicationApplierConfiguration const*,
                std::unordered_map<std::string, bool> const&,
                Syncer::RestrictType, bool verbose, bool skipCreateDrop);

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
  int sendStartBatch(std::string&);

  /// @brief send an "extend batch" command
  int sendExtendBatch();

  /// @brief send a "finish batch" command
  int sendFinishBatch();

 protected:
  /// @brief progress message
  std::string _progress;

  /// @brief collection restriction
  std::unordered_map<std::string, bool> _restrictCollections;

  /// @brief collections synced
  std::map<TRI_voc_cid_t, std::string> _processedCollections;

  /// @brief dump batch id
  uint64_t _batchId;

  /// @brief dump batch last update time
  double _batchUpdateTime;

  /// @brief ttl for batches
  int _batchTtl;

  // in the cluster case it is a total NOGO to create or drop collections
  // because this HAS to be handled in the schmutz. otherwise it forgets who
  // the leader was etc.
  bool _skipCreateDrop;

};
}

#endif
