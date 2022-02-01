////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "Basics/Result.h"
#include "Cluster/ServerState.h"
#include "Containers/MerkleTree.h"
#include "Replication/InitialSyncer.h"
#include "Replication/ReplicationMetricsFeature.h"
#include "Replication/utilities.h"
#include "Utils/SingleCollectionTransaction.h"

#include <chrono>
#include <memory>

struct TRI_vocbase_t;

namespace arangodb {

class LogicalCollection;
class DatabaseInitialSyncer;
class ReplicationApplierConfiguration;

class DatabaseInitialSyncer : public InitialSyncer {
  friend ::arangodb::Result handleSyncKeysRocksDB(
      DatabaseInitialSyncer& syncer, arangodb::LogicalCollection* col,
      std::string const& keysId);
  friend ::arangodb::Result syncChunkRocksDB(
      DatabaseInitialSyncer& syncer, SingleCollectionTransaction* trx,
      ReplicationMetricsFeature::InitialSyncStats& stats,
      std::string const& keysId, uint64_t chunkId, std::string const& lowString,
      std::string const& highString, std::vector<std::string> const& markers);

 private:
  // constructor is private, as DatabaseInitialSyncer uses shared_from_this()
  // and we must ensure that it is only created via make_shared.
  DatabaseInitialSyncer(TRI_vocbase_t& vocbase,
                        ReplicationApplierConfiguration const& configuration);

 public:
  static std::shared_ptr<DatabaseInitialSyncer> create(
      TRI_vocbase_t& vocbase,
      ReplicationApplierConfiguration const& configuration);

  /// @brief apply phases
  typedef enum {
    PHASE_NONE,
    PHASE_INIT,
    PHASE_VALIDATE,
    PHASE_DROP_CREATE,
    PHASE_DUMP
  } SyncPhase;

  struct Configuration {
    /// @brief replication applier config (from the base Syncer)
    ReplicationApplierConfiguration const& applier;
    /// @brief the dump batch state (from the base InitialSyncer)
    replutils::BatchInfo& batch;  // TODO worker safety
    /// @brief the client connection (from the base Syncer)
    replutils::Connection& connection;
    /// @brief whether or not we have flushed the WAL on the remote server
    bool flushed;  // TODO worker safety
    /// @brief info about leader node (from the base Syncer)
    replutils::LeaderInfo& leader;
    /// @brief the progress info (from the base InitialSyncer)
    replutils::ProgressInfo& progress;  // TODO worker safety
    /// @brief the syncer state (from the base Syncer)
    SyncerState& state;
    /// @brief the database to dump
    TRI_vocbase_t& vocbase;

    explicit Configuration(ReplicationApplierConfiguration const&,
                           replutils::BatchInfo&, replutils::Connection&, bool,
                           replutils::LeaderInfo&, replutils::ProgressInfo&,
                           SyncerState& state, TRI_vocbase_t&);

    bool isChild() const noexcept;  // TODO worker safety
  };

  /// @brief run method, performs a full synchronization
  Result run(bool incremental, char const* context = nullptr) override {
    return runWithInventory(incremental, velocypack::Slice::noneSlice(),
                            context);
  }

  /// @brief run method, performs a full synchronization with the
  ///        given list of collections.
  Result runWithInventory(bool incremental, velocypack::Slice collections,
                          char const* context = nullptr);

  TRI_vocbase_t* resolveVocbase(velocypack::Slice const& slice) override {
    return &_config.vocbase;
  }

  /// @brief translate a phase to a phase name
  std::string translatePhase(SyncPhase phase) const {
    switch (phase) {
      case PHASE_INIT:
        return "init";
      case PHASE_VALIDATE:
        return "validate";
      case PHASE_DROP_CREATE:
        return "drop-create";
      case PHASE_DUMP:
        return "dump";
      case PHASE_NONE:
        break;
    }
    return "none";
  }

  // TODO worker safety
  TRI_vocbase_t& vocbase() const {
    TRI_ASSERT(vocbases().size() == 1);
    return vocbases().begin()->second.database();
  }

  /// @brief check whether the initial synchronization should be aborted
  // TODO worker-safety
  bool isAborted() const override;

  /// @brief insert the batch ID for use in globalinitialsyncer
  // TODO worker safety
  void useAsChildSyncer(replutils::LeaderInfo const& info,
                        SyncerId const syncerId, uint64_t batchId,
                        double batchUpdateTime) {
    _state.syncerId = syncerId;
    _state.isChildSyncer = true;
    _state.leader = info;
    _config.batch.id = batchId;
    _config.batch.updateTime = batchUpdateTime;
  }

  /// @brief last time the batch was extended or created
  /// The batch prevents compaction in mmfiles and keeps a snapshot
  /// in rocksdb for a constant view of the data
  double batchUpdateTime() const { return _config.batch.updateTime; }

