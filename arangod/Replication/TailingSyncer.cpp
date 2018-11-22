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

#include "TailingSyncer.h"
#include "Basics/Exceptions.h"
#include "Basics/ReadLocker.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Logger/Logger.h"
#include "Rest/HttpRequest.h"
#include "Replication/InitialSyncer.h"
#include "Replication/ReplicationApplier.h"
#include "Replication/ReplicationTransaction.h"
#include "RestServer/DatabaseFeature.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Hints.h"
#include "Utils/CollectionGuard.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"
#include "VocBase/Methods/Databases.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::rest;

/// @brief base url of the replication API
std::string const TailingSyncer::WalAccessUrl = "/_api/wal";

TailingSyncer::TailingSyncer(ReplicationApplier* applier,
                             ReplicationApplierConfiguration const& configuration,
                             TRI_voc_tick_t initialTick,
                             bool useTick, TRI_voc_tick_t barrierId)
    : Syncer(configuration),
      _applier(applier),
      _hasWrittenState(false),
      _initialTick(initialTick),
      _usersModified(false),
      _useTick(useTick),
      _requireFromPresent(configuration._requireFromPresent),
      _supportsSingleOperations(false),
      _ignoreRenameCreateDrop(false),
      _ignoreDatabaseMarkers(true) {

  if (barrierId > 0) {
    _barrierId = barrierId;
    _barrierUpdateTime = TRI_microtime();
  }

  // FIXME: move this into engine code
  std::string engineName = EngineSelectorFeature::ENGINE->typeName();
  _supportsSingleOperations = (engineName == "mmfiles");
}

TailingSyncer::~TailingSyncer() { abortOngoingTransactions(); }

/// @brief decide based on _masterInfo which api to use
///        GlobalTailingSyncer should overwrite this probably
std::string TailingSyncer::tailingBaseUrl(std::string const& cc) {
  bool act32 = simulate32Client();
  std::string const& base = act32 ? Syncer::ReplicationUrl : TailingSyncer::WalAccessUrl;
  if (act32) { // fallback pre 3.3
    if (cc == "tail") {
      return base + "/logger-follow?";
    } else if (cc == "open-transactions") {
      return base + "/determine-open-transactions?";
    }
    // should not be used for anything else
    TRI_ASSERT(false);
  }
  return base + "/" + cc + "?";
}

/// @brief set the applier progress
void TailingSyncer::setProgress(std::string const& msg) {
  if (_configuration._verbose) {
    LOG_TOPIC(INFO, Logger::REPLICATION) << msg;
  } else {
    LOG_TOPIC(DEBUG, Logger::REPLICATION) << msg;
  }
  _applier->setProgress(msg);
}

/// @brief abort all ongoing transactions
void TailingSyncer::abortOngoingTransactions() {
  try {
    // abort all running transactions
    for (auto& it : _ongoingTransactions) {
      auto trx = it.second;

      if (trx != nullptr) {
        trx->abort();
        delete trx;
      }
    }

    _ongoingTransactions.clear();
  } catch (...) {
    // ignore errors here
  }
}

/// @brief whether or not a marker should be skipped
bool TailingSyncer::skipMarker(TRI_voc_tick_t firstRegularTick,
                               VPackSlice const& slice) {
  bool tooOld = false;
  std::string const tick = VelocyPackHelper::getStringValue(slice, "tick", "");

  if (!tick.empty()) {
    tooOld = (static_cast<TRI_voc_tick_t>(StringUtils::uint64(
                  tick.c_str(), tick.size())) < firstRegularTick);

    if (tooOld) {
      int typeValue = VelocyPackHelper::getNumericValue<int>(slice, "type", 0);
      // handle marker type
      TRI_replication_operation_e type =
          static_cast<TRI_replication_operation_e>(typeValue);

      if (type == REPLICATION_MARKER_DOCUMENT ||
          type == REPLICATION_MARKER_REMOVE ||
          type == REPLICATION_TRANSACTION_START ||
          type == REPLICATION_TRANSACTION_ABORT ||
          type == REPLICATION_TRANSACTION_COMMIT) {
        // read "tid" entry from marker
        std::string const id =
            VelocyPackHelper::getStringValue(slice, "tid", "");

        if (!id.empty()) {
          TRI_voc_tid_t tid = static_cast<TRI_voc_tid_t>(
              StringUtils::uint64(id.c_str(), id.size()));

          if (tid > 0 &&
              _ongoingTransactions.find(tid) != _ongoingTransactions.end()) {
            // must still use this marker as it belongs to a transaction we need
            // to finish
            tooOld = false;
          }
        }
      }
    }
  }

  if (tooOld) {
    return true;
  }
  
  // the transient applier state is just used for one shard / collection
  if (_configuration._restrictCollections.empty()) {
    return false;
  }
    
  if (_configuration._restrictType.empty() && _configuration._includeSystem) {
    return false;
  }
  
  VPackSlice const name = slice.get("cname");
  if (name.isString()) {
    return isExcludedCollection(name.copyString());
  }
  
  // call virtual method
  return skipMarker(slice);
}

/// @brief whether or not a collection should be excluded
bool TailingSyncer::isExcludedCollection(std::string const& masterName) const {
  if (masterName[0] == '_' && !_configuration._includeSystem) {
    // system collection
    return true;
  }

  auto const it = _configuration._restrictCollections.find(masterName);

  bool found = (it != _configuration._restrictCollections.end());

  if (_configuration._restrictType == "include" && !found) {
    // collection should not be included
    return true;
  } else if (_configuration._restrictType == "exclude" && found) {
    return true;
  }

  if (TRI_ExcludeCollectionReplication(masterName, true)) {
    return true;
  }

  return false;
}


