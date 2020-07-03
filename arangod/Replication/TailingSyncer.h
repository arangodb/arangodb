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
class InitialSyncer;
class ReplicationApplier;
class ReplicationTransaction;

namespace httpclient {
class SimpleHttpResult;
}

namespace velocypack {
class Slice;
}

struct ApplyStats {
  uint64_t processedMarkers = 0;
  uint64_t processedDocuments = 0;
  uint64_t processedRemovals = 0;
};

class TailingSyncer : public Syncer {
 public:
  TailingSyncer(ReplicationApplier* applier, ReplicationApplierConfiguration const&,
                TRI_voc_tick_t initialTick, bool useTick, TRI_voc_tick_t barrierId);

  virtual ~TailingSyncer();

 public:
  /// @brief run method, performs continuous synchronization
  /// catches exceptions
  Result run();
  
 protected:
  /// @brief decide based on _masterInfo which api to use
  virtual std::string tailingBaseUrl(std::string const& command);

  /// @brief set the applier progress
  void setProgress(std::string const&);

  /// @brief abort all ongoing transactions
  void abortOngoingTransactions() noexcept;

  /// @brief abort all ongoing transactions for a specific database
  void abortOngoingTransactions(std::string const& dbName);

  /// @brief count all ongoing transactions for a specific database
  /// used only from within assertions
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  size_t countOngoingTransactions(arangodb::velocypack::Slice slice) const;
#endif

  /// @brief whether or not the are multiple ongoing transactions for one
  /// database
  /// used only from within assertions
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  bool hasMultipleOngoingTransactions() const;
#endif

  /// @brief whether or not a collection should be excluded
  bool skipMarker(TRI_voc_tick_t firstRegulaTick, arangodb::velocypack::Slice const& slice,
                  TRI_voc_tick_t actualMarkerTick, TRI_replication_operation_e type);

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
  Result processDocument(TRI_replication_operation_e, arangodb::velocypack::Slice const&);

  /// @brief renames a collection, based on the VelocyPack provided
  Result renameCollection(arangodb::velocypack::Slice const&);

  /// @brief changes the properties of a collection,
  /// based on the VelocyPack provided
  Result changeCollection(arangodb::velocypack::Slice const&);

  /// @brief truncate a collections. Assumes no trx are running
  Result truncateCollection(arangodb::velocypack::Slice const&);

  /// @brief changes the properties of a collection,
  /// based on the VelocyPack provided
  Result changeView(arangodb::velocypack::Slice const&);

  /// @brief apply a single marker from the continuous log
  Result applyLogMarker(arangodb::velocypack::Slice const& slice,
                        ApplyStats& applyStats,
                        TRI_voc_tick_t firstRegularTick, TRI_voc_tick_t markerTick,
                        TRI_replication_operation_e type);

  /// @brief apply the data from the continuous log
  Result applyLog(httpclient::SimpleHttpResult*, TRI_voc_tick_t firstRegularTick,
                  ApplyStats& applyStats, uint64_t& ignoreCount);

  /// @brief perform a continuous sync with the master
  Result runContinuousSync();

  /// @brief fetch the open transactions we still need to complete
  Result fetchOpenTransactions(TRI_voc_tick_t fromTick, TRI_voc_tick_t toTick,
                               TRI_voc_tick_t& startTick);

  /// @brief save the current applier state
  virtual Result saveApplierState() = 0;

 private:
  /// @brief get local replication applier state
  void getLocalState();

  /// @brief run method, performs continuous synchronization
  /// internal method, may throw exceptions
  arangodb::Result runInternal();

  /// @brief fetch data for the continuous synchronization
  /// @param fetchTick tick from which we want results
  /// @param lastScannedTick tick which the server MAY start scanning from
  /// @param firstRegularTick if we got openTransactions server will return the
  ///                         only operations belonging to these for smaller
  ///                         ticks
  void fetchMasterLog(std::shared_ptr<Syncer::JobSynchronizer> sharedStatus,
                      TRI_voc_tick_t fetchTick, TRI_voc_tick_t lastScannedTick,
                      TRI_voc_tick_t firstRegularTick);

  /// @brief apply continuous synchronization data from a batch
  arangodb::Result processMasterLog(std::shared_ptr<Syncer::JobSynchronizer> sharedStatus,
                                    TRI_voc_tick_t& fetchTick, TRI_voc_tick_t& lastScannedTick,
                                    TRI_voc_tick_t firstRegularTick, uint64_t& ignoreCount,
                                    bool& worked, bool& mustFetchBatch);

  /// @brief determines if we can work in parallel on master and slave
  void checkParallel();

  arangodb::Result removeSingleDocument(arangodb::LogicalCollection* coll, std::string const& key);

  arangodb::Result handleRequiredFromPresentFailure(TRI_voc_tick_t fromTick,
                                                    TRI_voc_tick_t readTick,
                                                    char const* type);

 protected:
  virtual bool skipMarker(arangodb::velocypack::Slice const& slice) = 0;

  /// @brief pointer to the applier
  ReplicationApplier* _applier;

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

  /// @brief initial tick for continuous synchronization
  TRI_voc_tick_t _initialTick;

  /// @brief whether or not an operation modified the _users collection
  bool _usersModified;

  /// @brief database list with modified _analyzers collection
  std::set<TRI_vocbase_t*> _analyzersModified;

  /// @brief use the initial tick
  bool _useTick;

  /// @brief whether or not the specified from tick must be present when
  /// fetching
  /// data from a master
  bool _requireFromPresent;

  /// @brief ignore rename, create and drop operations for collections
  bool _ignoreRenameCreateDrop;

  /// @brief ignore create / drop database
  bool _ignoreDatabaseMarkers;

  /// @brief whether or not master & slave can work in parallel
  bool _workInParallel;

  /// @brief which transactions were open and need to be treated specially
  std::unordered_map<TRI_voc_tid_t, std::unique_ptr<ReplicationTransaction>> _ongoingTransactions;

  /// @brief recycled builder for repeated document creation
  arangodb::velocypack::Builder _documentBuilder;

  static std::string const WalAccessUrl;
};
}  // namespace arangodb

#endif
