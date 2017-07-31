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

#ifndef ARANGOD_REPLICATION_INITIAL_SYNCER_H
#define ARANGOD_REPLICATION_INITIAL_SYNCER_H 1

#include "Basics/Common.h"
#include "Basics/StaticStrings.h"
#include "Logger/Logger.h"
#include "Replication/Syncer.h"
#include "Utils/SingleCollectionTransaction.h"

#include <velocypack/Slice.h>

class TRI_replication_applier_configuration_t;
struct TRI_vocbase_t;

namespace arangodb {

class LogicalCollection;
class InitialSyncer;

int handleSyncKeysMMFiles(arangodb::InitialSyncer& syncer,
                          arangodb::LogicalCollection* col,
                          std::string const& keysId, std::string const& cid,
                          std::string const& collectionName,
                          TRI_voc_tick_t maxTick, std::string& errorMsg);

int handleSyncKeysRocksDB(InitialSyncer& syncer,
                          arangodb::LogicalCollection* col,
                          std::string const& keysId, std::string const& cid,
                          std::string const& collectionName,
                          TRI_voc_tick_t maxTick, std::string& errorMsg);

int syncChunkRocksDB(InitialSyncer& syncer, SingleCollectionTransaction* trx,
                     std::string const& keysId, uint64_t chunkId,
                     std::string const& lowString,
                     std::string const& highString,
                     std::vector<std::pair<std::string, uint64_t>> const& markers,
                     std::string& errorMsg);
namespace httpclient {
class SimpleHttpResult;
}

class InitialSyncer : public Syncer {
  friend int ::arangodb::handleSyncKeysMMFiles(
      arangodb::InitialSyncer& syncer, arangodb::LogicalCollection* col,
      std::string const& keysId, std::string const& cid,
      std::string const& collectionName, TRI_voc_tick_t maxTick,
      std::string& errorMsg);

  friend int ::arangodb::handleSyncKeysRocksDB(
      InitialSyncer& syncer, arangodb::LogicalCollection* col,
      std::string const& keysId, std::string const& cid,
      std::string const& collectionName, TRI_voc_tick_t maxTick,
      std::string& errorMsg);

  friend int syncChunkRocksDB(InitialSyncer& syncer, SingleCollectionTransaction* trx,
                     std::string const& keysId, uint64_t chunkId,
                     std::string const& lowString,
                     std::string const& highString,
                     std::vector<std::pair<std::string, uint64_t>> const& markers,
                     std::string& errorMsg);

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief apply phases
  //////////////////////////////////////////////////////////////////////////////

  typedef enum {
    PHASE_NONE,
    PHASE_INIT,
    PHASE_VALIDATE,
    PHASE_DROP_CREATE,
    PHASE_DUMP
  } sync_phase_e;

 public:
  InitialSyncer(TRI_vocbase_t*, TRI_replication_applier_configuration_t const*,
                std::unordered_map<std::string, bool> const&,
                std::string const&, bool verbose, bool skipCreateDrop);

  ~InitialSyncer();

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief run method, performs a full synchronization
  //////////////////////////////////////////////////////////////////////////////

  int run(std::string&, bool);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the last log tick of the master at start
  //////////////////////////////////////////////////////////////////////////////

