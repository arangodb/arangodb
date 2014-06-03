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
#include "BasicsC/hashes.h"
#include "BasicsC/logging.h"
#include "Basics/ConditionLocker.h"
#include "Utils/DatabaseGuard.h"
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
  triagens::arango::DatabaseGuard guard(_server, databaseId);

  TRI_vocbase_t* vocbase = guard.database();

  if (vocbase == nullptr) {
    return TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
  }

  TRI_vocbase_col_t* collection = TRI_UseCollectionByIdVocBase(vocbase, collectionId);

  if (collection == nullptr) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  TRI_voc_tick_t minTransferTick = 0; // TODO: find the actual max tick of a collection
  for (auto it2 = operations.begin(); it2 != operations.end(); ++it2) {
    TRI_df_marker_t const* source = (*it2);

    if (source->_tick <= minTransferTick) {
      // we have already transferred this marker in a previous run, nothing to do
      continue;
    }

    char const* base = reinterpret_cast<char const*>(source);

    switch (source->_type) {
      case TRI_WAL_MARKER_ATTRIBUTE: {
        char const* name = base + sizeof(attribute_marker_t);
        size_t n = strlen(name) + 1; // add NULL byte
        TRI_voc_size_t const totalSize = sizeof(TRI_df_attribute_marker_t) + n;

        char* mem = nextFreeMarkerPosition(TRI_DF_MARKER_ATTRIBUTE, totalSize);

        if (mem == nullptr) {
          return TRI_ERROR_OUT_OF_MEMORY;
        }

        // set attribute id
        TRI_df_attribute_marker_t* m = reinterpret_cast<TRI_df_attribute_marker_t*>(mem);
        m->_aid = reinterpret_cast<attribute_marker_t const*>(source)->_attributeId;
 
        // copy attribute name into marker
        memcpy(mem + sizeof(TRI_df_attribute_marker_t), name, n);

        finishMarker(mem, source->_tick);
        break;
      }

      case TRI_WAL_MARKER_SHAPE: {
        TRI_shape_t const* shape = reinterpret_cast<TRI_shape_t const*>(base + sizeof(shape_marker_t));
        TRI_voc_size_t const totalSize = sizeof(TRI_df_shape_marker_t) + shape->_size;

        char* mem = nextFreeMarkerPosition(TRI_DF_MARKER_SHAPE, totalSize);

        if (mem == nullptr) {
          return TRI_ERROR_OUT_OF_MEMORY;
        }

        // copy shape into marker
        memcpy(mem + sizeof(TRI_df_shape_marker_t), shape, shape->_size);

        finishMarker(mem, source->_tick);
        break;
      }

      case TRI_WAL_MARKER_DOCUMENT: {
        document_marker_t const* orig = reinterpret_cast<document_marker_t const*>(source);

        TRI_shape_t const* shape = reinterpret_cast<TRI_shape_t const*>(base + orig->_offsetJson);
        char const* key = base + orig->_offsetKey;
        size_t n = strlen(key) + 1; // add NULL byte
        TRI_voc_size_t const totalSize = sizeof(TRI_doc_document_key_marker_t) +
                                         TRI_DF_ALIGN_BLOCK(n) + 
                                         shape->_size;

        char* mem = nextFreeMarkerPosition(TRI_DOC_MARKER_KEY_DOCUMENT, totalSize);

        if (mem == nullptr) {
          return TRI_ERROR_OUT_OF_MEMORY;
        }

        TRI_doc_document_key_marker_t* m = reinterpret_cast<TRI_doc_document_key_marker_t*>(mem);
        m->_rid        = orig->_revisionId;
        m->_tid        = orig->_transactionId;
        m->_shape      = orig->_shape;
        m->_offsetKey  = sizeof(TRI_doc_document_key_marker_t);
        m->_offsetJson = m->_offsetKey + TRI_DF_ALIGN_BLOCK(n);

        // copy key into marker
        memcpy(mem + m->_offsetKey, key, n);
 
        // copy shape into marker
        memcpy(mem + m->_offsetJson, shape, shape->_size);

        finishMarker(mem, source->_tick);
        break;
      }

      case TRI_WAL_MARKER_EDGE: {
        edge_marker_t const* orig = reinterpret_cast<edge_marker_t const*>(source);

        TRI_shape_t const* shape = reinterpret_cast<TRI_shape_t const*>(base + orig->_offsetJson);
        char const* key = base + orig->_offsetKey;
        size_t n = strlen(key) + 1; // add NULL byte
        char const* toKey = base + orig->_offsetToKey;
        size_t to = strlen(toKey) + 1; // add NULL byte
        char const* fromKey = base + orig->_offsetFromKey;
        size_t from = strlen(fromKey) + 1; // add NULL byte
        TRI_voc_size_t const totalSize = sizeof(TRI_doc_edge_key_marker_t) + 
                                         TRI_DF_ALIGN_BLOCK(n) + 
                                         TRI_DF_ALIGN_BLOCK(to) + 
                                         TRI_DF_ALIGN_BLOCK(from) + 
                                         shape->_size;

        char* mem = nextFreeMarkerPosition(TRI_DOC_MARKER_KEY_EDGE, totalSize);

        if (mem == nullptr) {
          return TRI_ERROR_OUT_OF_MEMORY;
        }

        size_t offsetKey = sizeof(TRI_doc_edge_key_marker_t);
        TRI_doc_edge_key_marker_t* m = reinterpret_cast<TRI_doc_edge_key_marker_t*>(mem);
        m->base._rid           = orig->_revisionId;
        m->base._tid           = orig->_transactionId;
        m->base._shape         = orig->_shape;
        m->base._offsetKey     = offsetKey;
        m->base._offsetJson    = offsetKey + TRI_DF_ALIGN_BLOCK(n) + TRI_DF_ALIGN_BLOCK(to) + TRI_DF_ALIGN_BLOCK(from);
        m->_toCid              = orig->_toCid;
        m->_fromCid            = orig->_fromCid;
        m->_offsetToKey        = offsetKey + TRI_DF_ALIGN_BLOCK(n);
        m->_offsetFromKey      = offsetKey + TRI_DF_ALIGN_BLOCK(n) + TRI_DF_ALIGN_BLOCK(to);

        // copy key into marker
        memcpy(mem + offsetKey, key, n);
        memcpy(mem + m->_offsetToKey, toKey, to);
        memcpy(mem + m->_offsetFromKey, fromKey, from);
 
        // copy shape into marker
        memcpy(mem + m->base._offsetJson, shape, shape->_size);

        finishMarker(mem, source->_tick);
        break;
      }

      case TRI_WAL_MARKER_REMOVE: {
        remove_marker_t const* orig = reinterpret_cast<remove_marker_t const*>(source);

        char const* key = base + sizeof(remove_marker_t);
        size_t n = strlen(key) + 1; // add NULL byte
        TRI_voc_size_t const totalSize = sizeof(TRI_doc_deletion_key_marker_t) + n;

        char* mem = nextFreeMarkerPosition(TRI_DOC_MARKER_KEY_DELETION, totalSize);

        if (mem == nullptr) {
          return TRI_ERROR_OUT_OF_MEMORY;
        }


        TRI_doc_deletion_key_marker_t* m = reinterpret_cast<TRI_doc_deletion_key_marker_t*>(mem);
        m->_rid       = orig->_revisionId;
        m->_tid       = orig->_transactionId;
        m->_offsetKey = sizeof(TRI_doc_deletion_key_marker_t);

        // copy key into marker
        memcpy(mem + m->_offsetKey, key, n);

        finishMarker(mem, source->_tick);
        break;
      }
    }
  }

  TRI_ReleaseCollectionVocBase(vocbase, collection);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the next position for a marker of the specified size
