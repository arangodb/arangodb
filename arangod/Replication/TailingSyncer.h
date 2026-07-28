////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Replication/ReplicationApplierConfiguration.h"
#include "Replication/ReplicationMetricsFeature.h"
#include "Replication/Syncer.h"
#include "VocBase/Identifiers/TransactionId.h"

#include <velocypack/Builder.h>

namespace arangodb {
struct Database;
class InitialSyncer;
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
  TailingSyncer(ReplicationApplierConfiguration const&,
                TRI_voc_tick_t initialTick, bool useTick);

  virtual ~TailingSyncer();

 protected:
  /// @brief decide based on _leaderInfo which api to use
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
  bool skipMarker(TRI_voc_tick_t firstRegulaTick,
                  arangodb::velocypack::Slice slice,
                  TRI_voc_tick_t actualMarkerTick,
                  TRI_replication_operation_e type);

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
                        ApplyStats& applyStats, TRI_voc_tick_t firstRegularTick,
                        TRI_voc_tick_t markerTick,
                        TRI_replication_operation_e type);

  /// @brief apply the data from the continuous log
  Result applyLog(httpclient::SimpleHttpResult*,
                  TRI_voc_tick_t firstRegularTick, ApplyStats& applyStats,
                  arangodb::velocypack::Builder& builder,
                  uint64_t& ignoreCount);

 private:
  arangodb::Result removeSingleDocument(arangodb::LogicalCollection* coll,
                                        std::string const& key);

 protected:
  virtual bool skipMarker(arangodb::velocypack::Slice slice) = 0;

  /// @brief initial tick for continuous synchronization
  TRI_voc_tick_t _initialTick;

  /// @brief whether or not an operation modified the _users collection
  bool _usersModified;

  /// @brief database list with modified _analyzers collection
  std::set<Database*> _analyzersModified;

  /// @brief ignore rename, create and drop operations for collections
  bool _ignoreRenameCreateDrop;

  /// @brief ignore create / drop database
  bool _ignoreDatabaseMarkers;

  /// @brief statistics for tailing syncer
  ReplicationMetricsFeature::TailingSyncStats _stats;

  /// @brief which transactions were open and need to be treated specially
  std::unordered_map<TransactionId, std::unique_ptr<ReplicationTransaction>>
      _ongoingTransactions;

  /// @brief recycled builder for repeated document creation
  arangodb::velocypack::Builder _documentBuilder;

  static std::string const WalAccessUrl;
};
}  // namespace arangodb