/// @brief process db create or drop markers
Result TailingSyncer::processDBMarker(TRI_replication_operation_e type,
                                   velocypack::Slice const& slice) {
  TRI_ASSERT(!_ignoreDatabaseMarkers);
  
  // the new wal access protocol contains database names
  VPackSlice const nameSlice = slice.get("db");
  if (!nameSlice.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                                   "create database marker did not contain database");
  }
  std::string name = nameSlice.copyString();
  if (name.empty() || (name[0] >= '0' && name[0] <= '9')) {
    LOG_TOPIC(ERR, Logger::REPLICATION) << "invalid database name in log";
    return Result(TRI_ERROR_ARANGO_DATABASE_NAME_INVALID);
  }
  
  if (type == REPLICATION_DATABASE_CREATE) {
    VPackSlice const data = slice.get("data");
    if (!data.isObject()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                                     "create database marker did not contain data");
    }
    TRI_ASSERT(data.get("name") == nameSlice);

    TRI_vocbase_t* vocbase = DatabaseFeature::DATABASE->lookupDatabase(name);
    if (vocbase != nullptr && name != TRI_VOC_SYSTEM_DATABASE) {
      LOG_TOPIC(WARN, Logger::REPLICATION) << "seeing database creation marker "
        << "for an already existing db. Dropping db...";
      TRI_vocbase_t* system = DatabaseFeature::DATABASE->systemDatabase();
      TRI_ASSERT(system);
      Result res = methods::Databases::drop(system, name);
      if (res.fail()) {
        LOG_TOPIC(ERR, Logger::REPLICATION) << res.errorMessage();
        return res;
      }
    }
    
    VPackSlice users = VPackSlice::emptyArraySlice();
    Result res = methods::Databases::create(name, users,
                                            VPackSlice::emptyObjectSlice());
    return res;
  } else if (type == REPLICATION_DATABASE_DROP) {
    TRI_vocbase_t* vocbase = DatabaseFeature::DATABASE->lookupDatabase(name);
    if (vocbase != nullptr && name != TRI_VOC_SYSTEM_DATABASE) {
      TRI_vocbase_t* system = DatabaseFeature::DATABASE->systemDatabase();
      TRI_ASSERT(system != nullptr);
      // delete from cache by id and name
      _vocbases.erase(std::to_string(vocbase->id()));
      _vocbases.erase(name);
      
      Result res = methods::Databases::drop(system, name);

      if (res.fail()) {
        LOG_TOPIC(ERR, Logger::REPLICATION) << res.errorMessage();
      }
      return res;
    }
    return TRI_ERROR_NO_ERROR; // ignoring because it's idempotent
  }
  TRI_ASSERT(false);
  return Result(TRI_ERROR_INTERNAL); // unreachable
}

/// @brief inserts a document, based on the VelocyPack provided
Result TailingSyncer::processDocument(TRI_replication_operation_e type,
                                      VPackSlice const& slice) {
  TRI_vocbase_t* vocbase = resolveVocbase(slice);
  if (vocbase == nullptr) {
    return Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }
  arangodb::LogicalCollection* coll = resolveCollection(vocbase, slice);
  if (coll == nullptr) {
    return Result(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }
  
  bool isSystem = coll->isSystem();
  // extract "data"
  VPackSlice const doc = slice.get("data");

  if (!doc.isObject()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, "invalid document format");
  }

  // extract "key"
  VPackSlice const key = doc.get(StaticStrings::KeyString);

  if (!key.isString()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, "invalid document key format");
  }

  // extract "rev"
  VPackSlice const rev = doc.get(StaticStrings::RevString);

  if (!rev.isNone() && !rev.isString()) {
    // _rev is an optional attribute
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, "invalid document revision format");
  }

  _documentBuilder.clear();
  _documentBuilder.openObject();
  _documentBuilder.add(StaticStrings::KeyString, key);
  if (rev.isString()) {
    // _rev is an optional attribute
    _documentBuilder.add(StaticStrings::RevString, rev);
  }
  _documentBuilder.close();

  VPackSlice const old = _documentBuilder.slice();

  // extract "tid"
  std::string const transactionId =
      VelocyPackHelper::getStringValue(slice, "tid", "");
  TRI_voc_tid_t tid = 0;

  if (!transactionId.empty()) {
    // operation is part of a transaction
    tid = static_cast<TRI_voc_tid_t>(
        StringUtils::uint64(transactionId.c_str(), transactionId.size()));
  }

  if (tid > 0) { // part of a transaction
    auto it = _ongoingTransactions.find(tid);

    if (it == _ongoingTransactions.end()) {
      return Result(TRI_ERROR_REPLICATION_UNEXPECTED_TRANSACTION, std::string("unexpected transaction ") + StringUtils::itoa(tid));
    }

    auto trx = (*it).second;

    if (trx == nullptr) {
      return Result(TRI_ERROR_REPLICATION_UNEXPECTED_TRANSACTION, std::string("unexpected transaction ") + StringUtils::itoa(tid));
    }

    trx->addCollectionAtRuntime(coll->cid(), coll->name(), AccessMode::Type::EXCLUSIVE);
    Result r = applyCollectionDumpMarker(*trx, coll, type, old, doc);

    if (r.errorNumber() == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED && isSystem) {
      // ignore unique constraint violations for system collections
      r.reset();
    }
    if (r.ok() && coll->name() == TRI_COL_NAME_USERS) {
      _usersModified = true;
    }

    return r;
  }

  // standalone operation
  // update the apply tick for all standalone operations
  SingleCollectionTransaction trx(
      transaction::StandaloneContext::Create(vocbase), coll->cid(),
      AccessMode::Type::EXCLUSIVE);

  if (_supportsSingleOperations) {
    trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);
  }

  Result res = trx.begin();

  // fix error handling here when function returns result
  if (!res.ok()) {
    return Result(res.errorNumber(), std::string("unable to create replication transaction: ") + res.errorMessage());
  }

  std::string collectionName = trx.name();
    
  res = applyCollectionDumpMarker(trx, coll, type, old, doc);
  if (res.errorNumber() == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED && isSystem) {
    // ignore unique constraint violations for system collections
    res.reset();
  }
  
  // fix error handling here when function returns result
  if (res.ok()) {
    res = trx.commit();

    if (res.ok() && collectionName == TRI_COL_NAME_USERS) {
      _usersModified = true;
    }
  }

  return res;
}

/// @brief starts a transaction, based on the VelocyPack provided
Result TailingSyncer::startTransaction(VPackSlice const& slice) {
  // {"type":2200,"tid":"230920705812199", "database": "123", "collections":[{"cid":"230920700700391","operations":10}]}
  
  TRI_vocbase_t* vocbase = resolveVocbase(slice);
  if (vocbase == nullptr) {
    return Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }
  
  std::string const id = VelocyPackHelper::getStringValue(slice, "tid", "");
  if (id.empty()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, "transaction id value is missing in slice");
  }

  // transaction id
  // note: this is the remote transaction id!
  TRI_voc_tid_t tid =
      static_cast<TRI_voc_tid_t>(StringUtils::uint64(id.c_str(), id.size()));

  auto it = _ongoingTransactions.find(tid);

  if (it != _ongoingTransactions.end()) {
    // found a previous version of the same transaction - should not happen...
    auto trx = (*it).second;

    _ongoingTransactions.erase(tid);

    if (trx != nullptr) {
      // abort ongoing trx
      delete trx;
    }
  }

  TRI_ASSERT(tid > 0);

  LOG_TOPIC(TRACE, Logger::REPLICATION) << "starting replication transaction "
                                        << tid;

  auto trx = std::make_unique<ReplicationTransaction>(vocbase);
  Result res = trx->begin();

  if (res.ok()) {
    _ongoingTransactions[tid] = trx.get();
    trx.release();
  }

  return res;
}