  /// @brief fetch the server's inventory, public method
  Result getInventory(arangodb::velocypack::Builder& builder);

  /// @brief return information about the leader
  replutils::LeaderInfo leaderInfo() const;

  void setOnSuccessCallback(
      std::function<Result(DatabaseInitialSyncer&)> const& cb) {
    _onSuccess = cb;
  }

  void setAbortionCheckCallback(std::function<bool()> const& cb) {
    _checkAbortion = cb;
  }

 private:
  enum class FormatHint {
    // format must still be detected. this is used on the first call
    AutoDetect,
    // enveloped format. all documents are wrapped into a
    // {"type":2300,"data":{...}} envelope
    Envelope,
    // raw documents, i.e. no envelope
    NoEnvelope,
  };

  /// @brief order a new chunk from the /dump API
  void fetchDumpChunk(std::shared_ptr<Syncer::JobSynchronizer> sharedStatus,
                      std::string const& baseUrl,
                      arangodb::LogicalCollection* coll,
                      std::string const& leaderColl, int batch,
                      TRI_voc_tick_t fromTick, uint64_t chunkSize);

  /// @brief fetch the server's inventory
  Result fetchInventory(arangodb::velocypack::Builder& builder);

  /// @brief set a progress message
  void setProgress(std::string const& msg);

  /// @brief handle a single dump marker
  // TODO worker-safety
  Result parseCollectionDumpMarker(transaction::Methods&,
                                   arangodb::LogicalCollection*,
                                   arangodb::velocypack::Slice,
                                   FormatHint& hint);

  /// @brief apply the data from a collection dump
  // TODO worker-safety
  Result parseCollectionDump(transaction::Methods&, LogicalCollection* col,
                             httpclient::SimpleHttpResult*, uint64_t&);

  /// @brief whether or not the collection has documents
  bool hasDocuments(arangodb::LogicalCollection const& col);

  /// @brief incrementally fetch data from a collection
  // TODO worker safety
  Result fetchCollectionDump(arangodb::LogicalCollection*,
                             std::string const& leaderColl, TRI_voc_tick_t);

  /// @brief incrementally fetch data from a collection using keys as the
  /// primary document identifier
  // TODO worker safety
  Result fetchCollectionSync(arangodb::LogicalCollection*,
                             std::string const& leaderColl, TRI_voc_tick_t);

  /// @brief incrementally fetch data from a collection using keys as the
  /// primary document identifier
  // TODO worker safety
  Result fetchCollectionSyncByKeys(arangodb::LogicalCollection*,
                                   std::string const& leaderColl,
                                   TRI_voc_tick_t);

  /// @brief incrementally fetch data from a collection using revisions as the
  /// primary document identifier, not supported by all engines/collections
  // TODO worker safety
  Result fetchCollectionSyncByRevisions(arangodb::LogicalCollection*,
                                        std::string const& leaderColl,
                                        TRI_voc_tick_t);

  /// @brief changes the properties of a collection, based on the VelocyPack
  /// provided
  Result changeCollection(arangodb::LogicalCollection*,
                          arangodb::velocypack::Slice const&);

  /// @brief handle the information about a collection
  // TODO worker-safety
  Result handleCollection(arangodb::velocypack::Slice const&,
                          arangodb::velocypack::Slice const&, bool incremental,
                          SyncPhase);

  /// @brief handle the inventory response of the leader
  Result handleCollectionsAndViews(arangodb::velocypack::Slice const& colls,
                                   arangodb::velocypack::Slice const& views,
                                   bool incremental);

  /// @brief iterate over all collections from an array and apply an action
  Result iterateCollections(
      std::vector<std::pair<arangodb::velocypack::Slice,
                            arangodb::velocypack::Slice>> const&,
      bool incremental, SyncPhase);

  /// @brief create non-existing views locally
  Result handleViewCreation(VPackSlice const& views);

  /// @brief send a "start batch" command
  /// @param patchCount (optional)
  ///        Try to patch count of this collection (must be a collection name).
  ///        Only effective with the incremental sync.
  Result batchStart(char const* context, std::string const& patchCount = "");

  /// @brief send an "extend batch" command
  Result batchExtend();

  /// @brief send a "finish batch" command
  Result batchFinish() noexcept;

  Configuration _config;

  // custom callback executed when synchronization was completed successfully
  std::function<Result(DatabaseInitialSyncer&)> _onSuccess;

  // custom callback to check if the sync should be aborted
  std::function<bool()> _checkAbortion;

  // point in time when we last executed the _checkAbortion callback
  mutable std::chrono::steady_clock::time_point _lastAbortionCheck;

  /// @brief whether or not we are a coordinator/dbserver
  bool const _isClusterRole;
  uint64_t _quickKeysNumDocsLimit;

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  void adjustQuickKeysNumDocsLimit();
#endif
};

}  // namespace arangodb
