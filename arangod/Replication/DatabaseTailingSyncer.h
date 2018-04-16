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

#ifndef ARANGOD_REPLICATION_DATABASE_TAILING_SYNCER_H
#define ARANGOD_REPLICATION_DATABASE_TAILING_SYNCER_H 1

#include "TailingSyncer.h"
#include "Replication/ReplicationApplierConfiguration.h"
#include "Replication/DatabaseReplicationApplier.h"

namespace arangodb {
class DatabaseReplicationApplier;

class DatabaseTailingSyncer : public TailingSyncer {
 public:
  DatabaseTailingSyncer(
    TRI_vocbase_t& vocbase,
    ReplicationApplierConfiguration const& configuration,
    TRI_voc_tick_t initialTick,
    bool useTick,
    TRI_voc_tick_t barrierId
  );

  TRI_vocbase_t* resolveVocbase(velocypack::Slice const&) override { return _vocbase; }

  /// @brief return the syncer's replication applier
  DatabaseReplicationApplier* applier() const {
    return static_cast<DatabaseReplicationApplier*>(_applier);
  }

  /// @brief finalize the synchronization of a collection by tailing the WAL
  /// and filtering on the collection name until no more data is available
  Result syncCollectionFinalize(std::string const& collectionName);

 protected:

  /// @brief save the current applier state
  Result saveApplierState() override;

  TRI_vocbase_t* vocbase() const {
    TRI_ASSERT(vocbases().size() == 1);
    return &(vocbases().begin()->second.database());
  }

 private:

  /// @brief vocbase to use for this run
  TRI_vocbase_t* _vocbase;
  
};
}

#endif