/// @brief aborts a transaction, based on the VelocyPack provided
Result TailingSyncer::abortTransaction(VPackSlice const& slice) {
  // {"type":2201,"tid":"230920705812199","collections":[{"cid":"230920700700391","operations":10}]}
  std::string const id = VelocyPackHelper::getStringValue(slice, "tid", "");

  if (id.empty()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, "transaction id is missing in slice");
  }

  // transaction id
  // note: this is the remote transaction id!
  TRI_voc_tid_t const tid =
      static_cast<TRI_voc_tid_t>(StringUtils::uint64(id.c_str(), id.size()));

  auto it = _ongoingTransactions.find(tid);

  if (it == _ongoingTransactions.end()) {
    // invalid state, no transaction was started.
    return Result(TRI_ERROR_REPLICATION_UNEXPECTED_TRANSACTION);
  }

  TRI_ASSERT(tid > 0);

  LOG_TOPIC(TRACE, Logger::REPLICATION) << "aborting replication transaction "
                                        << tid;

  auto trx = (*it).second;
  _ongoingTransactions.erase(tid);

  if (trx != nullptr) {
    Result res = trx->abort();
    delete trx;

    return res;
  }

  return Result(TRI_ERROR_REPLICATION_UNEXPECTED_TRANSACTION);
}

/// @brief commits a transaction, based on the VelocyPack provided
Result TailingSyncer::commitTransaction(VPackSlice const& slice) {
  // {"type":2201,"tid":"230920705812199","collections":[{"cid":"230920700700391","operations":10}]}
  std::string const id = VelocyPackHelper::getStringValue(slice, "tid", "");

  if (id.empty()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, "transaction id is missing in slice");
  }

  // transaction id
  // note: this is the remote trasnaction id!
  TRI_voc_tid_t const tid =
      static_cast<TRI_voc_tid_t>(StringUtils::uint64(id.c_str(), id.size()));

  auto it = _ongoingTransactions.find(tid);

  if (it == _ongoingTransactions.end()) {
    // invalid state, no transaction was started.
    return Result(TRI_ERROR_REPLICATION_UNEXPECTED_TRANSACTION);
  }

  TRI_ASSERT(tid > 0);

  LOG_TOPIC(TRACE, Logger::REPLICATION) << "committing replication transaction "
                                        << tid;

  auto trx = (*it).second;
  _ongoingTransactions.erase(tid);

  if (trx != nullptr) {
    Result res = trx->commit();
    delete trx;

    return res;
  }

  return Result(TRI_ERROR_REPLICATION_UNEXPECTED_TRANSACTION);
}

/// @brief renames a collection, based on the VelocyPack provided
Result TailingSyncer::renameCollection(VPackSlice const& slice) {
  if (!slice.isObject()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, "rename slice is not an object");
  }

  VPackSlice collection = slice.get("collection");
  if (!collection.isObject()) {
    collection = slice.get("data");
  }

  if (!collection.isObject()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, "collection slice is not an object");
  }

  std::string const name =
      VelocyPackHelper::getStringValue(collection, "name", "");
  if (name.empty()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, "name attribute is missing in rename slice");
  }

  TRI_vocbase_t* vocbase = resolveVocbase(slice);
  if (vocbase == nullptr) {
    return Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }
  arangodb::LogicalCollection* col = nullptr;
  if (slice.hasKey("cuid")) {
    col = resolveCollection(vocbase, slice);
    if (col == nullptr) {
      return Result(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, "unknown cuid");
    }
  } else if (collection.hasKey("oldName")) {
    col = vocbase->lookupCollection(collection.get("oldName").copyString());
    if (col == nullptr) {
      return Result(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, "unknown old collection name");
    }
  } else {
    return Result(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, "unable to identify collection");
  }
  if (col->isSystem()) {
    LOG_TOPIC(WARN, Logger::REPLICATION) << "Renaming system collection " << col->name();
  }
  return Result(vocbase->renameCollection(col, name, true));
}

/// @brief changes the properties of a collection, based on the VelocyPack
/// provided
Result TailingSyncer::changeCollection(VPackSlice const& slice) {
  if (!slice.isObject()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, "collection slice is no object");
  }

  VPackSlice data = slice.get("collection");
  if (!data.isObject()) {
    data = slice.get("data");
  }
  
  if (!data.isObject()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, "data slice is no object");
  }

  VPackSlice d = data.get("deleted");
  bool const isDeleted = (d.isBool() && d.getBool());

  TRI_vocbase_t* vocbase = resolveVocbase(slice);
  if (vocbase == nullptr) {
    if (isDeleted) {
      // not a problem if a collection that is going to be deleted anyway
      // does not exist on slave
      return Result();
    }
    return Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }
  arangodb::LogicalCollection* col = resolveCollection(vocbase, slice);
  if (col == nullptr) {
    if (isDeleted) {
      // not a problem if a collection that is going to be deleted anyway
      // does not exist on slave
      return Result();
    }
    return Result(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }

  arangodb::CollectionGuard guard(vocbase, col);
  bool doSync = DatabaseFeature::DATABASE->forceSyncProperties();
  return guard.collection()->updateProperties(data, doSync);
}


/// @brief truncate a collections. Assumes no trx are running
Result TailingSyncer::truncateCollection(arangodb::velocypack::Slice const& slice) {
  if (!slice.isObject()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "collection slice is no object");
  }
  
  TRI_vocbase_t* vocbase = resolveVocbase(slice);
  if (vocbase == nullptr) {
    return Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }
  auto* col = resolveCollection(vocbase, slice);
  if (col == nullptr) {
    return Result(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }
  
  if (!_ongoingTransactions.empty()) {
    const char* msg = "Encountered truncate but still have ongoing transactions";
    LOG_TOPIC(ERR, Logger::REPLICATION) << msg;
    return Result(TRI_ERROR_REPLICATION_UNEXPECTED_TRANSACTION, msg);
  }
  
  SingleCollectionTransaction trx(transaction::StandaloneContext::Create(vocbase),
                                  col->cid(), AccessMode::Type::EXCLUSIVE);
  trx.addHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS);
  Result res = trx.begin();
  if (!res.ok()) {
    return res;
  }
  
  OperationOptions opts;
  OperationResult opRes = trx.truncate(col->name(), opts);
  
  if (opRes.fail()) {
    return opRes.result;
  }
  
  return trx.finish(opRes.result);
}

