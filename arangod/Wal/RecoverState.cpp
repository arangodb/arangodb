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

#include "RecoverState.h"
#include "Basics/FileUtils.h"
#include "Basics/conversions.h"
#include "Basics/files.h"
#include "VocBase/collection.h"
#include "VocBase/replication-applier.h"
#include "VocBase/voc-shaper.h"
#include "Wal/LogfileManager.h"
#include "Wal/Slots.h"
#include "Utils/Exception.h"

using namespace triagens::wal;

// -----------------------------------------------------------------------------
// --SECTION--                                                  helper functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a collection is volatile
////////////////////////////////////////////////////////////////////////////////

static inline bool IsVolatile (TRI_transaction_collection_t const* trxCollection) {
  return trxCollection->_collection->_collection->_info._isVolatile;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the directory for a database
////////////////////////////////////////////////////////////////////////////////

static std::string GetDatabaseDirectory (TRI_server_t* server,
                                         TRI_voc_tick_t databaseId) {
  char* idString = TRI_StringUInt64(databaseId);
  char* dname = TRI_Concatenate2String("database-", idString);
  TRI_FreeString(TRI_CORE_MEM_ZONE, idString);
  char* filename = TRI_Concatenate2File(server->_databasePath, dname);
  TRI_FreeString(TRI_CORE_MEM_ZONE, dname);

  std::string result(filename);
  TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the directory for a collection
////////////////////////////////////////////////////////////////////////////////
  
static std::string GetCollectionDirectory (TRI_vocbase_t* vocbase,
                                           TRI_voc_cid_t collectionId) {
  char* dirname = TRI_GetDirectoryCollection(vocbase->_path,
                                             "empty", // does not matter
                                             TRI_COL_TYPE_DOCUMENT, // does not matter 
                                             collectionId);

  std::string result(dirname);
  TRI_FreeString(TRI_CORE_MEM_ZONE, dirname);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wait until a database directory disappears
////////////////////////////////////////////////////////////////////////////////

static int WaitForDeletion (TRI_server_t* server,
                            TRI_voc_tick_t databaseId,
                            int statusCode) {
  std::string const result = GetDatabaseDirectory(server, databaseId);
 
  int iterations = 0;
  // wait for at most 30 seconds for the directory to be removed
  while (TRI_IsDirectory(result.c_str())) {
    if (iterations == 0) {
      LOG_TRACE("waiting for deletion of database directory '%s', called with status code %d", 
                result.c_str(), 
                statusCode);
      
      if (statusCode != TRI_ERROR_FORBIDDEN &&
          (statusCode == TRI_ERROR_ARANGO_DATABASE_NOT_FOUND ||
           statusCode != TRI_ERROR_NO_ERROR)) {
        LOG_WARNING("forcefully deleting database directory '%s'", result.c_str());
        TRI_RemoveDirectory(result.c_str());
      }
    }
    else if (iterations >= 30 * 10) {
      LOG_WARNING("unable to remove database directory '%s'", result.c_str());
      return TRI_ERROR_INTERNAL;
    }

    if (iterations == 5 * 10) {
      LOG_INFO("waiting for deletion of database directory '%s'", result.c_str());
    }

    ++iterations;
    usleep(100000);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wait until a collection directory disappears
////////////////////////////////////////////////////////////////////////////////

static int WaitForDeletion (TRI_vocbase_t* vocbase,
                            TRI_voc_cid_t collectionId,
                            int statusCode) {
  std::string const result = GetCollectionDirectory(vocbase, collectionId);

  int iterations = 0;
  // wait for at most 30 seconds for the directory to be removed
  while (TRI_IsDirectory(result.c_str())) {
    if (iterations == 0) {
      LOG_TRACE("waiting for deletion of collection directory '%s', called with status code %d", 
                result.c_str(),
                statusCode);
      
      if (statusCode != TRI_ERROR_FORBIDDEN && 
          (statusCode == TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND ||
           statusCode != TRI_ERROR_NO_ERROR)) {
        LOG_WARNING("forcefully deleting collection directory '%s'", result.c_str());
        TRI_RemoveDirectory(result.c_str());
      }
    }
    else if (iterations >= 30 * 10) {
      LOG_WARNING("unable to remove collection directory '%s'", result.c_str());
      return TRI_ERROR_INTERNAL;
    }
    
    if (iterations == 5 * 10) {
      LOG_INFO("waiting for deletion of collection directory '%s'", result.c_str());
    }

    ++iterations;
    usleep(100000);
  }

  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates the recover state
////////////////////////////////////////////////////////////////////////////////

RecoverState::RecoverState (TRI_server_t* server,
                            bool ignoreRecoveryErrors)
  : server(server),
    failedTransactions(),
    remoteTransactions(),
    remoteTransactionCollections(),
    remoteTransactionDatabases(),
    lastTick(0),
    logfilesToProcess(),
    openedCollections(),
    openedDatabases(),
    runningRemoteTransactions(),
    emptyLogfiles(),
    policy(TRI_DOC_UPDATE_ONLY_IF_NEWER, 0, nullptr),
    ignoreRecoveryErrors(ignoreRecoveryErrors) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the recover state
////////////////////////////////////////////////////////////////////////////////

RecoverState::~RecoverState () {
  releaseResources();

  // free running remote transactions
  for (auto it = runningRemoteTransactions.begin(); it != runningRemoteTransactions.end(); ++it) {
    auto trx = (*it).second;

    delete trx;
  }

  runningRemoteTransactions.clear();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief release opened collections and databases so they can be shut down
/// etc.
////////////////////////////////////////////////////////////////////////////////
  
void RecoverState::releaseResources () {
  // hand over running remote transactions to the applier
  for (auto it = runningRemoteTransactions.begin(); it != runningRemoteTransactions.end(); ++it) {
    auto* trx = (*it).second;
    
    TRI_vocbase_t* vocbase = trx->vocbase();
    TRI_ASSERT(vocbase != nullptr);

    auto* applier = vocbase->_replicationApplier;
    TRI_ASSERT(applier != nullptr);
    
    applier->addRemoteTransaction(trx);
  }

  // reset trx counter as we're moving transactions from this thread to a potential other
  triagens::arango::TransactionBase::setNumbers(0, 0);

  runningRemoteTransactions.clear();


  // release all collections
  for (auto it = openedCollections.begin(); it != openedCollections.end(); ++it) {
    TRI_vocbase_col_t* collection = (*it).second;
    TRI_ReleaseCollectionVocBase(collection->_vocbase, collection);
  }
  
  openedCollections.clear();
  
  // release all databases
  for (auto it = openedDatabases.begin(); it != openedDatabases.end(); ++it) {
    TRI_vocbase_t* vocbase = (*it).second;
    TRI_ReleaseDatabaseServer(server, vocbase);
  }
  
  openedDatabases.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets a database (and inserts it into the cache if not in it)
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* RecoverState::useDatabase (TRI_voc_tick_t databaseId) {
  auto it = openedDatabases.find(databaseId);

  if (it != openedDatabases.end()) {
    return (*it).second;
  }

  TRI_vocbase_t* vocbase = TRI_UseDatabaseByIdServer(server, databaseId);

  if (vocbase == nullptr) {
    return nullptr;
  }
        
  openedDatabases.insert(it, std::make_pair(databaseId, vocbase));
  return vocbase;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief release a database (so it can be dropped)
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* RecoverState::releaseDatabase (TRI_voc_tick_t databaseId) {
  auto it = openedDatabases.find(databaseId);

  if (it == openedDatabases.end()) {
    return nullptr;
  }
  
  TRI_vocbase_t* vocbase = (*it).second;

  TRI_ASSERT(vocbase != nullptr);

  // release all collections we ourselves have opened for this database
  auto it2 = openedCollections.begin();

  while (it2 != openedCollections.end()) {
    TRI_vocbase_col_t* collection = (*it2).second;
  
    TRI_ASSERT(collection != nullptr);

    if (collection->_vocbase->_id == databaseId) {
      // correct database, now release the collection
      TRI_ASSERT(vocbase == collection->_vocbase);

      TRI_ReleaseCollectionVocBase(vocbase, collection);
      // get new iterator position
      it2 = openedCollections.erase(it2);
    }
    else {
      // collection not found, advance in the loop
      ++it2;
    }
  }
  
  TRI_ReleaseDatabaseServer(server, vocbase);
  openedDatabases.erase(databaseId);
        
  return vocbase;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief release a collection (so it can be dropped)
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* RecoverState::releaseCollection (TRI_voc_cid_t collectionId) {
  auto it = openedCollections.find(collectionId);

  if (it == openedCollections.end()) {
    return nullptr;
  }
  
  TRI_vocbase_col_t* collection = (*it).second;
  
  TRI_ASSERT(collection != nullptr);
  TRI_ReleaseCollectionVocBase(collection->_vocbase, collection);
  openedCollections.erase(collectionId);

  return collection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets a collection (and inserts it into the cache if not in it)
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* RecoverState::useCollection (TRI_vocbase_t* vocbase,
                                                TRI_voc_cid_t collectionId) {
  auto it = openedCollections.find(collectionId);

  if (it != openedCollections.end()) {
    return (*it).second;
  }

  TRI_vocbase_col_status_e status; // ignored here
  TRI_vocbase_col_t* collection = TRI_UseCollectionByIdVocBase(vocbase, collectionId, status);

  if (collection == nullptr) {
    return nullptr;
  }
  
  TRI_document_collection_t* document = collection->_collection;
  TRI_ASSERT(document != nullptr);

  // disable secondary indexes for the moment
  document->useSecondaryIndexes(false);

  openedCollections.insert(it, std::make_pair(collectionId, collection));
  return collection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a collection
/// the collection will be opened after this call and inserted into a local
/// cache for faster lookups
/// returns nullptr if the collection does not exist
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t* RecoverState::getCollection (TRI_voc_tick_t databaseId,
                                                        TRI_voc_cid_t collectionId) {

  TRI_vocbase_t* vocbase = useDatabase(databaseId);

  if (vocbase == nullptr) {
    LOG_TRACE("database %llu not found", (unsigned long long) databaseId);
    return nullptr;
  }
  
  TRI_vocbase_col_t* collection = useCollection(vocbase, collectionId);

  if (collection == nullptr) {
    LOG_TRACE("collection %llu of database %llu not found", (unsigned long long) collectionId, (unsigned long long) databaseId);
    return nullptr;
  }

  TRI_document_collection_t* document = collection->_collection;
  TRI_ASSERT(document != nullptr);

  return document;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an operation in a remote transaction
////////////////////////////////////////////////////////////////////////////////

int RecoverState::executeRemoteOperation (TRI_voc_tick_t databaseId,
                                          TRI_voc_cid_t collectionId,
                                          TRI_voc_tid_t transactionId,
                                          TRI_df_marker_t const* marker,
                                          TRI_voc_fid_t fid,
                                          std::function<int(RemoteTransactionType*, Marker*)> func) {

  auto it = remoteTransactions.find(transactionId);
  if (it == remoteTransactions.end()) {
    LOG_WARNING("remote transaction not found: internal error");
    return TRI_ERROR_INTERNAL;
  }

  TRI_voc_tid_t externalId = (*it).second.second;
  auto it2 = runningRemoteTransactions.find(externalId);
  if (it2 == runningRemoteTransactions.end()) {
    LOG_WARNING("remote transaction not found: internal error");
    return TRI_ERROR_INTERNAL;
  }
  
  auto trx = (*it2).second;

  registerRemoteUsage(databaseId, collectionId);

  EnvelopeMarker* envelope = nullptr;
  int res = TRI_ERROR_INTERNAL;

  try {
    envelope = new EnvelopeMarker(marker, fid);

    // execute the operation
    res = func(trx, envelope);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }
  catch (triagens::arango::Exception const& ex) {
    res = ex.code();
  }
  catch (...) {
    res = TRI_ERROR_INTERNAL;
  }
    
  if (envelope != nullptr) {
    delete envelope;
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a single operation inside a transaction
////////////////////////////////////////////////////////////////////////////////

int RecoverState::executeSingleOperation (TRI_voc_tick_t databaseId,
                                          TRI_voc_cid_t collectionId,
                                          TRI_df_marker_t const* marker,
                                          TRI_voc_fid_t fid,
                                          std::function<int(SingleWriteTransactionType*, Marker*)> func) {

  // first find the correct database
  TRI_vocbase_t* vocbase = useDatabase(databaseId);

  if (vocbase == nullptr) {
    LOG_WARNING("database %llu not found", (unsigned long long) databaseId);
    return TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
  }

  TRI_vocbase_col_t* collection = useCollection(vocbase, collectionId);

  if (collection == nullptr || collection->_collection == nullptr) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  TRI_voc_tick_t tickMax = collection->_collection->_tickMax;
  if (marker->_tick <= tickMax) {
    // already transferred this marker
    return TRI_ERROR_NO_ERROR;
  }


  SingleWriteTransactionType* trx = nullptr;
  EnvelopeMarker* envelope = nullptr;
  int res = TRI_ERROR_INTERNAL;

  try {
    trx = new SingleWriteTransactionType(new triagens::arango::StandaloneTransactionContext(), vocbase, collectionId);

    if (trx == nullptr) {
      THROW_ARANGO_EXCEPTION(res);
    }

    trx->addHint(TRI_TRANSACTION_HINT_NO_BEGIN_MARKER, false);
    trx->addHint(TRI_TRANSACTION_HINT_NO_ABORT_MARKER, false);
    trx->addHint(TRI_TRANSACTION_HINT_NO_THROTTLING, false);
    trx->addHint(TRI_TRANSACTION_HINT_LOCK_NEVER, false);

    res = trx->begin();

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
    
    envelope = new EnvelopeMarker(marker, fid);

    // execute the operation
    res = func(trx, envelope);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    // commit the operation
    res = trx->commit();
  }
  catch (triagens::arango::Exception const& ex) {
    res = ex.code();
  }
  catch (...) {
    res = TRI_ERROR_INTERNAL;
  }
    
  if (envelope != nullptr) {
    delete envelope;
  }

  if (trx != nullptr) {
    delete trx;
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief callback to handle one marker during recovery
/// this function only builds up state and does not change any data
////////////////////////////////////////////////////////////////////////////////

bool RecoverState::InitialScanMarker (TRI_df_marker_t const* marker,
                                      void* data,
                                      TRI_datafile_t* datafile) {
  RecoverState* state = reinterpret_cast<RecoverState*>(data);

  TRI_ASSERT(marker != nullptr);

  // note the marker's tick
  TRI_ASSERT(marker->_tick >= state->lastTick);

  if (marker->_tick > state->lastTick) {
    state->lastTick = marker->_tick;
  }

  switch (marker->_type) {

    // -----------------------------------------------------------------------------
    // transactions
    // -----------------------------------------------------------------------------

    case TRI_WAL_MARKER_BEGIN_TRANSACTION: {
      transaction_begin_marker_t const* m = reinterpret_cast<transaction_begin_marker_t const*>(marker);
      // insert this transaction into the list of failed transactions
      // we do this because if we don't find a commit marker for this transaction,
      // we'll have it in the failed list at the end of the scan and can ignore it
      state->failedTransactions.emplace(std::make_pair(m->_transactionId, std::make_pair(m->_databaseId, false)));
      break;
    }

    case TRI_WAL_MARKER_COMMIT_TRANSACTION: {
      transaction_commit_marker_t const* m = reinterpret_cast<transaction_commit_marker_t const*>(marker);
      // remove this transaction from the list of failed transactions
      state->failedTransactions.erase(m->_transactionId);
      break;
    }

    case TRI_WAL_MARKER_ABORT_TRANSACTION: {
      // insert this transaction into the list of failed transactions
      transaction_abort_marker_t const* m = reinterpret_cast<transaction_abort_marker_t const*>(marker);

      auto it = state->failedTransactions.find(m->_transactionId);
      if (it != state->failedTransactions.end()) {
        // delete previous element if present
        state->failedTransactions.erase(m->_transactionId);
      }

      // and (re-)insert
      state->failedTransactions.emplace(std::make_pair(m->_transactionId, std::make_pair(m->_databaseId, true)));
      break;
    }

    case TRI_WAL_MARKER_BEGIN_REMOTE_TRANSACTION: {
      transaction_remote_begin_marker_t const* m = reinterpret_cast<transaction_remote_begin_marker_t const*>(marker);
      // insert this transaction into the list of remote transactions
      state->remoteTransactions.emplace(std::make_pair(m->_transactionId, std::make_pair(m->_databaseId, m->_externalId)));
      break;
    }

    case TRI_WAL_MARKER_COMMIT_REMOTE_TRANSACTION: {
      transaction_remote_commit_marker_t const* m = reinterpret_cast<transaction_remote_commit_marker_t const*>(marker);
      // remove this transaction from the list of remote transactions
      state->remoteTransactions.erase(m->_transactionId);
      break;
    }

    case TRI_WAL_MARKER_ABORT_REMOTE_TRANSACTION: {
      transaction_remote_abort_marker_t const* m = reinterpret_cast<transaction_remote_abort_marker_t const*>(marker);
      // insert this transaction into the list of failed transactions
      // the transaction is treated the same as a regular local transaction that is aborted
      auto it = state->failedTransactions.find(m->_transactionId);
      if (it == state->failedTransactions.end()) {
        // insert the transaction into the list of failed transactions
        state->failedTransactions.emplace(std::make_pair(m->_transactionId, std::make_pair(m->_databaseId, false)));
      }
      
      // remove this transaction from the list of remote transactions
      state->remoteTransactions.erase(m->_transactionId);
      break;
    }
/*
    // -----------------------------------------------------------------------------
    // create markers 
    // -----------------------------------------------------------------------------

    case TRI_WAL_MARKER_CREATE_COLLECTION: {
      collection_create_marker_t const* m = reinterpret_cast<collection_create_marker_t const*>(marker);
      // undo a potential drop marker discovered before for the same collection
      state->droppedCollections.erase(m->_collectionId);
      break;
    }
  
    case TRI_WAL_MARKER_CREATE_DATABASE: {
      database_create_marker_t const* m = reinterpret_cast<database_create_marker_t const*>(marker);
      // undo a potential drop marker discovered before for the same database
      state->droppedDatabases.erase(m->_databaseId);
      break;
    }
      
    case TRI_WAL_MARKER_CREATE_INDEX: {
      // ignored
      break;
    }
   
    // -----------------------------------------------------------------------------
    // drop markers 
    // -----------------------------------------------------------------------------
*/
 
    case TRI_WAL_MARKER_DROP_COLLECTION: {
      collection_drop_marker_t const* m = reinterpret_cast<collection_drop_marker_t const*>(marker);
      // note that the collection was dropped and doesn't need to be recovered
      state->droppedIds.insert(m->_collectionId);
      break;
    }

/*
    case TRI_WAL_MARKER_DROP_DATABASE: {
      database_drop_marker_t const* m = reinterpret_cast<database_drop_marker_t const*>(marker);
      // note that the database was dropped and doesn't need to be recovered
      state->droppedDatabases.insert(m->_databaseId);
      break;
    }
       
    case TRI_WAL_MARKER_DROP_INDEX: {
      // ignored
      break;
    }
   */ 
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief callback to replay one marker during recovery
/// this function modifies indexes etc.
////////////////////////////////////////////////////////////////////////////////

bool RecoverState::ReplayMarker (TRI_df_marker_t const* marker,
                                 void* data,
                                 TRI_datafile_t* datafile) {
  RecoverState* state = reinterpret_cast<RecoverState*>(data);
 
#ifdef TRI_ENABLE_FAILURE_TESTS
  LOG_TRACE("replaying marker of type %s", TRI_NameMarkerDatafile(marker));
#endif
  
  switch (marker->_type) {

    // -----------------------------------------------------------------------------
    // attributes and shapes
    // -----------------------------------------------------------------------------

    case TRI_WAL_MARKER_ATTRIBUTE: {
      // re-insert the attribute into the shaper
      attribute_marker_t const* m = reinterpret_cast<attribute_marker_t const*>(marker);
      TRI_voc_cid_t collectionId = m->_collectionId;
      TRI_voc_tick_t databaseId  = m->_databaseId;

      if (state->isDropped(databaseId, collectionId)) {
        return true;
      }

      int res = state->executeSingleOperation(databaseId, collectionId, marker, datafile->_fid, [&](SingleWriteTransactionType* trx, Marker* envelope) -> int { 
        TRI_document_collection_t* document = trx->documentCollection();

        // re-insert the attribute
        int res = TRI_InsertAttributeVocShaper(document->getShaper(), marker, false);
        return res;
      });

      if (res != TRI_ERROR_NO_ERROR && 
          res != TRI_ERROR_ARANGO_DATABASE_NOT_FOUND && 
          res != TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
        LOG_WARNING("could not apply attribute marker: %s", TRI_errno_string(res));
        return state->canContinue();
      }
      break;
    }


    case TRI_WAL_MARKER_SHAPE: {
      // re-insert the shape into the shaper
      shape_marker_t const* m = reinterpret_cast<shape_marker_t const*>(marker);
      TRI_voc_cid_t collectionId = m->_collectionId;
      TRI_voc_tick_t databaseId  = m->_databaseId;

      if (state->isDropped(databaseId, collectionId)) {
        return true;
      }
      
      int res = state->executeSingleOperation(databaseId, collectionId, marker, datafile->_fid, [&](SingleWriteTransactionType* trx, Marker* envelope) -> int { 
        TRI_document_collection_t* document = trx->documentCollection();
     
        // re-insert the shape
        int res = TRI_InsertShapeVocShaper(document->getShaper(), marker, false);
        return res;
      });

      if (res != TRI_ERROR_NO_ERROR && 
          res != TRI_ERROR_ARANGO_DATABASE_NOT_FOUND && 
          res != TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
        LOG_WARNING("could not apply shape marker: %s", TRI_errno_string(res));
        return state->canContinue();
      }
      break;
    }
   
    
    // -----------------------------------------------------------------------------
    // crud operations
    // -----------------------------------------------------------------------------

    case TRI_WAL_MARKER_DOCUMENT: {
      // re-insert the document into the collection
      document_marker_t const* m = reinterpret_cast<document_marker_t const*>(marker);
      TRI_voc_cid_t collectionId = m->_collectionId;
      TRI_voc_tick_t databaseId  = m->_databaseId;

      if (state->isDropped(databaseId, collectionId)) {
        return true;
      }

      TRI_voc_tick_t transactionId = m->_transactionId;
      if (state->ignoreTransaction(transactionId)) {
        // transaction was aborted
        return true;
      }
        
      char const* base = reinterpret_cast<char const*>(m); 
      char const* key = base + m->_offsetKey;
      TRI_shaped_json_t shaped;
      TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, m);

      int res = TRI_ERROR_NO_ERROR;

      if (state->isRemoteTransaction(transactionId)) {
        // remote operation
        res = state->executeRemoteOperation(databaseId, collectionId, transactionId, marker, datafile->_fid, [&](RemoteTransactionType* trx, Marker* envelope) -> int { 
          if (IsVolatile(trx->trxCollection(collectionId))) {
            return TRI_ERROR_NO_ERROR;
          } 

          TRI_doc_mptr_copy_t mptr;
          int res = TRI_InsertShapedJsonDocumentCollection(trx->trxCollection(collectionId), (TRI_voc_key_t) key, m->_revisionId, envelope, &mptr, &shaped, nullptr, false, false, true);

          if (res == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) {
            state->policy.setExpectedRevision(m->_revisionId);
            res = TRI_UpdateShapedJsonDocumentCollection(trx->trxCollection(collectionId), (TRI_voc_key_t) key, m->_revisionId, envelope, &mptr, &shaped, &state->policy, false, false);
          }

          return res;
        });
      }
      else if (! state->isUsedByRemoteTransaction(collectionId)) {
        // local operation
        res = state->executeSingleOperation(databaseId, collectionId, marker, datafile->_fid, [&](SingleWriteTransactionType* trx, Marker* envelope) -> int { 
          if (IsVolatile(trx->trxCollection())) {
            return TRI_ERROR_NO_ERROR;
          } 

          TRI_doc_mptr_copy_t mptr;
          int res = TRI_InsertShapedJsonDocumentCollection(trx->trxCollection(), (TRI_voc_key_t) key, m->_revisionId, envelope, &mptr, &shaped, nullptr, false, false, true);

          if (res == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) {
            state->policy.setExpectedRevision(m->_revisionId);
            res = TRI_UpdateShapedJsonDocumentCollection(trx->trxCollection(), (TRI_voc_key_t) key, m->_revisionId, envelope, &mptr, &shaped, &state->policy, false, false);
          }

          return res;
        });
      }
      else {
        // ERROR - found a local action for a collection that has an ongoing remote transaction
        res = TRI_ERROR_TRANSACTION_INTERNAL;
      }
      
      if (res != TRI_ERROR_NO_ERROR && 
          res != TRI_ERROR_ARANGO_CONFLICT &&
          res != TRI_ERROR_ARANGO_DATABASE_NOT_FOUND && 
          res != TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
        LOG_WARNING("unable to insert document in collection %llu of database %llu: %s", 
                    (unsigned long long) collectionId,
                    (unsigned long long) databaseId,
                    TRI_errno_string(res));
        return state->canContinue();
      }
      break;
    }


    case TRI_WAL_MARKER_EDGE: {
      // re-insert the edge into the collection
      edge_marker_t const* m = reinterpret_cast<edge_marker_t const*>(marker);
      TRI_voc_cid_t collectionId = m->_collectionId;
      TRI_voc_tick_t databaseId  = m->_databaseId;
      
      if (state->isDropped(databaseId, collectionId)) {
        return true;
      }
      
      TRI_voc_tick_t transactionId = m->_transactionId;
      if (state->ignoreTransaction(transactionId)) {
        return true;
      }

      char const* base = reinterpret_cast<char const*>(m); 
      char const* key = base + m->_offsetKey;
      TRI_document_edge_t edge;
      edge._fromCid = m->_fromCid;
      edge._toCid   = m->_toCid;
      edge._fromKey = const_cast<char*>(base) + m->_offsetFromKey;
      edge._toKey   = const_cast<char*>(base) + m->_offsetToKey;

      TRI_shaped_json_t shaped;
      TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, m);

      int res = TRI_ERROR_NO_ERROR;
      
      if (state->isRemoteTransaction(transactionId)) {
        // remote operation
        res = state->executeRemoteOperation(databaseId, collectionId, transactionId, marker, datafile->_fid, [&](RemoteTransactionType* trx, Marker* envelope) -> int {
          if (IsVolatile(trx->trxCollection(collectionId))) {
            return TRI_ERROR_NO_ERROR;
          } 

          TRI_doc_mptr_copy_t mptr;
          int res = TRI_InsertShapedJsonDocumentCollection(trx->trxCollection(collectionId), (TRI_voc_key_t) key, m->_revisionId, envelope, &mptr, &shaped, &edge, false, false, true);

          if (res == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) {
            state->policy.setExpectedRevision(m->_revisionId);
            res = TRI_UpdateShapedJsonDocumentCollection(trx->trxCollection(collectionId), (TRI_voc_key_t) key, m->_revisionId, envelope, &mptr, &shaped, &state->policy, false, false);
          }

          return res;
        });
      }
      else if (! state->isUsedByRemoteTransaction(collectionId)) {
        // local operation
        res = state->executeSingleOperation(databaseId, collectionId, marker, datafile->_fid, [&](SingleWriteTransactionType* trx, Marker* envelope) -> int {
          if (IsVolatile(trx->trxCollection())) {
            return TRI_ERROR_NO_ERROR;
          } 
 
          TRI_doc_mptr_copy_t mptr;
          int res = TRI_InsertShapedJsonDocumentCollection(trx->trxCollection(), (TRI_voc_key_t) key, m->_revisionId, envelope, &mptr, &shaped, &edge, false, false, true);

          if (res == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) {
            state->policy.setExpectedRevision(m->_revisionId);
            res = TRI_UpdateShapedJsonDocumentCollection(trx->trxCollection(), (TRI_voc_key_t) key, m->_revisionId, envelope, &mptr, &shaped, &state->policy, false, false);
          }

          return res;
        });
      }
      else {
        // ERROR - found a local action for a collection that has an ongoing remote transaction
        res = TRI_ERROR_TRANSACTION_INTERNAL;
      }

      if (res != TRI_ERROR_NO_ERROR && 
          res != TRI_ERROR_ARANGO_CONFLICT &&
          res != TRI_ERROR_ARANGO_DATABASE_NOT_FOUND && 
          res != TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
        LOG_WARNING("unable to insert edge in collection %llu of database %llu: %s", 
                    (unsigned long long) collectionId,
                    (unsigned long long) databaseId,
                    TRI_errno_string(res));
        return state->canContinue();
      }

      break;
    }


    case TRI_WAL_MARKER_REMOVE: {
      // re-apply the remove operation
      remove_marker_t const* m = reinterpret_cast<remove_marker_t const*>(marker);
      TRI_voc_cid_t collectionId = m->_collectionId;
      TRI_voc_tick_t databaseId  = m->_databaseId;
      
      if (state->isDropped(databaseId, collectionId)) {
        return true;
      }

      TRI_voc_tick_t transactionId = m->_transactionId;
      if (state->ignoreTransaction(transactionId)) {
        return true;
      }
     
      char const* base = reinterpret_cast<char const*>(m); 
      char const* key = base + sizeof(remove_marker_t);
      int res = TRI_ERROR_NO_ERROR;
       
      if (state->isRemoteTransaction(transactionId)) {
        // remote operation
        res = state->executeRemoteOperation(databaseId, collectionId, transactionId, marker, datafile->_fid, [&](RemoteTransactionType* trx, Marker* envelope) -> int { 
          if (IsVolatile(trx->trxCollection(collectionId))) {
            return TRI_ERROR_NO_ERROR;
          } 

          // remove the document and ignore any potential errors
          state->policy.setExpectedRevision(m->_revisionId);
          TRI_RemoveShapedJsonDocumentCollection(trx->trxCollection(collectionId), (TRI_voc_key_t) key, m->_revisionId, envelope, &state->policy, false, false);

          return TRI_ERROR_NO_ERROR;
        });
      }
      else if (! state->isUsedByRemoteTransaction(collectionId)) {
        // local operation
        res = state->executeSingleOperation(databaseId, collectionId, marker, datafile->_fid, [&](SingleWriteTransactionType* trx, Marker* envelope) -> int { 
          if (IsVolatile(trx->trxCollection())) {
            return TRI_ERROR_NO_ERROR;
          } 

          // remove the document and ignore any potential errors
          state->policy.setExpectedRevision(m->_revisionId);
          TRI_RemoveShapedJsonDocumentCollection(trx->trxCollection(), (TRI_voc_key_t) key, m->_revisionId, envelope, &state->policy, false, false);

          return TRI_ERROR_NO_ERROR;
        });
      }
      else {
        // ERROR - found a local action for a collection that has an ongoing remote transaction
        res = TRI_ERROR_TRANSACTION_INTERNAL;
      }

      if (res != TRI_ERROR_NO_ERROR && 
          res != TRI_ERROR_ARANGO_CONFLICT &&
          res != TRI_ERROR_ARANGO_DATABASE_NOT_FOUND && 
          res != TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
        LOG_WARNING("unable to remove document in collection %llu of database %llu: %s", 
                    (unsigned long long) collectionId,
                    (unsigned long long) databaseId,
                    TRI_errno_string(res));
        return state->canContinue();
      }
      break;
    }
    

    // -----------------------------------------------------------------------------
    // transactions
    // -----------------------------------------------------------------------------
    
    case TRI_WAL_MARKER_BEGIN_REMOTE_TRANSACTION: {
      transaction_remote_begin_marker_t const* m = reinterpret_cast<transaction_remote_begin_marker_t const*>(marker);
      TRI_voc_tick_t databaseId  = m->_databaseId;
      TRI_voc_tid_t externalId   = m->_externalId;
      // start a remote transaction
      
      if (state->isDropped(databaseId)) {
        return true;
      }
      
      TRI_vocbase_t* vocbase = state->useDatabase(databaseId);
      if (vocbase == nullptr) {
        LOG_WARNING("cannot start remote transaction in database %llu: %s", 
                    (unsigned long long) databaseId,
                    TRI_errno_string(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND));
      }

      auto trx = new RemoteTransactionType(state->server, vocbase, externalId);

      if (trx == nullptr) {
        LOG_WARNING("unable to start transaction: %s", TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY));
        return state->canContinue();
      }

      trx->addHint(TRI_TRANSACTION_HINT_NO_BEGIN_MARKER, true);

      int res = trx->begin();

      if (res != TRI_ERROR_NO_ERROR) {
        LOG_WARNING("unable to start transaction: %s", TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY));
        delete trx;
        return state->canContinue();
      }

      state->runningRemoteTransactions.emplace(std::make_pair(m->_externalId, trx));
      break;
    }
    
    
    case TRI_WAL_MARKER_RENAME_COLLECTION: {
      collection_rename_marker_t const* m = reinterpret_cast<collection_rename_marker_t const*>(marker);
      TRI_voc_cid_t collectionId = m->_collectionId;
      TRI_voc_tick_t databaseId  = m->_databaseId;
      
      if (state->isDropped(databaseId)) {
        return true;
      }

      TRI_vocbase_t* vocbase = state->useDatabase(databaseId);

      if (vocbase == nullptr) {
        // if the underlying database is gone, we can go on
        LOG_TRACE("cannot open database %llu", (unsigned long long) databaseId);
        return true;
      }

      TRI_vocbase_col_t* collection = state->releaseCollection(collectionId);

      if (collection == nullptr) {
        collection = TRI_LookupCollectionByIdVocBase(vocbase, collectionId);
      }

      if (collection == nullptr) {
        // if the underlying collection is gone, we can go on
        LOG_TRACE("cannot open collection %llu", (unsigned long long) collectionId);
        return true;
      }

      char const* name = reinterpret_cast<char const*>(m) + sizeof(collection_rename_marker_t);

    
      // check if other collection exist with target name
      TRI_vocbase_col_t* other = TRI_LookupCollectionByNameVocBase(vocbase, name); 

      if (other != nullptr) {
        TRI_voc_cid_t otherCid = other->_cid;
        state->releaseCollection(otherCid);
        int statusCode = TRI_DropCollectionVocBase(vocbase, other, false);
        WaitForDeletion(vocbase, otherCid, statusCode);
      }

      int res = TRI_RenameCollectionVocBase(vocbase, collection, name, true, false);

      if (res != TRI_ERROR_NO_ERROR) {
        LOG_WARNING("cannot rename collection collection %llu in database %llu: %s", 
                    (unsigned long long) collectionId, 
                    (unsigned long long) databaseId,
                    TRI_errno_string(res));
        return state->canContinue();
      }
      break;
    }
    
    case TRI_WAL_MARKER_CHANGE_COLLECTION: {
      collection_change_marker_t const* m = reinterpret_cast<collection_change_marker_t const*>(marker);
      TRI_voc_cid_t collectionId = m->_collectionId;
      TRI_voc_tick_t databaseId  = m->_databaseId;
      
      if (state->isDropped(databaseId)) {
        return true;
      }

      TRI_vocbase_t* vocbase = state->useDatabase(databaseId);

      if (vocbase == nullptr) {
        // if the underlying database is gone, we can go on
        LOG_TRACE("cannot open database %llu", (unsigned long long) databaseId);
        return true;
      }

      TRI_document_collection_t* document = state->getCollection(databaseId, collectionId);

      if (document == nullptr) {
        // if the underlying collection is gone, we can go on
        LOG_TRACE("cannot change properties of collection %llu in database %llu: %s", 
                  (unsigned long long) collectionId,
                  (unsigned long long) databaseId,
                  TRI_errno_string(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND));
        return true;
      }
      
      char const* properties = reinterpret_cast<char const*>(m) + sizeof(collection_change_marker_t);
      TRI_json_t* json = triagens::basics::JsonHelper::fromString(properties);

      if (! TRI_IsObjectJson(json)) {
        if (json != nullptr) {
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
        }
        LOG_WARNING("cannot unpack collection properties for collection %llu in database %llu", 
                    (unsigned long long) collectionId, 
                    (unsigned long long) databaseId);
        return state->canContinue();
      }
      
      TRI_json_t const* value;
      
      TRI_col_info_t parameters;
      parameters._doCompact = true;
      parameters._waitForSync = vocbase->_settings.defaultWaitForSync;
      parameters._maximalSize = vocbase->_settings.defaultMaximalSize; 

      value = TRI_LookupObjectJson(json, "doCompact");
      if (TRI_IsBooleanJson(value)) {
        parameters._doCompact = value->_value._boolean;
      }
      
      value = TRI_LookupObjectJson(json, "waitForSync");
      if (TRI_IsBooleanJson(value)) {
        parameters._waitForSync = value->_value._boolean;
      }
      
      value = TRI_LookupObjectJson(json, "maximalSize");
      if (TRI_IsNumberJson(value)) {
        parameters._maximalSize = static_cast<TRI_voc_size_t>(value->_value._number);
      }
      
      int res = TRI_UpdateCollectionInfo(vocbase, document, &parameters, vocbase->_settings.forceSyncProperties);

      if (res != TRI_ERROR_NO_ERROR) {
        LOG_WARNING("cannot change collection properties for collection %llu in database %llu: %s", 
                    (unsigned long long) collectionId, 
                    (unsigned long long) databaseId,
                    TRI_errno_string(res));
        return state->canContinue();
      }
      break;
    }


    // -----------------------------------------------------------------------------
    // create operations
    // -----------------------------------------------------------------------------

    case TRI_WAL_MARKER_CREATE_INDEX: {
      index_create_marker_t const* m = reinterpret_cast<index_create_marker_t const*>(marker);
      TRI_voc_cid_t collectionId = m->_collectionId;
      TRI_voc_tick_t databaseId  = m->_databaseId;
      TRI_idx_iid_t indexId      = m->_indexId;
      
      if (state->isDropped(databaseId, collectionId)) {
        return true;
      }
     
      TRI_vocbase_t* vocbase = state->useDatabase(databaseId);

      if (vocbase == nullptr) {
        // if the underlying database is gone, we can go on
        LOG_TRACE("cannot create index for collection %llu in database %llu: %s", 
                  (unsigned long long) collectionId,
                  (unsigned long long) databaseId,
                  TRI_errno_string(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND));
        return true;
      }

      TRI_document_collection_t* document = state->getCollection(databaseId, collectionId);

      if (document == nullptr) {
        // if the underlying collection is gone, we can go on
        LOG_TRACE("cannot create index for collection %llu in database %llu: %s", 
                  (unsigned long long) collectionId,
                  (unsigned long long) databaseId,
                  TRI_errno_string(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND));
        return true;
      }
      
      char const* properties = reinterpret_cast<char const*>(m) + sizeof(index_create_marker_t);
      TRI_json_t* json = triagens::basics::JsonHelper::fromString(properties);
      
      if (! TRI_IsObjectJson(json)) {
        if (json != nullptr) {
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
        }
        LOG_WARNING("cannot unpack index properties for index %llu, collection %llu in database %llu", 
                    (unsigned long long) indexId, 
                    (unsigned long long) collectionId, 
                    (unsigned long long) databaseId);
        return state->canContinue();
      }

      if (! TRI_IsObjectJson(json)) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
        LOG_WARNING("cannot unpack index properties for index %llu, collection %llu in database %llu", 
                    (unsigned long long) indexId, 
                    (unsigned long long) collectionId, 
                    (unsigned long long) databaseId);
        return state->canContinue();
      }

      // fake transaction to satisfy assertions
      triagens::arango::TransactionBase trx(true); 
      
      std::string collectionDirectory = GetCollectionDirectory(vocbase, collectionId);
      char* idString = TRI_StringUInt64(indexId);
      char* indexName = TRI_Concatenate3String("index-", idString, ".json");
      TRI_FreeString(TRI_CORE_MEM_ZONE, idString);
      char* filename = TRI_Concatenate2File(collectionDirectory.c_str(), indexName);
      TRI_FreeString(TRI_CORE_MEM_ZONE, indexName);
      
      bool ok = TRI_SaveJson(filename, json, vocbase->_settings.forceSyncProperties);
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

      if (! ok) {
        TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
        LOG_WARNING("cannot create index %llu, collection %llu in database %llu", 
                    (unsigned long long) indexId, 
                    (unsigned long long) collectionId, 
                    (unsigned long long) databaseId);
        return state->canContinue();
      }
      else {
        TRI_PushBackVectorString(&document->_indexFiles, filename);
      }

      break;
    }


    case TRI_WAL_MARKER_CREATE_COLLECTION: {
      collection_create_marker_t const* m = reinterpret_cast<collection_create_marker_t const*>(marker);
      TRI_voc_cid_t collectionId = m->_collectionId;
      TRI_voc_tick_t databaseId  = m->_databaseId;
      
      // remove the drop marker 
      state->droppedCollections.erase(collectionId);
      
      if (state->isDropped(databaseId)) {
        return true;
      }

      TRI_vocbase_t* vocbase = state->useDatabase(databaseId);

      if (vocbase == nullptr) {
        // if the underlying database is gone, we can go on
        LOG_TRACE("cannot open database %llu", (unsigned long long) databaseId);
        return true;
      }

      TRI_vocbase_col_t* collection = state->releaseCollection(collectionId);

      if (collection == nullptr) {
        collection = TRI_LookupCollectionByIdVocBase(vocbase, collectionId);
      }

      if (collection != nullptr) {
        // drop an existing collection
        int statusCode = TRI_DropCollectionVocBase(vocbase, collection, false);
        WaitForDeletion(vocbase, collectionId, statusCode);
      }

      char const* properties = reinterpret_cast<char const*>(m) + sizeof(collection_create_marker_t);
      TRI_json_t* json = triagens::basics::JsonHelper::fromString(properties);
        
      if (! TRI_IsObjectJson(json)) {
        if (json != nullptr) {
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
        }
        LOG_WARNING("cannot unpack collection properties for collection %llu in database %llu", 
                    (unsigned long long) collectionId, 
                    (unsigned long long) databaseId);
        return state->canContinue();
      }

      // check if there is another collection with the same name as the one that we attempt to create
      TRI_json_t const* name = TRI_LookupObjectJson(json, "name");

      if (TRI_IsStringJson(name)) {
        collection = TRI_LookupCollectionByNameVocBase(vocbase, name->_value._string.data);
        
        if (collection != nullptr && ! TRI_IsSystemNameCollection(name->_value._string.data)) {
          // if yes, delete it
          TRI_voc_cid_t otherCid = collection->_cid;

          state->releaseCollection(otherCid);
          int statusCode = TRI_DropCollectionVocBase(vocbase, collection, false);
          WaitForDeletion(vocbase, otherCid, statusCode);
        }
      }

      
      TRI_col_info_t info;
      memset(&info, 0, sizeof(TRI_col_info_t));

      TRI_FromJsonCollectionInfo(&info, json);
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      
      // fake transaction to satisfy assertions
      triagens::arango::TransactionBase trx(true); 

      WaitForDeletion(vocbase, collectionId, TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  
      if (state->willBeDropped(collectionId)) {
        // in case we detect that this collection is going to be deleted anyway, set
        // the sync properties to false temporarily
        bool oldSync = vocbase->_settings.forceSyncProperties;
        vocbase->_settings.forceSyncProperties = false;
        collection = TRI_CreateCollectionVocBase(vocbase, &info, collectionId, false);
        vocbase->_settings.forceSyncProperties = oldSync;

      }
      else {
        // collection will be kept
        collection = TRI_CreateCollectionVocBase(vocbase, &info, collectionId, false);
      }

      TRI_FreeCollectionInfoOptions(&info);

      if (collection == nullptr) {
        LOG_WARNING("cannot create collection %llu in database %llu", 
                    (unsigned long long) collectionId, 
                    (unsigned long long) databaseId);
        return state->canContinue();
      }
      break;
    }


    case TRI_WAL_MARKER_CREATE_DATABASE: {
      database_create_marker_t const* m = reinterpret_cast<database_create_marker_t const*>(marker);
      TRI_voc_tick_t databaseId = m->_databaseId;
     
      // remove the drop marker 
      state->droppedDatabases.erase(databaseId);
      TRI_vocbase_t* vocbase = state->releaseDatabase(databaseId);

      if (vocbase != nullptr) {
        // remove already existing database
        int statusCode = TRI_DropByIdDatabaseServer(state->server, databaseId, false, false);
        WaitForDeletion(state->server, databaseId, statusCode);
      }

      char const* properties = reinterpret_cast<char const*>(m) + sizeof(database_create_marker_t);
      TRI_json_t* json = triagens::basics::JsonHelper::fromString(properties);
        
      if (! TRI_IsObjectJson(json)) {
        if (json != nullptr) {
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
        }
        LOG_WARNING("cannot unpack database properties for database %llu", (unsigned long long) databaseId);
        return state->canContinue();
      }

      TRI_json_t const* nameValue = TRI_LookupObjectJson(json, "name");

      if (! TRI_IsStringJson(nameValue)) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
        LOG_WARNING("cannot unpack database properties for database %llu", (unsigned long long) databaseId);
        return state->canContinue();
      }

      std::string nameString(nameValue->_value._string.data);
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

      // remove already existing database with same name
      vocbase = TRI_LookupDatabaseByNameServer(state->server, nameString.c_str());

      if (vocbase != nullptr) {
        TRI_voc_tick_t otherId = vocbase->_id;

        state->releaseDatabase(otherId);
        int statusCode = TRI_DropDatabaseServer(state->server, nameString.c_str(), false, false);
        WaitForDeletion(state->server, otherId, statusCode);
      }

      TRI_vocbase_defaults_t defaults;
      TRI_GetDatabaseDefaultsServer(state->server, &defaults);
      
      // fake transaction to satisfy assertions
      triagens::arango::TransactionBase trx(true); 

      vocbase = nullptr;
      WaitForDeletion(state->server, databaseId, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
      int res = TRI_CreateDatabaseServer(state->server, databaseId, nameString.c_str(), &defaults, &vocbase, false);

      if (res != TRI_ERROR_NO_ERROR) {
        LOG_WARNING("cannot create database %llu: %s", (unsigned long long) databaseId, TRI_errno_string(res));
        return state->canContinue();
      }
      break;
    }


    // -----------------------------------------------------------------------------
    // drop operations
    // -----------------------------------------------------------------------------
    
    case TRI_WAL_MARKER_DROP_INDEX: {
      index_drop_marker_t const* m = reinterpret_cast<index_drop_marker_t const*>(marker);
      TRI_voc_cid_t collectionId = m->_collectionId;
      TRI_voc_tick_t databaseId  = m->_databaseId;
      TRI_idx_iid_t indexId      = m->_indexId;

      if (state->isDropped(databaseId, collectionId)) {
        return true;
      }
      
      TRI_vocbase_t* vocbase = state->useDatabase(databaseId);

      if (vocbase == nullptr) {
        // if the underlying database is gone, we can go on
        LOG_TRACE("cannot open database %llu", (unsigned long long) databaseId);
        return true;
      }
            
      TRI_document_collection_t* document = state->getCollection(databaseId, collectionId);
      if (document == nullptr) {
        // if the underlying collection gone, we can go on
        return true;
      }
      
      // fake transaction to satisfy assertions
      triagens::arango::TransactionBase trx(true); 

      // ignore any potential error returned by this call
      TRI_DropIndexDocumentCollection(document, indexId, false);
      TRI_RemoveFileIndexCollection(document, indexId);

      // additionally remove the index file
      std::string collectionDirectory = GetCollectionDirectory(vocbase, collectionId);
      char* idString = TRI_StringUInt64(indexId);
      char* indexName = TRI_Concatenate3String("index-", idString, ".json");
      TRI_FreeString(TRI_CORE_MEM_ZONE, idString);
      char* filename = TRI_Concatenate2File(collectionDirectory.c_str(), indexName);
      TRI_FreeString(TRI_CORE_MEM_ZONE, indexName);
      
      TRI_UnlinkFile(filename);
      TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
      break;
    }


    case TRI_WAL_MARKER_DROP_COLLECTION: {
      collection_drop_marker_t const* m = reinterpret_cast<collection_drop_marker_t const*>(marker);
      TRI_voc_cid_t collectionId = m->_collectionId;
      TRI_voc_tick_t databaseId  = m->_databaseId;
      
      // insert the drop marker 
      state->droppedCollections.insert(collectionId);

      TRI_vocbase_t* vocbase = state->useDatabase(databaseId);
      if (vocbase == nullptr) {
        // database already deleted - do nothing
        return true;
      }

      // ignore any potential error returned by this call
      TRI_vocbase_col_t* collection = state->releaseCollection(collectionId);

      if (collection == nullptr) {
        collection = TRI_LookupCollectionByIdVocBase(vocbase, collectionId);
      }

      if (collection != nullptr) {
        // fake transaction to satisfy assertions
        triagens::arango::TransactionBase trx(true); 

        int statusCode = TRI_DropCollectionVocBase(vocbase, collection, false);
        WaitForDeletion(vocbase, collectionId, statusCode);
      }
      break;
    }


    case TRI_WAL_MARKER_DROP_DATABASE: {
      database_drop_marker_t const* m = reinterpret_cast<database_drop_marker_t const*>(marker);
      TRI_voc_tick_t databaseId = m->_databaseId;
      
      // insert the drop marker 
      state->droppedDatabases.insert(databaseId);
      TRI_vocbase_t* vocbase = state->releaseDatabase(databaseId);

      if (vocbase != nullptr) {
        // fake transaction to satisfy assertions
        triagens::arango::TransactionBase trx(true); 
  
        // ignore any potential error returned by this call
        TRI_DropByIdDatabaseServer(state->server, databaseId, false, false);
      }
      break;
    }
    
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replay a single logfile
////////////////////////////////////////////////////////////////////////////////
    
int RecoverState::replayLogfile (Logfile* logfile) {
  LOG_INFO("replaying WAL logfile '%s'", logfile->filename().c_str());

  if (! TRI_IterateDatafile(logfile->df(), &RecoverState::ReplayMarker, static_cast<void*>(this))) {
    LOG_WARNING("WAL inspection failed when scanning logfile '%s'", logfile->filename().c_str());
    return TRI_ERROR_ARANGO_RECOVERY;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replay all logfiles
////////////////////////////////////////////////////////////////////////////////
    
int RecoverState::replayLogfiles () {
  droppedCollections.clear();
  droppedDatabases.clear();

  for (auto& it : logfilesToProcess) {
    TRI_ASSERT(it != nullptr);
    int res = replayLogfile(it);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief abort open transactions
////////////////////////////////////////////////////////////////////////////////

int RecoverState::abortOpenTransactions () {
  if (failedTransactions.empty()) {
    // nothing to do
    return TRI_ERROR_NO_ERROR;
  }

  LOG_TRACE("writing abort markers for still open transactions");
  int res = TRI_ERROR_NO_ERROR;

  try {
    // write abort markers for all transactions
    for (auto it = failedTransactions.begin(); it != failedTransactions.end(); ++it) {
      TRI_voc_tid_t transactionId = (*it).first;

      if ((*it).second.second) {
        // already handled
        continue;
      }

      TRI_voc_tick_t databaseId = (*it).second.first;

      AbortTransactionMarker marker(databaseId, transactionId);
      SlotInfoCopy slotInfo = triagens::wal::LogfileManager::instance()->allocateAndWrite(marker.mem(), marker.size(), false);
    
      if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
      }
    }
  }
  catch (triagens::arango::Exception const& ex) {
    res = ex.code();
  }
  catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove all empty logfiles found during logfile inspection
////////////////////////////////////////////////////////////////////////////////

int RecoverState::removeEmptyLogfiles () {
  if (emptyLogfiles.empty()) {
    return TRI_ERROR_NO_ERROR;
  }

  LOG_TRACE("removing empty WAL logfiles");

  for (auto it = emptyLogfiles.begin(); it != emptyLogfiles.end(); ++it) {
    auto filename = (*it);

    if (basics::FileUtils::remove(filename, 0)) {
      LOG_TRACE("removing empty WAL logfile '%s'", filename.c_str());
    }
  }

  emptyLogfiles.clear();

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fill the secondary indexes of all collections used in recovery
////////////////////////////////////////////////////////////////////////////////

int RecoverState::fillIndexes () {
  // fake transaction to allow populating the secondary indexes
  triagens::arango::TransactionBase trx(true);

  // release all collections
  for (auto it = openedCollections.begin(); it != openedCollections.end(); ++it) {
    TRI_vocbase_col_t* collection = (*it).second;

    TRI_document_collection_t* document = collection->_collection;

    TRI_ASSERT(document != nullptr);

    // activate secondary indexes
    document->useSecondaryIndexes(true);

    int res = TRI_FillIndexesDocumentCollection(document);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }
 
  return TRI_ERROR_NO_ERROR; 
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
