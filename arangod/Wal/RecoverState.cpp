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

#include "RecoverState.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/conversions.h"
#include "Basics/files.h"
#include "Basics/Exceptions.h"
#include "Basics/memory-map.h"
#include "Basics/tri-strings.h"
#include "Basics/VelocyPackHelper.h"
#include "RestServer/DatabaseFeature.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "VocBase/DatafileHelper.h"
#include "VocBase/LogicalCollection.h"
#include "Wal/LogfileManager.h"
#include "Wal/Slots.h"

#include "Indexes/RocksDBFeature.h"

#include <velocypack/Collection.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::wal;

/// @brief convert a number slice into its numeric equivalent
template <typename T>
static inline T NumericValue(VPackSlice const& slice, char const* attribute) {
  if (!slice.isObject()) {
    LOG(ERR) << "invalid value type when looking for attribute '" << attribute << "': expecting object";
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }
  VPackSlice v = slice.get(attribute);
  if (v.isString()) {
    return static_cast<T>(std::stoull(v.copyString()));
  }
  if (v.isNumber()) {
    return v.getNumber<T>();
  }
  
  LOG(ERR) << "invalid value for attribute '" << attribute << "'";
  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

/// @brief creates the recover state
RecoverState::RecoverState(bool ignoreRecoveryErrors)
    : databaseFeature(nullptr),
      failedTransactions(),
      lastTick(0),
      logfilesToProcess(),
      openedCollections(),
      openedDatabases(),
      emptyLogfiles(),
      ignoreRecoveryErrors(ignoreRecoveryErrors),
      errorCount(0),
      maxRevisionId(0),
      lastDatabaseId(0),
      lastCollectionId(0) {
        
  databaseFeature = application_features::ApplicationServer::getFeature<DatabaseFeature>("Database");
}

/// @brief destroys the recover state
RecoverState::~RecoverState() { releaseResources(); }

/// @brief release opened collections and databases so they can be shut down
/// etc.
void RecoverState::releaseResources() {
  // release all collections
  for (auto it = openedCollections.begin(); it != openedCollections.end();
       ++it) {
    arangodb::LogicalCollection* collection = it->second;
    collection->vocbase()->releaseCollection(collection);
  }

  openedCollections.clear();

  // release all databases
  for (auto it = openedDatabases.begin(); it != openedDatabases.end(); ++it) {
    (*it).second->release();
  }

  openedDatabases.clear();
}

/// @brief gets a database (and inserts it into the cache if not in it)
TRI_vocbase_t* RecoverState::useDatabase(TRI_voc_tick_t databaseId) {
  auto it = openedDatabases.find(databaseId);

  if (it != openedDatabases.end()) {
    return (*it).second;
  }

  TRI_vocbase_t* vocbase = databaseFeature->useDatabase(databaseId);

  if (vocbase == nullptr) {
    return nullptr;
  }

  openedDatabases.emplace(databaseId, vocbase);
  return vocbase;
}

/// @brief release a database (so it can be dropped)
TRI_vocbase_t* RecoverState::releaseDatabase(TRI_voc_tick_t databaseId) {
  auto it = openedDatabases.find(databaseId);

  if (it == openedDatabases.end()) {
    return nullptr;
  }

  TRI_vocbase_t* vocbase = (*it).second;

  TRI_ASSERT(vocbase != nullptr);

  // release all collections we ourselves have opened for this database
  auto it2 = openedCollections.begin();

  while (it2 != openedCollections.end()) {
    arangodb::LogicalCollection* collection = it2->second;

    TRI_ASSERT(collection != nullptr);

    if (collection->vocbase()->id() == databaseId) {
      // correct database, now release the collection
      TRI_ASSERT(vocbase == collection->vocbase());
      vocbase->releaseCollection(collection);
      // get new iterator position
      it2 = openedCollections.erase(it2);
    } else {
      // collection not found, advance in the loop
      ++it2;
    }
  }

  vocbase->release();
  openedDatabases.erase(databaseId);

  return vocbase;
}

/// @brief release a collection (so it can be dropped)
arangodb::LogicalCollection* RecoverState::releaseCollection(TRI_voc_cid_t collectionId) {
  auto it = openedCollections.find(collectionId);

  if (it == openedCollections.end()) {
    return nullptr;
  }

  arangodb::LogicalCollection* collection = it->second;

  TRI_ASSERT(collection != nullptr);
  collection->vocbase()->releaseCollection(collection);
  openedCollections.erase(collectionId);

  return collection;
}

/// @brief gets a collection (and inserts it into the cache if not in it)
arangodb::LogicalCollection* RecoverState::useCollection(
    TRI_vocbase_t* vocbase, TRI_voc_cid_t collectionId, int& res) {
  auto it = openedCollections.find(collectionId);

  if (it != openedCollections.end()) {
    res = TRI_ERROR_NO_ERROR;
    return (*it).second;
  }

  TRI_set_errno(TRI_ERROR_NO_ERROR);
  TRI_vocbase_col_status_e status;  // ignored here
  arangodb::LogicalCollection* collection = vocbase->useCollection(collectionId, status);

  if (collection == nullptr) {
    res = TRI_errno();

    if (res == TRI_ERROR_ARANGO_CORRUPTED_COLLECTION) {
      LOG(WARN) << "unable to open collection " << collectionId << ". Please check the logs above for errors.";
    }

    return nullptr;
  }

  // disable secondary indexes for the moment
  collection->useSecondaryIndexes(false);

  openedCollections.emplace(collectionId, collection);
  res = TRI_ERROR_NO_ERROR;
  return collection;
}

/// @brief looks up a collection
/// the collection will be opened after this call and inserted into a local
/// cache for faster lookups
/// returns nullptr if the collection does not exist
LogicalCollection* RecoverState::getCollection(
    TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId) {
  TRI_vocbase_t* vocbase = useDatabase(databaseId);

  if (vocbase == nullptr) {
    LOG(TRACE) << "database " << databaseId << " not found";
    return nullptr;
  }

  int res;
  arangodb::LogicalCollection* collection = useCollection(vocbase, collectionId, res);

  if (collection == nullptr) {
    LOG(TRACE) << "collection " << collectionId << " of database " << databaseId << " not found";
    return nullptr;
  }
  return collection;
}

/// @brief executes a single operation inside a transaction
int RecoverState::executeSingleOperation(
    TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
    TRI_df_marker_t const* marker, TRI_voc_fid_t fid,
    std::function<int(SingleCollectionTransaction*, MarkerEnvelope*)> func) {
  // first find the correct database
  TRI_vocbase_t* vocbase = useDatabase(databaseId);

  if (vocbase == nullptr) {
    LOG(TRACE) << "database " << databaseId << " not found";
    return TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
  }

  int res;
  arangodb::LogicalCollection* collection = useCollection(vocbase, collectionId, res);

  if (collection == nullptr) {
    if (res == TRI_ERROR_ARANGO_CORRUPTED_COLLECTION) {
      return res;
    }

    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  TRI_voc_tick_t maxTick = collection->maxTick();
  if (marker->getTick() <= maxTick) {
    // already transferred this marker
    return TRI_ERROR_NO_ERROR;
  }

  res = TRI_ERROR_INTERNAL;

  try {
    SingleCollectionTransaction trx(arangodb::StandaloneTransactionContext::Create(vocbase), collectionId, TRI_TRANSACTION_WRITE);

    trx.addHint(TRI_TRANSACTION_HINT_SINGLE_OPERATION, false);
    trx.addHint(TRI_TRANSACTION_HINT_NO_BEGIN_MARKER, false);
    trx.addHint(TRI_TRANSACTION_HINT_NO_ABORT_MARKER, false);
    trx.addHint(TRI_TRANSACTION_HINT_NO_THROTTLING, false);
    trx.addHint(TRI_TRANSACTION_HINT_LOCK_NEVER, false);
    trx.addHint(TRI_TRANSACTION_HINT_RECOVERY, false); // to turn off waitForSync!

    res = trx.begin();

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    MarkerEnvelope envelope(marker, fid);

    // execute the operation
    res = func(&trx, &envelope);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    // commit the operation
    res = trx.commit();
  } catch (arangodb::basics::Exception const& ex) {
    LOG(ERR) << "caught exception during recovery of marker type " << TRI_NameMarkerDatafile(marker) << ": " << ex.what();
    res = ex.code();
  } catch (std::exception const& ex) {
    LOG(ERR) << "caught exception during recovery of marker type " << TRI_NameMarkerDatafile(marker) << ": " << ex.what();
    res = TRI_ERROR_INTERNAL;
  } catch (...) {
    LOG(ERR) << "caught unknown exception during recovery of marker type " << TRI_NameMarkerDatafile(marker);
    res = TRI_ERROR_INTERNAL;
  }

  return res;
}

/// @brief callback to handle one marker during recovery
/// this function only builds up state and does not change any data
bool RecoverState::InitialScanMarker(TRI_df_marker_t const* marker, void* data,
                                     TRI_datafile_t* datafile) {
  RecoverState* state = reinterpret_cast<RecoverState*>(data);

  TRI_ASSERT(marker != nullptr);

  // note the marker's tick
  TRI_voc_tick_t const tick = marker->getTick();

  TRI_ASSERT(tick >= state->lastTick);

  if (tick > state->lastTick) {
    state->lastTick = tick;
  }

  TRI_df_marker_type_t const type = marker->getType();

  switch (type) {
    case TRI_DF_MARKER_VPACK_DOCUMENT: {
      VPackSlice const payloadSlice(reinterpret_cast<char const*>(marker) + DatafileHelper::VPackOffset(type));
      if (payloadSlice.isObject()) {
        TRI_voc_rid_t revisionId = Transaction::extractRevFromDocument(payloadSlice);
        if (revisionId > state->maxRevisionId) {
          state->maxRevisionId = revisionId;
        }
      }
      break;
    }

    case TRI_DF_MARKER_VPACK_BEGIN_TRANSACTION: {
      // insert this transaction into the list of failed transactions
      // we do this because if we don't find a commit marker for this
      // transaction,
      // we'll have it in the failed list at the end of the scan and can ignore
      // it
      TRI_voc_tick_t const databaseId = DatafileHelper::DatabaseId(marker);
      TRI_voc_tid_t const tid = DatafileHelper::TransactionId(marker);
      state->failedTransactions.emplace(tid, std::make_pair(databaseId, false));
      break;
    }

    case TRI_DF_MARKER_VPACK_COMMIT_TRANSACTION: {
      // remove this transaction from the list of failed transactions
      TRI_voc_tid_t const tid = DatafileHelper::TransactionId(marker);
      state->failedTransactions.erase(tid);
      break;
    }

    case TRI_DF_MARKER_VPACK_ABORT_TRANSACTION: {
      // insert this transaction into the list of failed transactions
      TRI_voc_tick_t const databaseId = DatafileHelper::DatabaseId(marker);
      TRI_voc_tid_t const tid = DatafileHelper::TransactionId(marker);
      state->failedTransactions[tid] = std::make_pair(databaseId, true);
      break;
    }
    
    case TRI_DF_MARKER_VPACK_DROP_DATABASE: {
      // note that the database was dropped and doesn't need to be recovered
      TRI_voc_tick_t const databaseId = DatafileHelper::DatabaseId(marker);
      state->totalDroppedDatabases.emplace(databaseId);
      break;
    }

    case TRI_DF_MARKER_VPACK_DROP_COLLECTION: {
      // note that the collection was dropped and doesn't need to be recovered
      TRI_voc_cid_t const collectionId = DatafileHelper::CollectionId(marker);
      state->totalDroppedCollections.emplace(collectionId);
      break;
    }

    default: {
      // do nothing
    }
  }

  return true;
}

/// @brief callback to replay one marker during recovery
/// this function modifies indexes etc.
bool RecoverState::ReplayMarker(TRI_df_marker_t const* marker, void* data,
                                TRI_datafile_t* datafile) {
  RecoverState* state = reinterpret_cast<RecoverState*>(data);

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  LOG(TRACE) << "replaying marker of type " << TRI_NameMarkerDatafile(marker);
#endif
      
  TRI_df_marker_type_t const type = marker->getType();

  try {
    switch (type) {
      case TRI_DF_MARKER_PROLOGUE: {
        // simply note the last state
        TRI_voc_tick_t const databaseId = DatafileHelper::DatabaseId(marker);
        TRI_voc_cid_t const collectionId = DatafileHelper::CollectionId(marker);

        LOG(TRACE) << "found prologue marker. databaseId: " << databaseId << ", collectionId: " << collectionId;
        state->resetCollection(databaseId, collectionId);
        return true;
      }

      // -----------------------------------------------------------------------------
      // crud operations
      // -----------------------------------------------------------------------------

      case TRI_DF_MARKER_VPACK_DOCUMENT: {
        // re-insert the document/edge into the collection
        TRI_voc_tick_t const databaseId = state->lastDatabaseId; // from prologue
        TRI_voc_cid_t const collectionId = state->lastCollectionId; // from prologue
        
        if (state->isDropped(databaseId, collectionId)) {
          return true;
        }

        TRI_voc_tid_t const tid = DatafileHelper::TransactionId(marker);
        
        if (state->ignoreTransaction(tid)) {
          // transaction was aborted
          return true;
        }
        
        LOG(TRACE) << "found document marker. databaseId: " << databaseId << ", collectionId: " << collectionId << ", transactionId: " << tid;

        int res = state->executeSingleOperation(
            databaseId, collectionId, marker, datafile->fid(),
            [&](SingleCollectionTransaction* trx, MarkerEnvelope* envelope) -> int {
              if (trx->documentCollection()->isVolatile()) {
                return TRI_ERROR_NO_ERROR;
              }

              TRI_df_marker_t const* marker = static_cast<TRI_df_marker_t const*>(envelope->mem());

              std::string const collectionName = trx->documentCollection()->name();
              uint8_t const* ptr = reinterpret_cast<uint8_t const*>(marker) + DatafileHelper::VPackOffset(type);

              OperationOptions options;
              options.silent = true;
              options.recoveryMarker = envelope;
              options.isRestore = true;
              options.waitForSync = false;
              options.ignoreRevs = true;

              // try an insert first
              TRI_ASSERT(VPackSlice(ptr).isObject());
              OperationResult opRes = trx->insert(collectionName, VPackSlice(ptr), options);
              int res = opRes.code;

              if (res == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) {
                // document/edge already exists, now make it a replace
                opRes = trx->replace(collectionName, VPackSlice(ptr), options);
                res = opRes.code;
              }

              return res;
            });

        if (res != TRI_ERROR_NO_ERROR && res != TRI_ERROR_ARANGO_CONFLICT &&
            res != TRI_ERROR_ARANGO_DATABASE_NOT_FOUND &&
            res != TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
          LOG(WARN) << "unable to insert document in collection " << collectionId << " of database " << databaseId << ": " << TRI_errno_string(res);
          ++state->errorCount;
          return state->canContinue();
        }
        break;
      }

      case TRI_DF_MARKER_VPACK_REMOVE: {
        // re-apply the remove operation
        TRI_voc_tick_t const databaseId = state->lastDatabaseId; // from prologue
        TRI_voc_cid_t const collectionId = state->lastCollectionId; // from prologue

        TRI_ASSERT(databaseId > 0);
        TRI_ASSERT(collectionId > 0);

        if (state->isDropped(databaseId, collectionId)) {
          return true;
        }

        TRI_voc_tid_t const tid = DatafileHelper::TransactionId(marker);
        
        if (state->ignoreTransaction(tid)) {
          return true;
        }
        
        LOG(TRACE) << "found remove marker. databaseId: " << databaseId << ", collectionId: " << collectionId << ", transactionId: " << tid;

        int res = state->executeSingleOperation(
            databaseId, collectionId, marker, datafile->fid(),
            [&](SingleCollectionTransaction* trx, MarkerEnvelope* envelope) -> int {
              if (trx->documentCollection()->isVolatile()) {
                return TRI_ERROR_NO_ERROR;
              }
              
              TRI_df_marker_t const* marker = static_cast<TRI_df_marker_t const*>(envelope->mem());

              std::string const collectionName = trx->documentCollection()->name();
              uint8_t const* ptr = reinterpret_cast<uint8_t const*>(marker) + DatafileHelper::VPackOffset(type);

              OperationOptions options;
              options.silent = true;
              options.recoveryMarker = envelope;
              options.waitForSync = false;
              options.ignoreRevs = true;

              try {
                OperationResult opRes = trx->remove(collectionName, VPackSlice(ptr), options);
                if (opRes.code == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
                  // document to delete is not present. this error can be ignored
                  return TRI_ERROR_NO_ERROR;
                }
                return opRes.code;
              } catch (arangodb::basics::Exception const& ex) {
                if (ex.code() == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
                  // document to delete is not present. this error can be ignored
                  return TRI_ERROR_NO_ERROR;
                }
                return ex.code();
              }
              // should not get here... 
              return TRI_ERROR_INTERNAL;
            });

        if (res != TRI_ERROR_NO_ERROR && res != TRI_ERROR_ARANGO_CONFLICT &&
            res != TRI_ERROR_ARANGO_DATABASE_NOT_FOUND &&
            res != TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND &&
            res != TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
          LOG(WARN) << "unable to remove document in collection " << collectionId << " of database " << databaseId << ": " << TRI_errno_string(res);
          ++state->errorCount;
          return state->canContinue();
        }
        break;
      }

      // -----------------------------------------------------------------------------
      // ddl
      // -----------------------------------------------------------------------------

      case TRI_DF_MARKER_VPACK_RENAME_COLLECTION: {
        TRI_voc_tick_t const databaseId = DatafileHelper::DatabaseId(marker);
        TRI_voc_cid_t const collectionId = DatafileHelper::CollectionId(marker);
        VPackSlice const payloadSlice(reinterpret_cast<char const*>(marker) + DatafileHelper::VPackOffset(type));
        
        if (!payloadSlice.isObject()) {
          LOG(WARN) << "cannot rename collection: invalid marker";
          ++state->errorCount;
          return state->canContinue();
        }

        if (state->isDropped(databaseId)) {
          return true;
        }
        
        LOG(TRACE) << "found collection rename marker. databaseId: " << databaseId << ", collectionId: " << collectionId;

        TRI_vocbase_t* vocbase = state->useDatabase(databaseId);

        if (vocbase == nullptr) {
          // if the underlying database is gone, we can go on
          LOG(TRACE) << "cannot open database " << databaseId;
          return true;
        }

        arangodb::LogicalCollection* collection = state->releaseCollection(collectionId);

        if (collection == nullptr) {
          collection = vocbase->lookupCollection(collectionId);
        }

        if (collection == nullptr) {
          // if the underlying collection is gone, we can go on
          LOG(TRACE) << "cannot open collection " << collectionId;
          return true;
        }

        VPackSlice nameSlice = payloadSlice.get("name");
        if (!nameSlice.isString()) {
          LOG(WARN) << "cannot rename collection " << collectionId << " in database " << databaseId << ": name attribute is no string";
          ++state->errorCount;
          return state->canContinue();
        }
        std::string name = nameSlice.copyString();

        // check if other collection exist with target name
        arangodb::LogicalCollection* other = vocbase->lookupCollection(name);

        if (other != nullptr) {
          TRI_voc_cid_t otherCid = other->cid();
          state->releaseCollection(otherCid);
          vocbase->dropCollection(other, true, false);
        }

        int res = vocbase->renameCollection(collection, name, true, false);

        if (res != TRI_ERROR_NO_ERROR) {
          LOG(WARN) << "cannot rename collection " << collectionId << " in database " << databaseId << " to '" << name << "': " << TRI_errno_string(res);
          ++state->errorCount;
          return state->canContinue();
        }
        break;
      }

      case TRI_DF_MARKER_VPACK_CHANGE_COLLECTION: {
        TRI_voc_tick_t const databaseId = DatafileHelper::DatabaseId(marker);
        TRI_voc_cid_t const collectionId = DatafileHelper::CollectionId(marker);
        VPackSlice const payloadSlice(reinterpret_cast<char const*>(marker) + DatafileHelper::VPackOffset(type));
        
        if (!payloadSlice.isObject()) {
          LOG(WARN) << "cannot change properties of collection: invalid marker";
          ++state->errorCount;
          return state->canContinue();
        }

        if (state->isDropped(databaseId)) {
          return true;
        }
        
        LOG(TRACE) << "found collection change marker. databaseId: " << databaseId << ", collectionId: " << collectionId;

        TRI_vocbase_t* vocbase = state->useDatabase(databaseId);

        if (vocbase == nullptr) {
          // if the underlying database is gone, we can go on
          LOG(TRACE) << "cannot open database " << databaseId;
          return true;
        }

        LogicalCollection* collection =
            state->getCollection(databaseId, collectionId);

        if (collection == nullptr) {
          // if the underlying collection is gone, we can go on
          LOG(TRACE) << "cannot change properties of collection " << collectionId << " in database " << databaseId << ": " << TRI_errno_string(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
          return true;
        }

        // turn off sync temporarily if the database or collection are going to be
        // dropped later
        bool const forceSync = state->willBeDropped(databaseId, collectionId);
        int res = collection->update(payloadSlice, forceSync);

        if (res != TRI_ERROR_NO_ERROR) {
          LOG(WARN) << "cannot change collection properties for collection " << collectionId << " in database " << databaseId << ": " << TRI_errno_string(res);
          ++state->errorCount;
          return state->canContinue();
        }

        break;
      }

      case TRI_DF_MARKER_VPACK_CREATE_INDEX: {
        TRI_voc_tick_t const databaseId = DatafileHelper::DatabaseId(marker);
        TRI_voc_cid_t const collectionId = DatafileHelper::CollectionId(marker);
        VPackSlice const payloadSlice(reinterpret_cast<char const*>(marker) + DatafileHelper::VPackOffset(type));
        
        if (!payloadSlice.isObject()) {
          LOG(WARN) << "cannot create index for collection: invalid marker";
          ++state->errorCount;
          return state->canContinue();
        }
        
        TRI_idx_iid_t indexId = NumericValue<TRI_idx_iid_t>(payloadSlice, "id");

        if (state->isDropped(databaseId, collectionId)) {
          return true;
        }
        
        LOG(TRACE) << "found create index marker. databaseId: " << databaseId << ", collectionId: " << collectionId;

        TRI_vocbase_t* vocbase = state->useDatabase(databaseId);

        if (vocbase == nullptr) {
          // if the underlying database is gone, we can go on
          LOG(TRACE) << "cannot create index for collection " << collectionId << " in database " << databaseId << ": " << TRI_errno_string(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
          return true;
        }

        arangodb::LogicalCollection* col = vocbase->lookupCollection(collectionId);

        if (col == nullptr) {
          // if the underlying collection gone, we can go on
          LOG(TRACE) << "cannot create index for collection " << collectionId << " in database " << databaseId << ": " << TRI_errno_string(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
          return true;
        }

        RocksDBFeature::dropIndex(databaseId, collectionId, indexId);

        std::string const indexName("index-" + std::to_string(indexId) + ".json");
        std::string const filename(arangodb::basics::FileUtils::buildFilename(col->path(), indexName));

        bool const forceSync = state->willBeDropped(databaseId, collectionId);
        bool ok = arangodb::basics::VelocyPackHelper::velocyPackToFile(
            filename, payloadSlice, forceSync);

        if (!ok) {
          LOG(WARN) << "cannot create index " << indexId << ", collection " << collectionId << " in database " << databaseId;
          ++state->errorCount;
          return state->canContinue();
        } else {
          arangodb::SingleCollectionTransaction trx(
              arangodb::StandaloneTransactionContext::Create(vocbase),
              collectionId, TRI_TRANSACTION_WRITE);
          std::shared_ptr<arangodb::Index> unused;
          int res = col->restoreIndex(&trx, payloadSlice, unused);

          if (res != TRI_ERROR_NO_ERROR) {
            LOG(WARN) << "cannot create index " << indexId << ", collection " << collectionId << " in database " << databaseId;
            ++state->errorCount;
            return state->canContinue();
          }
        }

        break;
      }

      case TRI_DF_MARKER_VPACK_CREATE_COLLECTION: {
        TRI_voc_tick_t const databaseId = DatafileHelper::DatabaseId(marker);
        TRI_voc_cid_t const collectionId = DatafileHelper::CollectionId(marker);
        VPackSlice const payloadSlice(reinterpret_cast<char const*>(marker) + DatafileHelper::VPackOffset(type));
        
        if (!payloadSlice.isObject()) {
          LOG(WARN) << "cannot create collection: invalid marker";
          ++state->errorCount;
          return state->canContinue();
        }
        
        LOG(TRACE) << "found create collection marker. databaseId: " << databaseId << ", collectionId: " << collectionId;
        
        // remove the drop marker
        state->droppedCollections.erase(collectionId);

        if (state->isDropped(databaseId)) {
          return true;
        }

        TRI_vocbase_t* vocbase = state->useDatabase(databaseId);

        if (vocbase == nullptr) {
          // if the underlying database is gone, we can go on
          LOG(TRACE) << "cannot open database " << databaseId;
          return true;
        }

        arangodb::LogicalCollection* collection = state->releaseCollection(collectionId);

        if (collection == nullptr) {
          collection = vocbase->lookupCollection(collectionId);
        }
        
        if (collection != nullptr) {
          // drop an existing collection
          vocbase->dropCollection(collection, true, false);
        }

        RocksDBFeature::dropCollection(databaseId, collectionId);

        // check if there is another collection with the same name as the one that
        // we attempt to create
        VPackSlice const nameSlice = payloadSlice.get("name");
        std::string name = "";

        if (nameSlice.isString()) {
          name = nameSlice.copyString();
          collection = vocbase->lookupCollection(name);

          if (collection != nullptr) {  
            TRI_voc_cid_t otherCid = collection->cid();

            state->releaseCollection(otherCid);
            vocbase->dropCollection(collection, true, false);
          }
        } else {
          LOG(WARN) << "empty name attribute in create collection marker for collection " << collectionId << " and database " << databaseId;
          ++state->errorCount;
          return state->canContinue();
        }

        // fiddle "isSystem" value, which is not contained in the JSON file
        bool isSystemValue = false;
        if (!name.empty()) {
          isSystemValue = name[0] == '_';
        }

        VPackBuilder bx;
        bx.openObject();
        bx.add("isSystem", VPackValue(isSystemValue));
        bx.close();
        VPackSlice isSystem = bx.slice();
        VPackBuilder b2 = VPackCollection::merge(payloadSlice, isSystem, false);

        int res = TRI_ERROR_NO_ERROR;
        try {
          if (state->willBeDropped(collectionId)) {
            // in case we detect that this collection is going to be deleted anyway,
            // set the sync properties to false temporarily
            bool oldSync = state->databaseFeature->forceSyncProperties();
            state->databaseFeature->forceSyncProperties(false);
            collection = vocbase->createCollection(b2.slice(), collectionId, false);
            state->databaseFeature->forceSyncProperties(oldSync);
          } else {
            // collection will be kept
            collection = vocbase->createCollection(b2.slice(), collectionId, false);
          }
          TRI_ASSERT(collection != nullptr);
        } catch (basics::Exception const& ex) {
          res = ex.code();
        } catch (...) {
          res = TRI_ERROR_INTERNAL;
        }

        if (res != TRI_ERROR_NO_ERROR) {
          LOG(WARN) << "cannot create collection " << collectionId << " in database " << databaseId << ": " << TRI_errno_string(res);
          ++state->errorCount;
          return state->canContinue();
        }
        break;
      }

      case TRI_DF_MARKER_VPACK_CREATE_DATABASE: {
        TRI_voc_tick_t const databaseId = DatafileHelper::DatabaseId(marker);
        VPackSlice const payloadSlice(reinterpret_cast<char const*>(marker) + DatafileHelper::VPackOffset(type));
        
        if (!payloadSlice.isObject()) {
          LOG(WARN) << "cannot create database: invalid marker";
          ++state->errorCount;
          return state->canContinue();
        }
        
        LOG(TRACE) << "found create database marker. databaseId: " << databaseId;
        
        // remove the drop marker
        state->droppedDatabases.erase(databaseId);
        TRI_vocbase_t* vocbase = state->releaseDatabase(databaseId);

        if (vocbase != nullptr) {
          // remove already existing database
          // TODO: how to signal a dropDatabase failure here?
          state->databaseFeature->dropDatabase(databaseId, false, true, false);
        }

        VPackSlice const nameSlice = payloadSlice.get("name");

        if (!nameSlice.isString()) {
          LOG(WARN) << "cannot unpack database properties for database " << databaseId;
          ++state->errorCount;
          return state->canContinue();
        }

        std::string nameString = nameSlice.copyString();

        // remove already existing database with same name
        vocbase = state->databaseFeature->lookupDatabase(nameString);

        if (vocbase != nullptr) {
          TRI_voc_tick_t otherId = vocbase->id();

          state->releaseDatabase(otherId);
          // TODO: how to signal a dropDatabase failure here?
          state->databaseFeature->dropDatabase(nameString, false, true, false);
        }

        vocbase = nullptr;
        /* TODO: check what TRI_ERROR_ARANGO_DATABASE_NOT_FOUND means here 
        WaitForDeletion(state->server, databaseId,
                        TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
        */
        int res = state->databaseFeature->createDatabase(databaseId, nameString, false, vocbase);

        if (res != TRI_ERROR_NO_ERROR) {
          LOG(WARN) << "cannot create database " << databaseId << ": " << TRI_errno_string(res);
          ++state->errorCount;
          return state->canContinue();
        }

        RocksDBFeature::dropDatabase(databaseId);
        break;
      }

      case TRI_DF_MARKER_VPACK_DROP_INDEX: {
        TRI_voc_tick_t const databaseId = DatafileHelper::DatabaseId(marker);
        TRI_voc_cid_t const collectionId = DatafileHelper::CollectionId(marker);
        VPackSlice const payloadSlice(reinterpret_cast<char const*>(marker) + DatafileHelper::VPackOffset(type));
        
        if (!payloadSlice.isObject()) {
          LOG(WARN) << "cannot drop index for collection: invalid marker";
          ++state->errorCount;
          return state->canContinue();
        }
        
        TRI_idx_iid_t indexId = NumericValue<TRI_idx_iid_t>(payloadSlice, "id");
        
        LOG(TRACE) << "found drop index marker. databaseId: " << databaseId << ", collectionId: " << collectionId << ", indexId: " << indexId;

        if (state->isDropped(databaseId, collectionId)) {
          return true;
        }

        TRI_vocbase_t* vocbase = state->useDatabase(databaseId);

        if (vocbase == nullptr) {
          // if the underlying database is gone, we can go on
          LOG(TRACE) << "cannot open database " << databaseId;
          return true;
        }

        arangodb::LogicalCollection* col = vocbase->lookupCollection(collectionId);

        if (col == nullptr) {
          // if the underlying collection gone, we can go on
          return true;
        }

        // ignore any potential error returned by this call
        col->dropIndex(indexId, false);

        RocksDBFeature::dropIndex(databaseId, collectionId, indexId);

        // additionally remove the index file
        std::string const indexName("index-" + std::to_string(indexId) + ".json");
        std::string const filename(arangodb::basics::FileUtils::buildFilename(col->path(), indexName));

        TRI_UnlinkFile(filename.c_str());
        break;
      }

      case TRI_DF_MARKER_VPACK_DROP_COLLECTION: {
        TRI_voc_tick_t const databaseId = DatafileHelper::DatabaseId(marker);
        TRI_voc_cid_t const collectionId = DatafileHelper::CollectionId(marker);
        
        // insert the drop marker
        state->droppedCollections.emplace(collectionId);
        
        LOG(TRACE) << "found drop collection marker. databaseId: " << databaseId << ", collectionId: " << collectionId;

        TRI_vocbase_t* vocbase = state->useDatabase(databaseId);

        if (vocbase == nullptr) {
          // database already deleted - do nothing
          return true;
        }

        // ignore any potential error returned by this call
        arangodb::LogicalCollection* collection = state->releaseCollection(collectionId);

        if (collection == nullptr) {
          collection = vocbase->lookupCollection(collectionId);
        }

        if (collection != nullptr) {
          vocbase->dropCollection(collection, true, false);
        }
        RocksDBFeature::dropCollection(databaseId, collectionId);
        break;
      }

      case TRI_DF_MARKER_VPACK_DROP_DATABASE: {
        TRI_voc_tick_t const databaseId = DatafileHelper::DatabaseId(marker);

        // insert the drop marker
        state->droppedDatabases.emplace(databaseId);
        
        LOG(TRACE) << "found drop database marker. databaseId: " << databaseId;

        TRI_vocbase_t* vocbase = state->releaseDatabase(databaseId);

        if (vocbase != nullptr) {
          // ignore any potential error returned by this call
          state->databaseFeature->dropDatabase(databaseId, false, true, false);
        }

        RocksDBFeature::dropDatabase(databaseId);
        break;
      }
      
      case TRI_DF_MARKER_HEADER: 
      case TRI_DF_MARKER_COL_HEADER: 
      case TRI_DF_MARKER_FOOTER: {
        // new datafile or end of datafile. forget state!
        state->resetCollection();
        return true;
      }

      default: {
        // do nothing
      }
    }

    return true;
  } catch (std::exception const& ex) {
    LOG(WARN) << "cannot replay marker: " << ex.what();
    ++state->errorCount;
    return state->canContinue();
  } catch (...) {
    LOG(WARN) << "cannot replay marker";
    ++state->errorCount;
    return state->canContinue();
  }
}

/// @brief replay a single logfile
int RecoverState::replayLogfile(Logfile* logfile, int number) {
  std::string const logfileName = logfile->filename();

  int const n = static_cast<int>(logfilesToProcess.size());

  LOG(INFO) << "replaying WAL logfile '" << logfileName << "' (" << (number + 1) << " of " << n << ")";

  TRI_datafile_t* df = logfile->df();

  // Advise on sequential use:
  df->sequentialAccess();
  df->willNeed();

  if (!TRI_IterateDatafile(df, &RecoverState::ReplayMarker, static_cast<void*>(this))) {
    LOG(WARN) << "WAL inspection failed when scanning logfile '" << logfileName << "'";
    return TRI_ERROR_ARANGO_RECOVERY;
  }

  // Advise on random access use:
  df->randomAccess();

  return TRI_ERROR_NO_ERROR;
}

/// @brief replay all logfiles
int RecoverState::replayLogfiles() {
  droppedCollections.clear();
  droppedDatabases.clear();

  int i = 0;
  for (auto& it : logfilesToProcess) {
    TRI_ASSERT(it != nullptr);
    int res = replayLogfile(it, i++);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief abort open transactions
int RecoverState::abortOpenTransactions() {
  if (failedTransactions.empty()) {
    // nothing to do
    return TRI_ERROR_NO_ERROR;
  }

  LOG(TRACE) << "writing abort markers for still open transactions";
  int res = TRI_ERROR_NO_ERROR;
      
  VPackBuilder builder;

  try {
    // write abort markers for all transactions
    for (auto it = failedTransactions.begin(); it != failedTransactions.end();
         ++it) {
      TRI_voc_tid_t transactionId = (*it).first;

      if ((*it).second.second) {
        // already handled
        continue;
      }

      TRI_voc_tick_t databaseId = (*it).second.first;
     
      TransactionMarker marker(TRI_DF_MARKER_VPACK_ABORT_TRANSACTION, databaseId, transactionId);
      SlotInfoCopy slotInfo =
          arangodb::wal::LogfileManager::instance()->allocateAndWrite(marker, false);

      if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
      }

      // recycle builder
      builder.clear();
    }
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  return res;
}

/// @brief remove all empty logfiles found during logfile inspection
int RecoverState::removeEmptyLogfiles() {
  if (emptyLogfiles.empty()) {
    return TRI_ERROR_NO_ERROR;
  }

  LOG(TRACE) << "removing empty WAL logfiles";

  for (auto it = emptyLogfiles.begin(); it != emptyLogfiles.end(); ++it) {
    auto filename = (*it);

    if (basics::FileUtils::remove(filename, 0)) {
      LOG(TRACE) << "removing empty WAL logfile '" << filename << "'";
    }
  }

  emptyLogfiles.clear();

  return TRI_ERROR_NO_ERROR;
}

/// @brief fill the secondary indexes of all collections used in recovery
int RecoverState::fillIndexes() {
  // release all collections
  for (auto it = openedCollections.begin(); it != openedCollections.end();
       ++it) {
    arangodb::LogicalCollection* collection = (*it).second;

    // activate secondary indexes
    collection->useSecondaryIndexes(true);

    arangodb::SingleCollectionTransaction trx(
        arangodb::StandaloneTransactionContext::Create(collection->vocbase()),
        collection->cid(), TRI_TRANSACTION_WRITE);

    int res = collection->fillIndexes(&trx);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  return TRI_ERROR_NO_ERROR;
}