/// @brief apply a single marker from the continuous log
Result TailingSyncer::applyLogMarker(VPackSlice const& slice,
                                     TRI_voc_tick_t firstRegularTick,
                                     TRI_voc_tick_t& markerTick) {
  markerTick = 0;

  if (!slice.isObject()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, "marker slice is no object");
  }

  // fetch marker "type"
  int typeValue = VelocyPackHelper::getNumericValue<int>(slice, "type", 0);

  // fetch "tick"
  std::string const tick = VelocyPackHelper::getStringValue(slice, "tick", "");

  if (!tick.empty()) {
    markerTick = static_cast<TRI_voc_tick_t>(StringUtils::uint64(tick.c_str(), tick.size()));
  }

  // handle marker type
  TRI_replication_operation_e type = (TRI_replication_operation_e)typeValue;
  if (type == REPLICATION_MARKER_DOCUMENT ||
      type == REPLICATION_MARKER_REMOVE) {
    try {
      return processDocument(type, slice);
    } catch (basics::Exception const& ex) {
      return Result(ex.code(), ex.what());
    } catch (std::exception const& ex) {
      return Result(TRI_ERROR_INTERNAL, ex.what());
    } catch (...) {
      return Result(TRI_ERROR_INTERNAL, "unknown exception in processDocument");
    }
  }

  else if (type == REPLICATION_TRANSACTION_START) {
    return startTransaction(slice);
  }

  else if (type == REPLICATION_TRANSACTION_ABORT) {
    return abortTransaction(slice);
  }

  else if (type == REPLICATION_TRANSACTION_COMMIT) {
    return commitTransaction(slice);
  }

  else if (type == REPLICATION_COLLECTION_CREATE) {
    if (_ignoreRenameCreateDrop) {
      LOG_TOPIC(DEBUG, Logger::REPLICATION) << "Ignoring collection marker";
      return TRI_ERROR_NO_ERROR;
    }
    TRI_vocbase_t* vocbase = resolveVocbase(slice);
    if (vocbase == nullptr) {
      LOG_TOPIC(WARN, Logger::REPLICATION)
        << "Did not find database for " << slice.toJson();
      return Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    }
    if (slice.get("collection").isObject()) {
      return createCollection(vocbase, slice.get("collection"), nullptr);
    }
    return createCollection(vocbase, slice.get("data"), nullptr);
  }

  else if (type == REPLICATION_COLLECTION_DROP) {
    if (_ignoreRenameCreateDrop) {
      return TRI_ERROR_NO_ERROR;
    }
    return dropCollection(slice, false);
  }

  else if (type == REPLICATION_COLLECTION_RENAME) {
    if (_ignoreRenameCreateDrop) {
      // do not execute rename operations
      return Result();
    }
    return renameCollection(slice);
  }

  else if (type == REPLICATION_COLLECTION_CHANGE) {
    return changeCollection(slice);
  } else if (type == REPLICATION_COLLECTION_TRUNCATE) {
    return truncateCollection(slice);
  }

  else if (type == REPLICATION_INDEX_CREATE) {
    return createIndex(slice);
  }

  else if (type == REPLICATION_INDEX_DROP) {
    return dropIndex(slice);
  }

  else if (type == REPLICATION_VIEW_CREATE) {
    LOG_TOPIC(WARN, Logger::REPLICATION) << "views not supported in 3.3";
    return Result();
  }

  else if (type == REPLICATION_VIEW_DROP) {
    LOG_TOPIC(WARN, Logger::REPLICATION) << "views not supported in 3.3";
    return Result();
  }

  else if (type == REPLICATION_VIEW_CHANGE) {
    LOG_TOPIC(WARN, Logger::REPLICATION) << "views not supported in 3.3";
    return Result();
  }
  else if (type == REPLICATION_VIEW_RENAME) {
    LOG_TOPIC(WARN, Logger::REPLICATION) << "views not supported in 3.3";
    return Result();
  }
  
  else if (type == REPLICATION_DATABASE_CREATE ||
           type == REPLICATION_DATABASE_DROP) {
    if (_ignoreDatabaseMarkers) {
      LOG_TOPIC(DEBUG, Logger::REPLICATION) << "Ignoring database marker";
      return Result();
    }
    return processDBMarker(type, slice);
  }

  return Result(TRI_ERROR_REPLICATION_UNEXPECTED_MARKER, std::string("unexpected marker type ") + StringUtils::itoa(type));
}

/// @brief apply the data from the continuous log
Result TailingSyncer::applyLog(SimpleHttpResult* response,
                               TRI_voc_tick_t firstRegularTick,
                               uint64_t& processedMarkers,
                               uint64_t& ignoreCount) {
  // reload users if they were modified
  _usersModified = false;
  auto reloader = [this]() {
    if (_usersModified) {
      // reload users after initial dump
      reloadUsers();
      _usersModified = false;
    }
  };
  TRI_DEFER(reloader());

  StringBuffer& data = response->getBody();
  char const* p = data.begin();
  char const* end = p + data.length();

  // buffer must end with a NUL byte
  TRI_ASSERT(*end == '\0');

  // TODO: re-use a builder!
  auto builder = std::make_shared<VPackBuilder>();

  while (p < end) {
    char const* q = strchr(p, '\n');

    if (q == nullptr) {
      q = end;
    }

    char const* lineStart = p;
    size_t const lineLength = q - p;

    if (lineLength < 2) {
      // we are done
      return Result();
    }

    TRI_ASSERT(q <= end);

    processedMarkers++;

    builder->clear();
    try {
      VPackParser parser(builder);
      parser.parse(p, static_cast<size_t>(q - p));
    } catch (...) {
      // TODO: improve error reporting
      return Result(TRI_ERROR_OUT_OF_MEMORY);
    }

    p = q + 1;

    VPackSlice const slice = builder->slice();

    if (!slice.isObject()) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, "received invalid JSON data");
    }

    Result res;
    bool skipped;

    TRI_voc_tick_t markerTick = 0;

    if (skipMarker(firstRegularTick, slice)) {
      // entry is skipped
      res.reset();
      skipped = true;
    } else {
      res = applyLogMarker(slice, firstRegularTick, markerTick);
      skipped = false;
    }

    if (res.fail()) {
      // apply error
      std::string errorMsg = res.errorMessage();

      if (ignoreCount == 0) {
        if (lineLength > 1024) {
          errorMsg +=
              ", offending marker: " + std::string(lineStart, 1024) + "...";
        } else {
          errorMsg +=
              ", offending marker: " + std::string(lineStart, lineLength);
        }

        res.reset(res.errorNumber(), errorMsg);
        return res;
      }

      ignoreCount--;
      LOG_TOPIC(WARN, Logger::REPLICATION)
          << "ignoring replication error for database '" << _databaseName
          << "': " << errorMsg;
    }

    // update tick value
    //postApplyMarker(processedMarkers, skipped);
    WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock);

    if (markerTick >= firstRegularTick &&
        markerTick > _applier->_state._lastProcessedContinuousTick) {
      _applier->_state._lastProcessedContinuousTick = markerTick;
    }

    if (_applier->_state._lastProcessedContinuousTick >
        _applier->_state._lastAppliedContinuousTick) {
      _applier->_state._lastAppliedContinuousTick =
      _applier->_state._lastProcessedContinuousTick;
    }
    
    if (skipped) {
      ++_applier->_state._skippedOperations;
    } else if (_ongoingTransactions.empty()) {
      _applier->_state._safeResumeTick =
      _applier->_state._lastProcessedContinuousTick;
    }
  }

  // reached the end
  return Result();
}

