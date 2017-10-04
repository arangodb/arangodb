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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REPLICATION_TAILING_SYNCER_H
#define ARANGOD_REPLICATION_TAILING_SYNCER_H 1

#include "Basics/Common.h"
#include "Replication/ReplicationApplierConfiguration.h"
#include "Replication/Syncer.h"

#include <velocypack/Builder.h>

struct TRI_vocbase_t;

namespace arangodb {

namespace httpclient {
class SimpleHttpResult;
}

namespace velocypack {
class Slice;
}

class ReplicationTransaction;

class TailingSyncer : public Syncer {
 public:
  TailingSyncer(ReplicationApplierConfiguration const&,
                TRI_voc_tick_t initialTick, TRI_voc_tick_t barrierId);

  virtual ~TailingSyncer();

 public:
  /// @brief run method, performs continuous synchronization
  virtual int run() = 0;

 protected:
  /// @brief abort all ongoing transactions
  void abortOngoingTransactions();

  /// @brief whether or not a collection should be excluded
  bool skipMarker(TRI_voc_tick_t, arangodb::velocypack::Slice const&) const;

  /// @brief whether or not a collection should be excluded
  bool isExcludedCollection(std::string const&) const;

  /// @brief get local replication applier state
  int getLocalState(std::string&);

  /// @brief starts a transaction, based on the VelocyPack provided
  int startTransaction(arangodb::velocypack::Slice const&);

  /// @brief aborts a transaction, based on the VelocyPack provided
  int abortTransaction(arangodb::velocypack::Slice const&);

  /// @brief commits a transaction, based on the VelocyPack provided
  int commitTransaction(arangodb::velocypack::Slice const&);
  
  /// @brief process db create or drop markers
  int processDBMarker(TRI_replication_operation_e, velocypack::Slice const&);

  /// @brief process a document operation, based on the VelocyPack provided
  int processDocument(TRI_replication_operation_e,
                      arangodb::velocypack::Slice const&, std::string&);

  /// @brief renames a collection, based on the VelocyPack provided
  int renameCollection(arangodb::velocypack::Slice const&);

  /// @brief changes the properties of a collection, based on the VelocyPack
  /// provided
  int changeCollection(arangodb::velocypack::Slice const&);

  /// @brief apply a single marker from the continuous log
  int applyLogMarker(arangodb::velocypack::Slice const&, TRI_voc_tick_t,
                     std::string&);

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

  /// @brief called before marker is processed
  virtual void preApplyMarker(TRI_voc_tick_t firstRegularTick,
                             TRI_voc_tick_t newTick) {}
  /// @brief called after a marker was processed
  virtual void postApplyMarker(uint64_t processedMarkers, bool skipped) {}

 protected:

  /// @brief initial tick for continuous synchronization
  TRI_voc_tick_t _initialTick;

  /// @brief whether or not the specified from tick must be present when
  /// fetching
  /// data from a master
  bool _requireFromPresent;

  /// @brief whether we can use single operation transactions
  bool _supportsSingleOperations;

  /// @brief ignore rename, create and drop operations for collections
  bool _ignoreRenameCreateDrop;

  /// @brief ignore create / drop database
  bool _ignoreDatabaseMarkers;

  /// @brief which transactions were open and need to be treated specially
  std::unordered_map<TRI_voc_tid_t, ReplicationTransaction*>
      _ongoingTransactions;

  /// @brief recycled builder for repeated document creation
  arangodb::velocypack::Builder _documentBuilder;
};
}

#endif