////////////////////////////////////////////////////////////////////////////////

char* CollectorThread::nextFreeMarkerPosition (TRI_df_marker_type_e type,
                                               TRI_voc_size_t size) {
  char* mem = nullptr;

  if (mem != nullptr) {
    initMarker(reinterpret_cast<TRI_df_marker_t*>(mem), type, size);
  }

  return mem;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise a marker
////////////////////////////////////////////////////////////////////////////////

void CollectorThread::initMarker (TRI_df_marker_t* marker,
                                  TRI_df_marker_type_e type,
                                  TRI_voc_size_t size) {
  assert(marker != nullptr);

  marker->_size = size; 
  marker->_type = (TRI_df_marker_type_t) type;
  marker->_crc  = 0;
  marker->_tick = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the tick of a marker and calculate its CRC value
////////////////////////////////////////////////////////////////////////////////

void CollectorThread::finishMarker (char* mem,
                                    TRI_voc_tick_t tick) {
  TRI_df_marker_t* marker = reinterpret_cast<TRI_df_marker_t*>(mem);
  
  marker->_tick = tick;

  // calculate the CRC
  TRI_voc_crc_t crc = TRI_InitialCrc32();
  crc = TRI_BlockCrc32(crc, (char const*) marker, marker->_size);
  marker->_crc = TRI_FinalCrc32(crc);
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