/// @brief run method, performs continuous synchronization
/// catches exceptions
Result TailingSyncer::run() {
  try {
    return runInternal();
  } catch (arangodb::basics::Exception const& ex) {
    return Result(ex.code(), std::string("continuous synchronization for database '") + _databaseName + "' failed with exception: " + ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, std::string("continuous synchronization for database '") + _databaseName + "' failed with exception: " + ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL, std::string("continuous synchronization for database '") + _databaseName + "' failed with unknown exception");
  }
}

/// @brief run method, performs continuous synchronization
/// internal method, may throw exceptions
Result TailingSyncer::runInternal() {
  if (_client == nullptr || _connection == nullptr || _endpoint == nullptr) {
    return Result(TRI_ERROR_INTERNAL);
  }
  
  setAborted(false);
    
  TRI_DEFER(sendRemoveBarrier());
  uint64_t shortTermFailsInRow = 0;
  
retry:
  double const start = TRI_microtime();
  
  Result res;
  uint64_t connectRetries = 0;
  
  // reset failed connects
  {
    WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock);
    _applier->_state._failedConnects = 0;
  }
  
  while (true) {
    setProgress("fetching master state information");
    res = getMasterState();
    
    if (res.is(TRI_ERROR_REPLICATION_NO_RESPONSE)) {
      // master error. try again after a sleep period
      connectRetries++;
      {
        WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock);
        _applier->_state._failedConnects = connectRetries;
        _applier->_state._totalRequests++;
        _applier->_state._totalFailedConnects++;
      }
      
      if (connectRetries <= _configuration._maxConnectRetries) {
        // check if we are aborted externally
        if (_applier->sleepIfStillActive(_configuration._connectionRetryWaitTime)) {
          setProgress("fetching master state information failed. will retry now. "
                      "retries left: " +
                      std::to_string(_configuration._maxConnectRetries -
                                     connectRetries));
          continue;
        }
        
        // somebody stopped the applier
        res.reset(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
      }
    }
    
    // we either got a connection or an error
    break;
  }
    
  if (res.ok()) {
    WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock);
    try {
      getLocalState();
      _applier->_state._failedConnects = 0;
      _applier->_state._totalRequests++;
    } catch (basics::Exception const& ex) {
      res = Result(ex.code(), ex.what());
    } catch (std::exception const& ex) {
      res = Result(TRI_ERROR_INTERNAL, ex.what());
    } catch (...) {
      res.reset(TRI_ERROR_INTERNAL, "caught unknown exception");
    }
  }
  
  if (res.fail()) {
    // stop ourselves
    LOG_TOPIC(INFO, Logger::REPLICATION) << "stopping applier: " << res.errorMessage();
    try {
      WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock);
      _applier->_state._totalRequests++;
      getLocalState();
    } catch (basics::Exception const& ex) {
      res = Result(ex.code(), ex.what());
    } catch (std::exception const& ex) {
      res = Result(TRI_ERROR_INTERNAL, ex.what());
    } catch (...) {
      res.reset(TRI_ERROR_INTERNAL, "caught unknown exception");
    }

    _applier->stop(res);
    
    return res;
  }
  
  if (res.ok()) {
    res = runContinuousSync();
  }
  
  if (res.fail()) {
    // stop ourselves
    if (res.is(TRI_ERROR_REPLICATION_START_TICK_NOT_PRESENT) ||
        res.is(TRI_ERROR_REPLICATION_NO_START_TICK)) {
      if (res.is(TRI_ERROR_REPLICATION_START_TICK_NOT_PRESENT)) {
        LOG_TOPIC(WARN, Logger::REPLICATION)
        << "replication applier stopped for database '" << _databaseName
        << "' because required tick is not present on master";
      }
      
      // remove previous applier state
      abortOngoingTransactions();
      
      _applier->removeState();
      
      // TODO: merge with removeState
      {
        WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock);
        
        LOG_TOPIC(DEBUG, Logger::REPLICATION)
        << "stopped replication applier for database '" << _databaseName
        << "' with lastProcessedContinuousTick: "
        << _applier->_state._lastProcessedContinuousTick
        << ", lastAppliedContinuousTick: "
        << _applier->_state._lastAppliedContinuousTick
        << ", safeResumeTick: " << _applier->_state._safeResumeTick;
        
        _applier->_state._lastProcessedContinuousTick = 0;
        _applier->_state._lastAppliedContinuousTick = 0;
        _applier->_state._safeResumeTick = 0;
        _applier->_state._failedConnects = 0;
        _applier->_state._totalRequests = 0;
        _applier->_state._totalFailedConnects = 0;
        _applier->_state._totalResyncs = 0;
        
        saveApplierState();
      }
      
      setAborted(false);
      
      if (!_configuration._autoResync) {
        LOG_TOPIC(INFO, Logger::REPLICATION) << "Auto resync disabled, applier will stop";
        _applier->stop(res);
        return res;
      }
      
      if (TRI_microtime() - start < 120.0) {
        // the applier only ran for less than 2 minutes. probably
        // auto-restarting it won't help much
        shortTermFailsInRow++;
      } else {
        shortTermFailsInRow = 0;
      }
      
      // check if we've made too many retries
      if (shortTermFailsInRow > _configuration._autoResyncRetries) {
        if (_configuration._autoResyncRetries > 0) {
          // message only makes sense if there's at least one retry
          LOG_TOPIC(WARN, Logger::REPLICATION)
          << "aborting automatic resynchronization for database '"
          << _databaseName << "' after "
          << _configuration._autoResyncRetries << " retries";
        } else {
          LOG_TOPIC(WARN, Logger::REPLICATION)
          << "aborting automatic resynchronization for database '"
          << _databaseName << "' because autoResyncRetries is 0";
        }
        
        // always abort if we get here
        _applier->stop(res);
        return res;
      }

      {
        // increase number of syncs counter
        WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock);
        ++_applier->_state._totalResyncs;
      }
      
      // do an automatic full resync
      LOG_TOPIC(WARN, Logger::REPLICATION)
      << "restarting initial synchronization for database '"
      << _databaseName << "' because autoResync option is set. retry #"
      << shortTermFailsInRow;
      
      // start initial synchronization
      try {
        std::unique_ptr<InitialSyncer> syncer = _applier->buildInitialSyncer();
        Result r = syncer->run(_configuration._incremental);
        if (r.ok()) {
          TRI_voc_tick_t lastLogTick = syncer->getLastLogTick();
          LOG_TOPIC(INFO, Logger::REPLICATION)
          << "automatic resynchronization for database '" << _databaseName
          << "' finished. restarting continuous replication applier from "
          "tick "
          << lastLogTick;
          _initialTick = lastLogTick;
          _useTick = true;
          goto retry;
        }
        res.reset(r.errorNumber(), r.errorMessage());
        LOG_TOPIC(WARN, Logger::REPLICATION) << "(global tailing) initial replication failed: " << res.errorMessage();
        // fall through otherwise
      } catch (...) {
        res.reset(TRI_ERROR_INTERNAL, "caught unknown exception during initial replication");
      }
    }
    
    _applier->stop(res);
    return res;
  }
  
  return Result();
}

