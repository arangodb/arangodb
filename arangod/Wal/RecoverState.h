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

#ifndef ARANGOD_WAL_RECOVER_STATE_H
#define ARANGOD_WAL_RECOVER_STATE_H 1

#include "Basics/Common.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/datafile.h"
#include "VocBase/ticks.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"
#include "Wal/Logfile.h"
#include "Wal/Marker.h"

namespace arangodb {
class DatabaseFeature;

namespace wal {

/// @brief state that is built up when scanning a WAL logfile during recovery
struct RecoverState {
  RecoverState(RecoverState const&) = delete;
  RecoverState& operator=(RecoverState const&) = delete;

  /// @brief creates the recover state
  explicit RecoverState(bool);

  /// @brief destroys the recover state
  ~RecoverState();
  
  /// @brief checks if there will be a drop marker for the database or collection
  bool willBeDropped(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId) const {
    if (totalDroppedDatabases.find(databaseId) != totalDroppedDatabases.end()) {
      return true;
    }
    return (totalDroppedCollections.find(collectionId) != totalDroppedCollections.end());
  }

  /// @brief checks if there will be a drop marker for the collection
  bool willBeDropped(TRI_voc_cid_t collectionId) const {
    return (totalDroppedCollections.find(collectionId) != totalDroppedCollections.end());
  }

  /// @brief checks if a database is dropped already
  bool isDropped(TRI_voc_tick_t databaseId) const {
    return (droppedDatabases.find(databaseId) != droppedDatabases.end());
  }

  /// @brief checks if a database or collection is dropped already
  bool isDropped(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId) const {
    if (isDropped(databaseId)) {
      // database has been dropped
      return true;
    }

    if (droppedCollections.find(collectionId) != droppedCollections.end()) {
      // collection has been dropped
      return true;
    }

    return false;
  }

  /// @brief whether or not to abort recovery on first error
  inline bool canContinue() const { return ignoreRecoveryErrors; }

  /// @brief whether or not the recovery procedure must be run
  inline bool mustRecover() const { return !logfilesToProcess.empty(); }

  /// @brief whether or not to ignore a specific transaction in replay
  inline bool ignoreTransaction(TRI_voc_tid_t transactionId) const {
    return (transactionId > 0 &&
            failedTransactions.find(transactionId) != failedTransactions.end());
  }

  void resetCollection() {
    resetCollection(0, 0);
  }
  
  void resetCollection(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId) {
    lastDatabaseId = databaseId;
    lastCollectionId = collectionId;
  }

  /// @brief release opened collections and databases so they can be shut down
  /// etc.
  void releaseResources();

  /// @brief gets a database (and inserts it into the cache if not in it)
  TRI_vocbase_t* useDatabase(TRI_voc_tick_t);

  /// @brief releases a database
  TRI_vocbase_t* releaseDatabase(TRI_voc_tick_t);

  /// @brief release a collection (so it can be dropped)
  arangodb::LogicalCollection* releaseCollection(TRI_voc_cid_t);

  /// @brief gets a collection (and inserts it into the cache if not in it)
  arangodb::LogicalCollection* useCollection(TRI_vocbase_t*, TRI_voc_cid_t, int&);

  /// @brief looks up a collection
  /// the collection will be opened after this call and inserted into a local
  /// cache for faster lookups
  /// returns nullptr if the collection does not exist
  arangodb::LogicalCollection* getCollection(TRI_voc_tick_t, TRI_voc_cid_t);

  /// @brief executes a single operation inside a transaction
  int executeSingleOperation(
      TRI_voc_tick_t, TRI_voc_cid_t, TRI_df_marker_t const*, TRI_voc_fid_t,
      std::function<int(SingleCollectionTransaction*, MarkerEnvelope*)>);

  /// @brief callback to handle one marker during recovery
  /// this function modifies indexes etc.
  static bool ReplayMarker(TRI_df_marker_t const*, void*, TRI_datafile_t*);

  /// @brief callback to handle one marker during recovery
  /// this function only builds up state and does not change any data
  static bool InitialScanMarker(TRI_df_marker_t const*, void*, TRI_datafile_t*);

  /// @brief replay a single logfile
  int replayLogfile(Logfile*, int);

  /// @brief replay all logfiles
  int replayLogfiles();

  /// @brief abort open transactions
  int abortOpenTransactions();

  /// @brief remove all empty logfiles found during logfile inspection
  int removeEmptyLogfiles();

  /// @brief fill the secondary indexes of all collections used in recovery
  int fillIndexes();

  DatabaseFeature* databaseFeature;
  std::unordered_map<TRI_voc_tid_t, std::pair<TRI_voc_tick_t, bool>>
      failedTransactions;
  std::unordered_set<TRI_voc_cid_t> droppedCollections;
  std::unordered_set<TRI_voc_tick_t> droppedDatabases;
  std::unordered_set<TRI_voc_cid_t> totalDroppedCollections;
  std::unordered_set<TRI_voc_tick_t> totalDroppedDatabases;

  TRI_voc_tick_t lastTick;
  std::vector<Logfile*> logfilesToProcess;
  std::unordered_map<TRI_voc_cid_t, arangodb::LogicalCollection*> openedCollections;
  std::unordered_map<TRI_voc_tick_t, TRI_vocbase_t*> openedDatabases;
  std::vector<std::string> emptyLogfiles;

  bool ignoreRecoveryErrors;
  int64_t errorCount;

 private:
  TRI_voc_tick_t lastDatabaseId;
  TRI_voc_cid_t lastCollectionId;

};
}
}

#endif
