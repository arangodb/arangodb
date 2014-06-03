////////////////////////////////////////////////////////////////////////////////
/// @brief Write-ahead log garbage collection thread
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "CollectorThread.h"
#include "BasicsC/logging.h"
#include "Basics/ConditionLocker.h"
#include "VocBase/server.h"
#include "Wal/Logfile.h"
#include "Wal/LogfileManager.h"

using namespace triagens::wal;

// -----------------------------------------------------------------------------
// --SECTION--                                                    CollectorState
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief state that is built up when scanning a WAL logfile
////////////////////////////////////////////////////////////////////////////////

struct CollectorState {
  std::unordered_map<TRI_voc_cid_t, TRI_voc_tick_t>                           collections;
  std::unordered_map<TRI_voc_cid_t, CollectorThread::OperationsType>          structuralOperations;
  std::unordered_map<TRI_voc_cid_t, CollectorThread::DocumentOperationsType>  documentOperations;
  std::unordered_set<TRI_voc_tid_t>                                           failedTransactions;
  std::unordered_set<TRI_voc_tid_t>                                           handledTransactions;
};

// -----------------------------------------------------------------------------
// --SECTION--                                             class CollectorThread
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief wait interval for the collector thread when idle
////////////////////////////////////////////////////////////////////////////////

uint64_t const CollectorThread::Interval = 1000000;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the collector thread
////////////////////////////////////////////////////////////////////////////////