/// @brief get local replication apply state
void TailingSyncer::getLocalState() {
  uint64_t oldTotalRequests = _applier->_state._totalRequests;
  uint64_t oldTotalFailedConnects = _applier->_state._totalFailedConnects;
  
  bool const foundState = _applier->loadState();
  _applier->_state._totalRequests = oldTotalRequests;
  _applier->_state._totalFailedConnects = oldTotalFailedConnects;
    
  if (!foundState) {
    // no state file found, so this is the initialization
    _applier->_state._serverId = _masterInfo._serverId;
    if (_useTick && _initialTick > 0) {
      _applier->_state._lastProcessedContinuousTick = _initialTick - 1;
      _applier->_state._lastAppliedContinuousTick = _initialTick - 1;
    }
    _applier->persistState(true);
    return;
  }
 
  // a _masterInfo._serverId value of 0 may occur if no proper connection could be
  // established to the master initially
  if (_masterInfo._serverId != _applier->_state._serverId &&
      _applier->_state._serverId != 0 && _masterInfo._serverId != 0) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_REPLICATION_MASTER_CHANGE,
                                   std::string("encountered wrong master id in replication state file. found: ") +
                                   StringUtils::itoa(_masterInfo._serverId) + ", expected: " +
                                   StringUtils::itoa(_applier->_state._serverId));
  }
}

/// @brief perform a continuous sync with the master
Result TailingSyncer::runContinuousSync() {
  static uint64_t const MinWaitTime = 300 * 1000;        //  0.30 seconds
  static uint64_t const MaxWaitTime = 60 * 1000 * 1000;  // 60    seconds
  uint64_t connectRetries = 0;
  uint64_t inactiveCycles = 0;
  
  // get start tick
  // ---------------------------------------
  TRI_voc_tick_t fromTick = 0;
  TRI_voc_tick_t safeResumeTick = 0;
  
  {
    WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock);

    if (_useTick) {
      // use user-defined tick
      fromTick = _initialTick;
      _applier->_state._lastAppliedContinuousTick = 0;
      _applier->_state._lastProcessedContinuousTick = 0;
      // saveApplierState();
    } else {
      // if we already transferred some data, we'll use the last applied tick
      if (_applier->_state._lastAppliedContinuousTick >= fromTick) {
        fromTick = _applier->_state._lastAppliedContinuousTick;
      } else {
        LOG_TOPIC(WARN, Logger::REPLICATION)
        << "restarting continuous synchronization from previous state"
        << ", lastAppliedContinuousTick in state: " << _applier->_state._lastAppliedContinuousTick
        << ", lastProcessedContinuousTick in state: " << _applier->_state._lastProcessedContinuousTick
        << ", safeResumeTick in state: " << _applier->_state._safeResumeTick << ", fromTick: 0";
      }
      safeResumeTick = _applier->_state._safeResumeTick;
    }
  }
  
  LOG_TOPIC(DEBUG, Logger::REPLICATION)
  << "requesting continuous synchronization, fromTick: " << fromTick
  << ", safeResumeTick " << safeResumeTick << ", useTick: " << _useTick
  << ", initialTick: " << _initialTick;
  
  if (fromTick == 0) {
    return Result(TRI_ERROR_REPLICATION_NO_START_TICK);
  }
  
  // get the applier into a sensible start state by fetching the list of
  // open transactions from the master
  TRI_voc_tick_t fetchTick = safeResumeTick;
  if (safeResumeTick > 0 && safeResumeTick == fromTick) {
    // special case in which from and to are equal
    fetchTick = safeResumeTick;
  } else {
    // adjust fetchTick so we can tail starting from the tick containing
    // the open transactions we did not commit locally
    Result res = fetchOpenTransactions(safeResumeTick, fromTick, fetchTick);
    if (res.fail()) {
      return res;
    }
  }
  
  if (fetchTick > fromTick) {
    // must not happen
    return Result(TRI_ERROR_INTERNAL);
  }
  
  std::string const progress =
  "starting with from tick " + StringUtils::itoa(fromTick) +
  ", fetch tick " + StringUtils::itoa(fetchTick) + ", open transactions: " +
  StringUtils::itoa(_ongoingTransactions.size());
  setProgress(progress);
  
  // run in a loop. the loop is terminated when the applier is stopped or an
  // error occurs
  while (true) {
    bool worked = false;
    bool masterActive = false;
    
    // fetchTick is passed by reference!
    Result res = followMasterLog(fetchTick, fromTick, _configuration._ignoreErrors, worked, masterActive);
    
    uint64_t sleepTime;
    
    if (res.is(TRI_ERROR_REPLICATION_NO_RESPONSE) ||
        res.is(TRI_ERROR_REPLICATION_MASTER_ERROR)) {
      // master error. try again after a sleep period
      if (_configuration._connectionRetryWaitTime > 0) {
        sleepTime = _configuration._connectionRetryWaitTime;
        if (sleepTime < MinWaitTime) {
          sleepTime = MinWaitTime;
        }
      } else {
        // default to prevent spinning too busy here
        sleepTime = 30 * 1000 * 1000;
      }
      
      connectRetries++;
      
      {
        WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock);
        
        _applier->_state._failedConnects = connectRetries;
        _applier->_state._totalRequests++;
        _applier->_state._totalFailedConnects++;
      }
      
      if (connectRetries > _configuration._maxConnectRetries) {
        // halt
        return res;
      }
    } else {
      connectRetries = 0;
      
      {
        WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock);
        
        _applier->_state._failedConnects = connectRetries;
        _applier->_state._totalRequests++;
      }
      
      if (res.fail()) {
        // some other error we will not ignore
        return res;
      }
      
      // no error
      if (worked) {
        // we have done something, so we won't sleep (but check for cancelation)
        inactiveCycles = 0;
        sleepTime = 0;
      } else {
        sleepTime = _configuration._idleMinWaitTime;
        if (sleepTime < MinWaitTime) {
          sleepTime = MinWaitTime;  // hard-coded minimum wait time
        }
        
        if (_configuration._adaptivePolling) {
          inactiveCycles++;
          if (inactiveCycles > 60) {
            sleepTime *= 5;
          } else if (inactiveCycles > 30) {
            sleepTime *= 3;
          }
          if (inactiveCycles > 15) {
            sleepTime *= 2;
          }
          
          if (sleepTime > _configuration._idleMaxWaitTime) {
            sleepTime = _configuration._idleMaxWaitTime;
          }
        }
        
        if (sleepTime > MaxWaitTime) {
          sleepTime = MaxWaitTime;  // hard-coded maximum wait time
        }
      }
    }
    
    LOG_TOPIC(TRACE, Logger::REPLICATION) << "master active: " << masterActive
    << ", worked: " << worked
    << ", sleepTime: " << sleepTime;
    
    // this will make the applier thread sleep if there is nothing to do,
    // but will also check for cancelation
    if (!_applier->sleepIfStillActive(sleepTime)) {
      return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
    }
  }
  
  // won't be reached
  return Result(TRI_ERROR_INTERNAL);
}

