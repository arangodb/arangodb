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
#include "Basics/Result.h"
#include "Replication/ReplicationApplierConfiguration.h"
#include "Replication/Syncer.h"
#include "Replication/utilities.h"
#include "Utils/SingleCollectionTransaction.h"

#include <velocypack/Slice.h>

struct TRI_vocbase_t;

namespace arangodb {
class InitialSyncer : public Syncer {
 public:
  explicit InitialSyncer(ReplicationApplierConfiguration const&,
                         replutils::ProgressInfo::Setter s =
                             [](std::string const&) -> void {});

  ~InitialSyncer();

 public:
  virtual Result run(bool incremental) = 0;

  /// @brief return the last log tick of the master at start
  TRI_voc_tick_t getLastLogTick() const { return _state.master.lastLogTick; }

  /// @brief return the collections that were synced
  std::map<TRI_voc_cid_t, std::string> const& getProcessedCollections() const {
    return _progress.processedCollections;
  }

  std::string progress() const { return _progress.message; }

 protected:
  replutils::BatchInfo _batch;
  replutils::ProgressInfo _progress;
};
}  // namespace arangodb

#endif
