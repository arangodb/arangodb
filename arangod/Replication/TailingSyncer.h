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
  /// @brief set the applier progress
  virtual void setProgress(std::string const&);

  /// @brief abort all ongoing transactions
  void abortOngoingTransactions();

  /// @brief whether or not a collection should be excluded
  bool skipMarker(TRI_voc_tick_t, arangodb::velocypack::Slice const&) const;

  /// @brief whether or not a collection should be excluded
  bool isExcludedCollection(std::string const&) const;

  /// @brief starts a transaction, based on the VelocyPack provided
  Result startTransaction(arangodb::velocypack::Slice const&);

  /// @brief aborts a transaction, based on the VelocyPack provided
  Result abortTransaction(arangodb::velocypack::Slice const&);

  /// @brief commits a transaction, based on the VelocyPack provided
  Result commitTransaction(arangodb::velocypack::Slice const&);
  
  /// @brief process db create or drop markers
  Result processDBMarker(TRI_replication_operation_e, velocypack::Slice const&);

  /// @brief process a document operation, based on the VelocyPack provided
  Result processDocument(TRI_replication_operation_e,
                         arangodb::velocypack::Slice const&);

  /// @brief renames a collection, based on the VelocyPack provided
  Result renameCollection(arangodb::velocypack::Slice const&);

  /// @brief changes the properties of a collection, based on the VelocyPack
  /// provided
  Result changeCollection(arangodb::velocypack::Slice const&);

  /// @brief apply a single marker from the continuous log
  Result applyLogMarker(arangodb::velocypack::Slice const&, TRI_voc_tick_t);

  /// @brief apply the data from the continuous log
  Result applyLog(httpclient::SimpleHttpResult*, TRI_voc_tick_t firstRegularTick, 
                  uint64_t& processedMarkers, uint64_t& ignoreCount);

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
