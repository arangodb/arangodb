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

#ifndef ARANGOD_REPLICATION_DATABASE_TAILING_SYNCER_H
#define ARANGOD_REPLICATION_DATABASE_TAILING_SYNCER_H 1

#include "Replication/DatabaseReplicationApplier.h"
#include "Replication/ReplicationApplierConfiguration.h"
#include "Replication/utilities.h"
#include "TailingSyncer.h"

#include <memory>

namespace arangodb {
class DatabaseReplicationApplier;

class DatabaseTailingSyncer : public TailingSyncer {
 private:
  // constructor is private, as DatabaseTailingSyncer uses shared_from_this() and
  // we must ensure that it is only created via make_shared.
  DatabaseTailingSyncer(TRI_vocbase_t& vocbase,
                        ReplicationApplierConfiguration const& configuration,
                        TRI_voc_tick_t initialTick, bool useTick);

 public:
  static std::shared_ptr<DatabaseTailingSyncer> create(TRI_vocbase_t& vocbase,
                                                       ReplicationApplierConfiguration const& configuration,
                                                       TRI_voc_tick_t initialTick, bool useTick);

  TRI_vocbase_t* resolveVocbase(velocypack::Slice const&) override {
    return _vocbase;
  }

  /// @brief return the syncer's replication applier
  DatabaseReplicationApplier* applier() const {
    return static_cast<DatabaseReplicationApplier*>(_applier);
  }

  /// @brief finalize the synchronization of a collection by tailing the WAL
  /// and filtering on the collection name until no more data is available
  Result syncCollectionFinalize(arangodb::replutils::LeaderInfo const& leaderInfo,
                                std::string const& collectionName, char const* context) {
    TRI_voc_tick_t dummy = 0;
    bool dummyDidTimeout = false;
    double dummyTimeout = 300.0;
    return syncCollectionCatchupInternal(leaderInfo, collectionName, dummyTimeout, true,
                                         dummy, dummyDidTimeout, context);
  }

  /// @brief catch up with changes in a leader shard by doing the same
  /// as in syncCollectionFinalize, but potentially stopping earlier.
  /// This function will use some heuristics to stop early, when most
  /// of the catching up is already done. In this case, the replication
  /// will end and store the tick to which it got in the argument `until`.
  /// The idea is that one can use `syncCollectionCatchup` without stopping
  /// writes on the leader to catch up mostly. Then one can stop writes
  /// by getting an exclusive lock on the leader and use
  /// `syncCollectionFinalize` to finish off the rest.
  /// Internally, both use `syncCollectionCatchupInternal`.
  Result syncCollectionCatchup(arangodb::replutils::LeaderInfo const& leaderInfo,
                               std::string const& collectionName, double timeout,
                               TRI_voc_tick_t& until, bool& didTimeout, char const* context) {
    return syncCollectionCatchupInternal(leaderInfo, collectionName, timeout, false, until, didTimeout, context);
  }

 protected:
  Result syncCollectionCatchupInternal(arangodb::replutils::LeaderInfo const& leaderInfo, 
                                       std::string const& collectionName,
                                       double timeout, bool hard,
                                       TRI_voc_tick_t& until, bool& didTimeout, char const* context);
  /// @brief save the current applier state
  Result saveApplierState() override;

  TRI_vocbase_t* vocbase() const {
    TRI_ASSERT(vocbases().size() == 1);
    return &(vocbases().begin()->second.database());
  }

  /// @brief whether or not we should skip a specific marker
  bool skipMarker(arangodb::velocypack::Slice const& slice) override;

 private:
  /// @brief vocbase to use for this run
  TRI_vocbase_t* _vocbase;

  /// @brief translation between globallyUniqueId and collection name
  std::unordered_map<std::string, std::string> _translations;

  bool _queriedTranslations;
};
}  // namespace arangodb

#endif