  TRI_voc_tick_t getLastLogTick() const { return _masterInfo._lastLogTick; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief translate a phase to a phase name
  //////////////////////////////////////////////////////////////////////////////

  std::string translatePhase(sync_phase_e phase) const {
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

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the collections that were synced
  //////////////////////////////////////////////////////////////////////////////

  std::map<TRI_voc_cid_t, std::string> const& getProcessedCollections() const {
    return _processedCollections;
  }

  std::string progress() { return _progress; }

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief set a progress message
  //////////////////////////////////////////////////////////////////////////////

  void setProgress(std::string const& msg) {
    _progress = msg;

    if (_verbose) {
      LOG_TOPIC(INFO, Logger::REPLICATION) << msg;
    } else {
      LOG_TOPIC(DEBUG, Logger::REPLICATION) << msg;
    }

    if (_vocbase->replicationApplier() != nullptr) {
      _vocbase->replicationApplier()->setProgress(msg.c_str(), true);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief send a WAL flush command
  //////////////////////////////////////////////////////////////////////////////

  int sendFlush(std::string&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief send a "start batch" command
  //////////////////////////////////////////////////////////////////////////////

  int sendStartBatch(std::string&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief send an "extend batch" command
  //////////////////////////////////////////////////////////////////////////////

  int sendExtendBatch();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief send a "finish batch" command
  //////////////////////////////////////////////////////////////////////////////

  int sendFinishBatch();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief check whether the initial synchronization should be aborted
  //////////////////////////////////////////////////////////////////////////////

  bool checkAborted();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief apply the data from a collection dump
  //////////////////////////////////////////////////////////////////////////////

  int applyCollectionDump(transaction::Methods&, std::string const&,
                          httpclient::SimpleHttpResult*, uint64_t&,
                          std::string&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief determine the number of documents in a collection
  //////////////////////////////////////////////////////////////////////////////

  int64_t getSize(arangodb::LogicalCollection*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief incrementally fetch data from a collection
  //////////////////////////////////////////////////////////////////////////////

  int handleCollectionDump(arangodb::LogicalCollection*, std::string const&,
                           std::string const&, TRI_voc_tick_t, std::string&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief incrementally fetch data from a collection
  //////////////////////////////////////////////////////////////////////////////

  int handleCollectionSync(arangodb::LogicalCollection*, std::string const&,
                           std::string const&, TRI_voc_tick_t, std::string&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief changes the properties of a collection, based on the VelocyPack
  /// provided
  //////////////////////////////////////////////////////////////////////////////

  int changeCollection(arangodb::LogicalCollection*,
                       arangodb::velocypack::Slice const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief handle the information about a collection
  //////////////////////////////////////////////////////////////////////////////

  int handleCollection(arangodb::velocypack::Slice const&,
                       arangodb::velocypack::Slice const&, bool, std::string&,
                       sync_phase_e);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief handle the inventory response of the master
  //////////////////////////////////////////////////////////////////////////////

  int handleInventoryResponse(arangodb::velocypack::Slice const&, bool,
                              std::string&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief iterate over all collections from an array and apply an action
  //////////////////////////////////////////////////////////////////////////////

  int iterateCollections(
      std::vector<std::pair<arangodb::velocypack::Slice,
                            arangodb::velocypack::Slice>> const&,
      bool, std::string&, sync_phase_e);

  std::unordered_map<std::string, std::string> createHeaders() {
    return { {StaticStrings::ClusterCommSource, ServerState::instance()->getId()} };
  }

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief progress message
  //////////////////////////////////////////////////////////////////////////////

  std::string _progress;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief collection restriction
  //////////////////////////////////////////////////////////////////////////////

  std::unordered_map<std::string, bool> _restrictCollections;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief collection restriction type
  //////////////////////////////////////////////////////////////////////////////

  std::string const _restrictType;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief collections synced
  //////////////////////////////////////////////////////////////////////////////

  std::map<TRI_voc_cid_t, std::string> _processedCollections;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief dump batch id
  //////////////////////////////////////////////////////////////////////////////

  uint64_t _batchId;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief dump batch last update time
  //////////////////////////////////////////////////////////////////////////////

  double _batchUpdateTime;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ttl for batches
  //////////////////////////////////////////////////////////////////////////////

  int _batchTtl;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief include system collections in the dump?
  //////////////////////////////////////////////////////////////////////////////

  bool _includeSystem;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief chunk size to use
  //////////////////////////////////////////////////////////////////////////////

  uint64_t _chunkSize;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief verbosity
  //////////////////////////////////////////////////////////////////////////////

  bool _verbose;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the WAL on the remote server has been flushed by us
  //////////////////////////////////////////////////////////////////////////////

  bool _hasFlushed;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief maximum internal value for chunkSize
  //////////////////////////////////////////////////////////////////////////////

  static size_t const MaxChunkSize;

  // in the cluster case it is a total NOGO to create or drop collections
  // because this HAS to be handled in the schmutz. otherwise it forgets who
  // the leader was etc.
  bool _skipCreateDrop;

};
}

#endif