/// @brief fetch the open transactions we still need to complete
Result TailingSyncer::fetchOpenTransactions(TRI_voc_tick_t fromTick,
                                            TRI_voc_tick_t toTick,
                                            TRI_voc_tick_t& startTick) {
  std::string const baseUrl = tailingBaseUrl("open-transactions");
  std::string const url = baseUrl + "serverId=" + _localServerIdString +
  "&from=" + StringUtils::itoa(fromTick) + "&to=" +
  StringUtils::itoa(toTick);
  
  std::string const progress = "fetching initial master state with from tick " +
  StringUtils::itoa(fromTick) + ", to tick " +
  StringUtils::itoa(toTick);
  
  setProgress(progress);
  
  // send request
  std::unique_ptr<SimpleHttpResult> response(_client->request(rest::RequestType::GET, url, nullptr, 0));

  if (hasFailed(response.get())) {
    return buildHttpError(response.get(), url);
  }

  bool fromIncluded = false;
  
  bool found;
  std::string header =
  response->getHeaderField(TRI_REPLICATION_HEADER_FROMPRESENT, found);
  
  if (found) {
    fromIncluded = StringUtils::boolean(header);
  }
  
  // fetch the tick from where we need to start scanning later
  header = response->getHeaderField(TRI_REPLICATION_HEADER_LASTINCLUDED, found);
  if (!found) {
    // we changed the API in 3.3 to use last included
    header = response->getHeaderField(TRI_REPLICATION_HEADER_LASTTICK, found);
    if (!found) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") +
                    _masterInfo._endpoint + ": required header " + TRI_REPLICATION_HEADER_LASTTICK +
                    " is missing in determine-open-transactions response");
    }
  }
  
  TRI_voc_tick_t readTick = StringUtils::uint64(header);
  
  if (!fromIncluded && _requireFromPresent && fromTick > 0 &&
      (!simulate32Client() || fromTick != readTick)) {
    return Result(TRI_ERROR_REPLICATION_START_TICK_NOT_PRESENT, std::string("required init tick value '") +
                  StringUtils::itoa(fromTick) + "' is not present (anymore?) on master at " +
                  _masterInfo._endpoint + ". Last tick available on master is '" + StringUtils::itoa(readTick) +
                  "'. It may be required to do a full resync and increase the number of historic logfiles/WAL file timeout on the master.");
  }
  
  startTick = readTick;
  if (startTick == 0) {
    startTick = toTick;
  }
  
  VPackBuilder builder;
  Result r = parseResponse(builder, response.get());
  
  if (r.fail()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") + _masterInfo._endpoint + ": invalid response type for initial data. expecting array");
  }
  
  VPackSlice const slice = builder.slice();
  if (!slice.isArray()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") + _masterInfo._endpoint + ": invalid response type for initial data. expecting array");
  }
  
  for (auto const& it : VPackArrayIterator(slice)) {
    if (!it.isString()) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") +
                    _masterInfo._endpoint + ": invalid response type for initial data. expecting array of ids");
    }
    _ongoingTransactions.emplace(StringUtils::uint64(it.copyString()), nullptr);
  }
  
  {
    std::string const progress =
    "fetched initial master state for from tick " +
    StringUtils::itoa(fromTick) + ", to tick " + StringUtils::itoa(toTick) +
    ", got start tick: " + StringUtils::itoa(readTick) +
    ", open transactions: " + std::to_string(_ongoingTransactions.size());
    
    setProgress(progress);
  }
  
  return Result();
}