CollectorThread::CollectorThread (LogfileManager* logfileManager,
                                  TRI_server_t* server) 
  : Thread("WalCollector"),
    _logfileManager(logfileManager),
    _server(server),
    _condition(),
    _stop(0) {
  
  allowAsynchronousCancelation();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the collector thread
////////////////////////////////////////////////////////////////////////////////

CollectorThread::~CollectorThread () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the collector thread
////////////////////////////////////////////////////////////////////////////////

void CollectorThread::stop () {
  if (_stop > 0) {
    return;
  }

  _stop = 1;
  _condition.signal();

  while (_stop != 2) {
    usleep(1000);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief signal the thread that there is something to do
////////////////////////////////////////////////////////////////////////////////

void CollectorThread::signal () {
  CONDITION_LOCKER(guard, _condition);
  guard.signal();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    Thread methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief main loop
////////////////////////////////////////////////////////////////////////////////

void CollectorThread::run () {
  while (_stop == 0) {
    // collect a logfile if any qualifies
    bool worked = this->collectLogfiles();

    // delete a logfile if any qualifies
    worked |= this->removeLogfiles();
    
    CONDITION_LOCKER(guard, _condition);
    if (! worked) {
      // sleep only if there was nothing to do
      guard.wait(Interval);
    }
  }

  _stop = 2;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief perform collection of a logfile (if any)
////////////////////////////////////////////////////////////////////////////////

bool CollectorThread::collectLogfiles () {
  Logfile* logfile = _logfileManager->getCollectableLogfile();

  if (logfile == nullptr) {
    return false;
  }

  _logfileManager->setCollectionRequested(logfile);

  try {
    int res = collect(logfile);

    if (res == TRI_ERROR_NO_ERROR) {
      _logfileManager->setCollectionDone(logfile);
      return true;
    }
  }
  catch (...) {
    // collection failed
    LOG_ERROR("logfile collection failed");
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform removal of a logfile (if any)
////////////////////////////////////////////////////////////////////////////////

bool CollectorThread::removeLogfiles () {
  Logfile* logfile = _logfileManager->getRemovableLogfile();

  if (logfile == nullptr) {
    return false;
  }

  _logfileManager->removeLogfile(logfile, true);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief callback to handle one marker during collection
////////////////////////////////////////////////////////////////////////////////

bool CollectorThread::ScanMarker (TRI_df_marker_t const* marker, 
                                  void* data, 
                                  TRI_datafile_t* datafile, 
                                  bool) {
  CollectorState* state = reinterpret_cast<CollectorState*>(data);

  // std::cout << "MARKER: " << TRI_NameMarkerDatafile(marker) << "\n";
  assert(marker != nullptr);

  switch (marker->_type) {
    case TRI_WAL_MARKER_ATTRIBUTE: {
      attribute_marker_t const* m = reinterpret_cast<attribute_marker_t const*>(marker);
      TRI_voc_cid_t collectionId = m->_collectionId;
      TRI_voc_tick_t databaseId = m->_databaseId;

      state->collections[collectionId] = databaseId;
      
      // fill list of structural operations 
      state->structuralOperations[collectionId].push_back(marker);
      break;
    }

    case TRI_WAL_MARKER_SHAPE: {
      shape_marker_t const* m = reinterpret_cast<shape_marker_t const*>(marker);
      TRI_voc_cid_t collectionId = m->_collectionId;
      TRI_voc_tick_t databaseId = m->_databaseId;

      state->collections[collectionId] = databaseId;

      // fill list of structural operations 
      state->structuralOperations[collectionId].push_back(marker);
      break;
    }
      
    case TRI_WAL_MARKER_DOCUMENT: {
      document_marker_t const* m = reinterpret_cast<document_marker_t const*>(marker);
      TRI_voc_cid_t collectionId = m->_collectionId;
      TRI_voc_tid_t transactionId = m->_transactionId;

      if (state->failedTransactions.find(transactionId) != state->failedTransactions.end()) {
        // transaction had failed
        break;
      }

      char const* key = reinterpret_cast<char const*>(m) + m->_offsetKey;
      state->documentOperations[collectionId][std::string(key)] = marker;
      
      state->collections[collectionId] = m->_databaseId;
      break;
    }

    case TRI_WAL_MARKER_EDGE: {
      edge_marker_t const* m = reinterpret_cast<edge_marker_t const*>(marker);
      TRI_voc_cid_t collectionId = m->_collectionId;
      TRI_voc_tid_t transactionId = m->_transactionId;

      if (state->failedTransactions.find(transactionId) != state->failedTransactions.end()) {
        // transaction had failed
        break;
      }

      char const* key = reinterpret_cast<char const*>(m) + m->_offsetKey;
      state->documentOperations[collectionId][std::string(key)] = marker;
      state->collections[collectionId] = m->_databaseId;
      break;
    }

    case TRI_WAL_MARKER_REMOVE: {
      remove_marker_t const* m = reinterpret_cast<remove_marker_t const*>(marker);
      TRI_voc_cid_t collectionId = m->_collectionId;
      TRI_voc_tid_t transactionId = m->_transactionId;

      if (state->failedTransactions.find(transactionId) != state->failedTransactions.end()) {
        // transaction had failed
        break;
      }

      char const* key = reinterpret_cast<char const*>(m) + sizeof(remove_marker_t);
      state->documentOperations[collectionId][std::string(key)] = marker;
      state->collections[collectionId] = m->_databaseId;
      break;
    }

    case TRI_WAL_MARKER_BEGIN_TRANSACTION:
    case TRI_WAL_MARKER_COMMIT_TRANSACTION: {
      // do nothing
    }

    case TRI_WAL_MARKER_ABORT_TRANSACTION: {
      transaction_abort_marker_t const* m = reinterpret_cast<transaction_abort_marker_t const*>(marker);
      // note which abort markers we found
      state->handledTransactions.insert(m->_transactionId);
      break;
    }

    /* TODO: check whether these need to be handled
    case TRI_WAL_MARKER_CREATE_COLLECTION:
    case TRI_WAL_MARKER_DROP_COLLECTION:
    case TRI_WAL_MARKER_RENAME_COLLECTION:
    case TRI_WAL_MARKER_CHANGE_COLLECTION:
    */
 
    default: {
      // do nothing intentionally
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief collect one logfile
////////////////////////////////////////////////////////////////////////////////

int CollectorThread::collect (Logfile* logfile) {
  LOG_INFO("collecting logfile %llu", (unsigned long long) logfile->id());

  TRI_datafile_t* df = logfile->df();

  assert(df != nullptr);

  // create a state for the collector, beginning with the list of failed transactions 
  CollectorState state;
  state.failedTransactions = _logfileManager->getFailedTransactions();

  // scan all markers in logfile, this will fill the state
  bool result = TRI_IterateDatafile(df, &CollectorThread::ScanMarker, static_cast<void*>(&state), false, false);

  if (! result) {
    return TRI_ERROR_INTERNAL;
  }

  // get an aggregated list of all collection ids
  std::vector<TRI_voc_cid_t> collectionIds;
  for (auto it = state.structuralOperations.begin(); it != state.structuralOperations.end(); ++it) {
    collectionIds.push_back((*it).first);
  }

  for (auto it = state.documentOperations.begin(); it != state.documentOperations.end(); ++it) {
    auto cid = (*it).first;
    if (state.structuralOperations.find(cid) == state.structuralOperations.end()) {
      collectionIds.push_back(cid);
    }
  }

  // now for each collection, write all surviving markers into collection datafiles
  for (auto it = collectionIds.begin(); it != collectionIds.end(); ++it) {
    auto cid = (*it);
    
    OperationsType sortedOperations;

    // insert structural operations - those are already sorted by tick
    if (state.structuralOperations.find(cid) != state.structuralOperations.end()) {
      OperationsType const& ops = state.structuralOperations[cid];

      sortedOperations.insert(sortedOperations.begin(), ops.begin(), ops.end());
    }

    // insert document operations - those are sorted by key, not by tick
    if (state.documentOperations.find(cid) != state.documentOperations.end()) {
      DocumentOperationsType const& ops = state.documentOperations[cid];

      for (auto it2 = ops.begin(); it2 != ops.end(); ++it2) {
        sortedOperations.push_back((*it2).second);
      }

      // sort vector by marker tick
      std::sort(sortedOperations.begin(), sortedOperations.end(), [] (TRI_df_marker_t const* left, TRI_df_marker_t const* right) {
        return (left->_tick < right->_tick);
      });
    }

    transferMarkers(cid, state.collections[cid], sortedOperations);
  }


  // remove all handled transactions from failedTransactions list
  if (! state.handledTransactions.empty()) {
    _logfileManager->unregisterFailedTransactions(state.handledTransactions);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief transfer markers into a collection
////////////////////////////////////////////////////////////////////////////////

int CollectorThread::transferMarkers (TRI_voc_cid_t collectionId,
                                      TRI_voc_tick_t databaseId,
                                      OperationsType const& operations) {
  TRI_vocbase_t* vocbase = TRI_UseDatabaseByIdServer(_server, databaseId);

  if (vocbase == nullptr) {
    return TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
  }

  TRI_vocbase_col_t* collection = TRI_UseCollectionByIdVocBase(vocbase, collectionId);

  if (collection == nullptr) {
    TRI_ReleaseDatabaseServer(_server, vocbase);
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  TRI_voc_tick_t minTransferTick = 0; // TODO: find the actual max tick of a collection
  for (auto it2 = operations.begin(); it2 != operations.end(); ++it2) {
    TRI_df_marker_t const* m = (*it2);

    if (m->_tick <= minTransferTick) {
      // we have already transferred this marker in a previous run, nothing to do
      continue;
    }
  }

  TRI_ReleaseCollectionVocBase(vocbase, collection);
  TRI_ReleaseDatabaseServer(_server, vocbase);

  return TRI_ERROR_NO_ERROR;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
