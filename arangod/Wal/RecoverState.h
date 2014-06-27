////////////////////////////////////////////////////////////////////////////////
/// @brief Recovery state
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_WAL_RECOVER_STATE_H
#define ARANGODB_WAL_RECOVER_STATE_H 1

#include "Basics/Common.h"
#include "Utils/transactions.h"
#include "VocBase/datafile.h"
#include "VocBase/document-collection.h"
#include "VocBase/server.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"
#include "Wal/Logfile.h"
#include "Wal/Marker.h"
#include <functional>

struct TRI_server_s;

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut for multi-operation remote transaction
////////////////////////////////////////////////////////////////////////////////

#define RemoteTransactionType triagens::arango::ReplicationTransaction

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut for single-operation write transaction
////////////////////////////////////////////////////////////////////////////////

#define SingleWriteTransactionType triagens::arango::SingleCollectionWriteTransaction<triagens::arango::RestTransactionContext, 1>


namespace triagens {
  namespace wal {

// -----------------------------------------------------------------------------
// --SECTION--                                                      RecoverState
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief state that is built up when scanning a WAL logfile during recovery
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

    struct RecoverState {

      RecoverState (RecoverState const&) = delete;
      RecoverState& operator= (RecoverState const&) = delete;

////////////////////////////////////////////////////////////////////////////////
/// @brief creates the recover state
////////////////////////////////////////////////////////////////////////////////

      RecoverState (TRI_server_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the recover state
////////////////////////////////////////////////////////////////////////////////

      ~RecoverState ();

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not there are remote transactions
////////////////////////////////////////////////////////////////////////////////
      
      inline bool hasRunningRemoteTransactions () const {
        return ! runningRemoteTransactions.empty();
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the recovery procedure must be run
////////////////////////////////////////////////////////////////////////////////
      
      inline bool mustRecover () const {
        return ! logfilesToProcess.empty();
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not to ignore a specific transaction in replay
////////////////////////////////////////////////////////////////////////////////

      inline bool ignoreTransaction (TRI_voc_tid_t transactionId) const {
        return (transactionId > 0 && failedTransactions.find(transactionId) != failedTransactions.end());
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a transaction was started remotely
////////////////////////////////////////////////////////////////////////////////

      inline bool isRemoteTransaction (TRI_voc_tid_t transactionId) const {
        return (transactionId > 0 && remoteTransactions.find(transactionId) != remoteTransactions.end());
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief register a collection for a remote transaction
////////////////////////////////////////////////////////////////////////////////

      inline void registerRemoteUsage (TRI_voc_tick_t databaseId,
                                       TRI_voc_cid_t collectionId) {
        remoteTransactionDatabases.insert(databaseId);
        remoteTransactionCollections.insert(collectionId);
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a transaction was started remotely
////////////////////////////////////////////////////////////////////////////////

      inline bool isUsedByRemoteTransaction (TRI_voc_tid_t collectionId) const {
        return (remoteTransactionCollections.find(collectionId) != remoteTransactionCollections.end());
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief release opened collections and databases so they can be shut down
/// etc.
////////////////////////////////////////////////////////////////////////////////

      void releaseResources ();

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a database is dropped already
////////////////////////////////////////////////////////////////////////////////

      bool isDropped (TRI_voc_tick_t) const; 

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a database or collection is dropped already
////////////////////////////////////////////////////////////////////////////////

      bool isDropped (TRI_voc_tick_t, 
                      TRI_voc_cid_t) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief gets a database (and inserts it into the cache if not in it)
////////////////////////////////////////////////////////////////////////////////

      TRI_vocbase_t* useDatabase (TRI_voc_tick_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief gets a collection (and inserts it into the cache if not in it)
////////////////////////////////////////////////////////////////////////////////

      TRI_vocbase_col_t* useCollection (TRI_vocbase_t*,
                                        TRI_voc_cid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a collection
/// the collection will be opened after this call and inserted into a local
/// cache for faster lookups
/// returns nullptr if the collection does not exist
////////////////////////////////////////////////////////////////////////////////

      TRI_document_collection_t* getCollection (TRI_voc_tick_t,
                                                TRI_voc_cid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an operation in a remote transaction
////////////////////////////////////////////////////////////////////////////////

      int executeRemoteOperation (TRI_voc_tick_t,
                                  TRI_voc_cid_t,
                                  TRI_voc_tid_t,
                                  TRI_df_marker_t const*,
                                  TRI_voc_fid_t,
                                  std::function<int(RemoteTransactionType*, Marker*)>);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a single operation inside a transaction
////////////////////////////////////////////////////////////////////////////////

      int executeSingleOperation (TRI_voc_tick_t,
                                  TRI_voc_cid_t,
                                  TRI_df_marker_t const*,
                                  TRI_voc_fid_t,
                                  std::function<int(SingleWriteTransactionType*, Marker*)>);

////////////////////////////////////////////////////////////////////////////////
/// @brief callback to handle one marker during recovery
/// this function modifies indexes etc.
////////////////////////////////////////////////////////////////////////////////

      static bool ReplayMarker (TRI_df_marker_t const*,
                                void*,
                                TRI_datafile_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief callback to handle one marker during recovery
/// this function only builds up state and does not change any data
////////////////////////////////////////////////////////////////////////////////

      static bool InitialScanMarker (TRI_df_marker_t const*,
                                     void*,
                                     TRI_datafile_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief replay a single logfile
////////////////////////////////////////////////////////////////////////////////
    
      int replayLogfile (Logfile*);

////////////////////////////////////////////////////////////////////////////////
/// @brief replay all logfiles
////////////////////////////////////////////////////////////////////////////////
    
      int replayLogfiles ();

////////////////////////////////////////////////////////////////////////////////
/// @brief abort open transactions
////////////////////////////////////////////////////////////////////////////////

      int abortOpenTransactions ();

////////////////////////////////////////////////////////////////////////////////
/// @brief remove all empty logfiles found during logfile inspection
////////////////////////////////////////////////////////////////////////////////

      int removeEmptyLogfiles ();

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

      TRI_server_t*                                                               server;
      std::unordered_map<TRI_voc_cid_t, TRI_voc_tick_t>                           collections;
      std::unordered_map<TRI_voc_tid_t, std::pair<TRI_voc_tick_t, bool>>          failedTransactions;
      std::unordered_map<TRI_voc_tid_t, std::pair<TRI_voc_tick_t, TRI_voc_tid_t>> remoteTransactions;
      std::unordered_set<TRI_voc_cid_t>                                           remoteTransactionCollections;
      std::unordered_set<TRI_voc_tick_t>                                          remoteTransactionDatabases;
      std::unordered_set<TRI_voc_cid_t>                                           droppedCollections;
      std::unordered_set<TRI_voc_tick_t>                                          droppedDatabases;
      TRI_voc_tick_t                                                              lastTick;
      std::vector<Logfile*>                                                       logfilesToProcess;
      std::unordered_map<TRI_voc_cid_t, TRI_vocbase_col_t*>                       openedCollections;
      std::unordered_map<TRI_voc_tick_t, TRI_vocbase_t*>                          openedDatabases;
      std::unordered_map<TRI_voc_tid_t, RemoteTransactionType*>                   runningRemoteTransactions;
      std::vector<std::string>                                                    emptyLogfiles;

      TRI_doc_update_policy_t                                                     policy;
    };

  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