/// @brief run the continuous synchronization
Result TailingSyncer::followMasterLog(TRI_voc_tick_t& fetchTick,
                                      TRI_voc_tick_t firstRegularTick,
                                      uint64_t& ignoreCount, bool& worked,
                                      bool& masterActive) {
  std::string const baseUrl = tailingBaseUrl("tail") + "chunkSize=" +
    StringUtils::itoa(_configuration._chunkSize) + "&barrier=" +
    StringUtils::itoa(_barrierId);
  
  TRI_voc_tick_t const originalFetchTick = fetchTick;
  worked = false;
  
  std::string const url = baseUrl + "&from=" + StringUtils::itoa(fetchTick) +
    "&firstRegular=" + StringUtils::itoa(firstRegularTick) + "&serverId=" +
    _localServerIdString + "&includeSystem=" + (_configuration._includeSystem ? "true" : "false");
  
  // send request
  std::string const progress =
    "fetching master log from tick " + StringUtils::itoa(fetchTick) +
    ", first regular tick " + StringUtils::itoa(firstRegularTick) +
    ", barrier: " + StringUtils::itoa(_barrierId) + ", open transactions: " +
    std::to_string(_ongoingTransactions.size()) + ", chunk size " + std::to_string(_configuration._chunkSize);
   
  setProgress(progress);
  
  std::string body;
  if (!_ongoingTransactions.empty()) {
    // stringify list of open transactions
    body.append("[\"");
    bool first = true;
    
    for (auto& it : _ongoingTransactions) {
      if (first) {
        first = false;
      } else {
        body.append("\",\"");
      }
      
      body.append(StringUtils::itoa(it.first));
    }
    body.append("\"]");
  } else {
    body.append("[]");
  }
  
  std::unique_ptr<SimpleHttpResult> response(_client->request(rest::RequestType::PUT,
                                                              url, body.c_str(), body.size()));
  
  if (hasFailed(response.get())) {
    return buildHttpError(response.get(), url);
  }
  
  bool found;
  std::string header = response->getHeaderField(TRI_REPLICATION_HEADER_CHECKMORE, found);
  
  if (!found) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") +
                  _masterInfo._endpoint + ": required header " + TRI_REPLICATION_HEADER_CHECKMORE + " is missing");
  }
  
  bool checkMore = StringUtils::boolean(header);
  
  // was the specified from value included the result?
  bool fromIncluded = false;
  header = response->getHeaderField(TRI_REPLICATION_HEADER_FROMPRESENT, found);
  if (found) {
    fromIncluded = StringUtils::boolean(header);
  }
  
  bool active = false;
  header = response->getHeaderField(TRI_REPLICATION_HEADER_ACTIVE, found);
  if (found) {
    active = StringUtils::boolean(header);
  }

  TRI_voc_tick_t lastScannedTick = 0;
  header = response->getHeaderField(TRI_REPLICATION_HEADER_LASTSCANNED, found);
  if (found) {
    lastScannedTick = StringUtils::uint64(header);
  }
  
  header = response->getHeaderField(TRI_REPLICATION_HEADER_LASTINCLUDED, found);
  if (!found) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") +
                  _masterInfo._endpoint + ": required header " + TRI_REPLICATION_HEADER_LASTINCLUDED +
                  " is missing in logger-follow response");
  }
  
  TRI_voc_tick_t lastIncludedTick = StringUtils::uint64(header);
  
  if (lastIncludedTick == 0 && lastScannedTick > 0 && lastScannedTick > fetchTick) {
    // master did not have any news for us  
    // still we can move forward the place from which to tail the WAL files
    fetchTick = lastScannedTick - 1;
  }
  if (lastIncludedTick > fetchTick) {
    fetchTick = lastIncludedTick;
    worked = true;
  } else {
    // we got the same tick again, this indicates we're at the end
    checkMore = false;
  }
  
  header = response->getHeaderField(TRI_REPLICATION_HEADER_LASTTICK, found);
  if (!found) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") +
                  _masterInfo._endpoint + ": required header " + TRI_REPLICATION_HEADER_LASTTICK +
                  " is missing in logger-follow response");
  }
  
  bool bumpTick = false;
  TRI_voc_tick_t tick = StringUtils::uint64(header);
  if (!checkMore && tick > lastIncludedTick) {
    // the master has a tick value which is not contained in this result
    // but it claims it does not have any more data
    // so it's probably a tick from an invisible operation (such as
    // closing a WAL file)
    bumpTick = true;
  }
  
  {
    WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock);
    _applier->_state._lastAvailableContinuousTick = tick;
  }
  
  if (!fromIncluded && _requireFromPresent && fetchTick > 0 && 
      (!simulate32Client() || originalFetchTick != tick)) {
    return Result(TRI_ERROR_REPLICATION_START_TICK_NOT_PRESENT, std::string("required follow tick value '") +
                  StringUtils::itoa(fetchTick) + "' is not present (anymore?) on master at " + _masterInfo._endpoint +
                  ". Last tick available on master is '" + StringUtils::itoa(tick) +
                  "'. It may be required to do a full resync and increase the number " +
                  "of historic logfiles/WAL file timeout on the master");
  }
  
  TRI_voc_tick_t lastAppliedTick;
  
  {
    READ_LOCKER(locker, _applier->_statusLock);
    lastAppliedTick = _applier->_state._lastAppliedContinuousTick;
  }
  
  uint64_t processedMarkers = 0;
  Result r = applyLog(response.get(), firstRegularTick, processedMarkers, ignoreCount);
 
  // cppcheck-suppress *
  if (processedMarkers > 0) {
    worked = true;
    
    WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock);
    _applier->_state._totalEvents += processedMarkers;
    
    if (_applier->_state._lastAppliedContinuousTick != lastAppliedTick) {
      _hasWrittenState = true;
      saveApplierState();
    }
  } else if (bumpTick) {
    WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock);
    
    if (_applier->_state._lastProcessedContinuousTick < tick) {
      _applier->_state._lastProcessedContinuousTick = tick;
    }
    
    if (_ongoingTransactions.empty() &&
        _applier->_state._safeResumeTick == 0) {
      _applier->_state._safeResumeTick = tick;
    }
    
    if (_ongoingTransactions.empty() && 
        _applier->_state._lastAppliedContinuousTick == 0) {
      _applier->_state._lastAppliedContinuousTick = _applier->_state._lastProcessedContinuousTick;
    }
    
    if (!_hasWrittenState) {
      _hasWrittenState = true;
      saveApplierState();
    }
  }
  
  if (!_hasWrittenState && _useTick) {
    // write state at least once so the start tick gets saved
    _hasWrittenState = true;
    
    WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock);
    
    _applier->_state._lastAppliedContinuousTick = firstRegularTick;
    _applier->_state._lastProcessedContinuousTick = firstRegularTick;
    
    if (_ongoingTransactions.empty() &&
        _applier->_state._safeResumeTick == 0) {
      _applier->_state._safeResumeTick = firstRegularTick;
    }
    
    saveApplierState();
  }
  
  if (r.fail()) {
    return r;
  }
  
  masterActive = active;
  
  if (!worked) {
    if (checkMore) {
      worked = true;
    }
  }
  
  return Result();
}
