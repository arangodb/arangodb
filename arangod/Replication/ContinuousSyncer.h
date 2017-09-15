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

#ifndef ARANGOD_REPLICATION_CONTINUOUS_SYNCER_H
#define ARANGOD_REPLICATION_CONTINUOUS_SYNCER_H 1

#include "Basics/Common.h"
#include "Replication/Syncer.h"

#include <velocypack/Builder.h>

struct TRI_vocbase_t;
class TRI_replication_applier_t;

namespace arangodb {

namespace httpclient {
class SimpleHttpResult;
}

namespace velocypack {
class Slice;
}

class ReplicationTransaction;

enum RestrictType : uint32_t {
  RESTRICT_NONE,
  RESTRICT_INCLUDE,
  RESTRICT_EXCLUDE
};

class ContinuousSyncer : public Syncer {
 public:
  ContinuousSyncer(TRI_vocbase_t*,
                   TRI_replication_applier_configuration_t const*,
                   TRI_voc_tick_t initialTick, bool useTick, TRI_voc_tick_t barrierId);

  ~ContinuousSyncer();

 public:
  /// @brief run method, performs continuous synchronization
  int run();
  
  /// @brief finalize the synchronization of a collection by tailing the WAL
  /// and filtering on the collection name until no more data is available
  int syncCollectionFinalize(std::string& errorMsg,
                             std::string const& collectionName,
                             TRI_voc_tick_t fetchTick);

  /// @brief return the syncer's replication applier
  TRI_replication_applier_t* applier() const { return _applier; }

 private:
  /// @brief abort all ongoing transactions
  void abortOngoingTransactions();

  /// @brief set the applier progress
  void setProgress(char const*);
  void setProgress(std::string const&);

  /// @brief save the current applier state
  int saveApplierState();

  /// @brief whether or not a collection should be excluded
  bool skipMarker(TRI_voc_tick_t, arangodb::velocypack::Slice const&) const;

  /// @brief whether or not a collection should be excluded
  bool excludeCollection(std::string const&) const;

  /// @brief get local replication applier state
  int getLocalState(std::string&);

  /// @brief starts a transaction, based on the VelocyPack provided
  int startTransaction(arangodb::velocypack::Slice const&);

  /// @brief aborts a transaction, based on the VelocyPack provided
  int abortTransaction(arangodb::velocypack::Slice const&);

  /// @brief commits a transaction, based on the VelocyPack provided
  int commitTransaction(arangodb::velocypack::Slice const&);

  /// @brief process a document operation, based on the VelocyPack provided
  int processDocument(TRI_replication_operation_e, arangodb::velocypack::Slice const&,
                      std::string&);

  /// @brief renames a collection, based on the VelocyPack provided
  int renameCollection(arangodb::velocypack::Slice const&);

  /// @brief changes the properties of a collection, based on the VelocyPack
  /// provided
  int changeCollection(arangodb::velocypack::Slice const&);

  /// @brief apply a single marker from the continuous log
  int applyLogMarker(arangodb::velocypack::Slice const&, TRI_voc_tick_t, std::string&);

  /// @brief apply the data from the continuous log
  int applyLog(httpclient::SimpleHttpResult*, TRI_voc_tick_t, std::string&,
               uint64_t&, uint64_t&);

  /// @brief perform a continuous sync with the master
  int runContinuousSync(std::string&);

  /// @brief fetch the initial master state
  int fetchMasterState(std::string&, TRI_voc_tick_t, TRI_voc_tick_t,
                       TRI_voc_tick_t&);

  /// @brief run the continuous synchronization
  int followMasterLog(std::string&, TRI_voc_tick_t&, TRI_voc_tick_t, uint64_t&,
                      bool&, bool&);
  
 private:
  /// @brief pointer to the applier state
  TRI_replication_applier_t* _applier;

  /// @brief stringified chunk size
  std::string _chunkSize;

  /// @brief collection restriction type
  RestrictType _restrictType;

  /// @brief initial tick for continuous synchronization
  TRI_voc_tick_t _initialTick;

  /// @brief use the initial tick
  bool _useTick;

  /// @brief whether or not to replicate system collections
  bool _includeSystem;

  /// @brief whether or not the specified from tick must be present when
  /// fetching
  /// data from a master
  bool _requireFromPresent;

  /// @brief whether or not the applier should be verbose
  bool _verbose;

  /// @brief whether or not the master is a 2.7 or higher (and supports some
  /// newer replication APIs)
  bool _masterIs27OrHigher;

  /// @brief whether or not the replication state file has been written at least
  /// once with non-empty values. this is required in situations when the
  /// replication applier is manually started and the master has absolutely no
  /// new data to provide, and the slave get shut down. in that case, the state
  /// file would never have been written with the initial start tick, so the
  /// start tick would be lost. re-starting the slave and the replication
  /// applier
  /// with the ticks from the file would then result in a "no start tick
  /// provided"
  /// error
  bool _hasWrittenState;

  /// @brief whether we can use single operation transactions
  bool _supportsSingleOperations;

  /// @brief ignore rename, create and drop operations for collections
  bool _ignoreRenameCreateDrop;
  
  /// @brief whether or not to store the applier state
  bool _transientApplierState;

  /// @brief which transactions were open and need to be treated specially
  std::unordered_map<TRI_voc_tid_t, ReplicationTransaction*>
      _ongoingTransactions;
  
  /// @brief recycled builder for repeated document creation
  arangodb::velocypack::Builder _documentBuilder;
};
}

#endif
