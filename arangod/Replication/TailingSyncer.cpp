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
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/ReadLocker.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Logger/Logger.h"
#include "Rest/HttpRequest.h"
#include "Replication/ReplicationTransaction.h"
#include "RestServer/DatabaseFeature.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Hints.h"
#include "Utils/CollectionGuard.h"
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

TailingSyncer::TailingSyncer(
    ReplicationApplierConfiguration const& configuration,
    TRI_voc_tick_t initialTick, TRI_voc_tick_t barrierId)
    : Syncer(configuration),
      _initialTick(initialTick),
      _requireFromPresent(configuration._requireFromPresent),
      _supportsSingleOperations(false),
      _ignoreRenameCreateDrop(false),
      _ignoreDatabaseMarkers(true) {

  if (configuration._restrictType == "include") {
    _restrictType = RESTRICT_INCLUDE;
  } else if (configuration._restrictType == "exclude") {
    _restrictType = RESTRICT_EXCLUDE;
  }
        
  if (barrierId > 0) {
    _barrierId = barrierId;
    _barrierUpdateTime = TRI_microtime();
  }

  // FIXME: move this into engine code
  std::string engineName = EngineSelectorFeature::ENGINE->typeName();
  _supportsSingleOperations = (engineName == "mmfiles");
}

TailingSyncer::~TailingSyncer() { abortOngoingTransactions(); }

/// @brief set the applier progress
void TailingSyncer::setProgress(std::string const& msg) {
  if (_configuration._verbose) {
    LOG_TOPIC(INFO, Logger::REPLICATION) << msg;
  } else {
    LOG_TOPIC(DEBUG, Logger::REPLICATION) << msg;
  }
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
                               VPackSlice const& slice) const {
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
  if (!_configuration._restrictCollections.empty()) {
    if (_restrictType == RESTRICT_NONE && _configuration._includeSystem) {
      return false;
    }

    VPackSlice const name = slice.get("cname");
    if (name.isString()) {
      return isExcludedCollection(name.copyString());
    }
  }

  return false;
}

/// @brief whether or not a collection should be excluded
bool TailingSyncer::isExcludedCollection(std::string const& masterName) const {
  if (masterName[0] == '_' && !_configuration._includeSystem) {
    // system collection
    return true;
  }

  auto const it = _configuration._restrictCollections.find(masterName);

  bool found = (it != _configuration._restrictCollections.end());

  if (_restrictType == RESTRICT_INCLUDE && !found) {
    // collection should not be included
    return true;
  } else if (_restrictType == RESTRICT_EXCLUDE && found) {
    return true;
  }

  if (TRI_ExcludeCollectionReplication(masterName, true)) {
    return true;
  }

  return false;
}


/// @brief process db create or drop markers
int TailingSyncer::processDBMarker(TRI_replication_operation_e type,
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
    LOG_TOPIC(ERR, Logger::REPLICATION) << "Invalid database name in log";
    TRI_ASSERT(false);
    return TRI_ERROR_ARANGO_DATABASE_NAME_INVALID;
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
      LOG_TOPIC(WARN, Logger::REPLICATION) << "Seeing database creation marker "
        << "for an alread existing db. Dropping db...";
      TRI_vocbase_t* system = DatabaseFeature::DATABASE->systemDatabase();
      TRI_ASSERT(system);
      Result res = methods::Databases::drop(system, name);
      if (res.fail()) {
        LOG_TOPIC(ERR, Logger::REPLICATION) << res.errorMessage();
        return res.errorNumber();
      }
    }
    
    VPackSlice users = VPackSlice::emptyArraySlice();
    Result res = methods::Databases::create(name, users,
                                            VPackSlice::emptyObjectSlice());
    return res.errorNumber();
  } else if (type == REPLICATION_DATABASE_DROP) {

    TRI_vocbase_t* vocbase = DatabaseFeature::DATABASE->lookupDatabase(name);
    if (vocbase != nullptr && name != TRI_VOC_SYSTEM_DATABASE) {
      TRI_vocbase_t* system = DatabaseFeature::DATABASE->systemDatabase();
      TRI_ASSERT(system != nullptr);
      Result res = methods::Databases::drop(system, name);
      if (res.fail()) {
        LOG_TOPIC(ERR, Logger::REPLICATION) << res.errorMessage();
      }
      return res.errorNumber();
    } else {
      TRI_ASSERT(false);// this should never  occur anyway
    }
    
  }
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL; // unreachable
}

/// @brief inserts a document, based on the VelocyPack provided
int TailingSyncer::processDocument(TRI_replication_operation_e type,
                                   VPackSlice const& slice,
                                   std::string& errorMsg) {
  
  TRI_vocbase_t* vocbase = resolveVocbase(slice);
  if (vocbase == nullptr) {
    return TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
  }
  arangodb::LogicalCollection* col = resolveCollection(vocbase, slice);
  if (col == nullptr) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }
  
  TRI_voc_cid_t cid = col->cid();
  bool isSystem = col->isSystem();
  
  // extract "data"
  VPackSlice const doc = slice.get("data");

  if (!doc.isObject()) {
    errorMsg = "invalid document format";
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // extract "key"
  VPackSlice const key = doc.get(StaticStrings::KeyString);

  if (!key.isString()) {
    errorMsg = "invalid document key format";
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // extract "rev"
  VPackSlice const rev = doc.get(StaticStrings::RevString);

  if (!rev.isString()) {
    errorMsg = "invalid document revision format";
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  _documentBuilder.clear();
  _documentBuilder.openObject();
  _documentBuilder.add(StaticStrings::KeyString, key);
  _documentBuilder.add(StaticStrings::RevString, rev);
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

  if (tid > 0) {
    auto it = _ongoingTransactions.find(tid);

    if (it == _ongoingTransactions.end()) {
      errorMsg = "unexpected transaction " + StringUtils::itoa(tid);
      return TRI_ERROR_REPLICATION_UNEXPECTED_TRANSACTION;
    }

    auto trx = (*it).second;

    if (trx == nullptr) {
      errorMsg = "unexpected transaction " + StringUtils::itoa(tid);
      return TRI_ERROR_REPLICATION_UNEXPECTED_TRANSACTION;
    }

    trx->addCollectionAtRuntime(cid, "", AccessMode::Type::EXCLUSIVE);
    int res = applyCollectionDumpMarker(*trx, trx->name(cid), type, old, doc,
                                        errorMsg);

    if (res == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED && isSystem) {
      // ignore unique constraint violations for system collections
      res = TRI_ERROR_NO_ERROR;
    }

    return res;
  }

  else {
    // standalone operation
    // update the apply tick for all standalone operations
    SingleCollectionTransaction trx(
        transaction::StandaloneContext::Create(vocbase), cid,
        AccessMode::Type::EXCLUSIVE);

    if (_supportsSingleOperations) {
      trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);
    }

    Result res = trx.begin();

    // fix error handling here when function returns result
    if (!res.ok()) {
      errorMsg =
          "unable to create replication transaction: " + res.errorMessage();
      res.reset(res.errorNumber(), errorMsg);
    } else {
      res =
          applyCollectionDumpMarker(trx, trx.name(), type, old, doc, errorMsg);
      if (res.errorNumber() == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED &&
          isSystem) {
        // ignore unique constraint violations for system collections
        res.reset();
      }
    }
    // fix error handling here when function returns result
    res = trx.finish(res);
    return res.errorNumber();
  }
}

/// @brief starts a transaction, based on the VelocyPack provided
int TailingSyncer::startTransaction(VPackSlice const& slice) {
  // {"type":2200,"tid":"230920705812199", "database": "123", "collections":[{"cid":"230920700700391","operations":10}]}
  
  TRI_vocbase_t* vocbase = resolveVocbase(slice);
  if (vocbase == nullptr) {
    return TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
  }
  
  std::string const id = VelocyPackHelper::getStringValue(slice, "tid", "");
  if (id.empty()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
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

  return res.errorNumber();
}

/// @brief aborts a transaction, based on the VelocyPack provided
int TailingSyncer::abortTransaction(VPackSlice const& slice) {
  // {"type":2201,"tid":"230920705812199","collections":[{"cid":"230920700700391","operations":10}]}
  std::string const id = VelocyPackHelper::getStringValue(slice, "tid", "");

  if (id.empty()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // transaction id
  // note: this is the remote transaction id!
  TRI_voc_tid_t const tid =
      static_cast<TRI_voc_tid_t>(StringUtils::uint64(id.c_str(), id.size()));

  auto it = _ongoingTransactions.find(tid);

  if (it == _ongoingTransactions.end()) {
    // invalid state, no transaction was started.
    return TRI_ERROR_REPLICATION_UNEXPECTED_TRANSACTION;
  }

  TRI_ASSERT(tid > 0);

  LOG_TOPIC(TRACE, Logger::REPLICATION) << "aborting replication transaction "
                                        << tid;

  auto trx = (*it).second;
  _ongoingTransactions.erase(tid);

  if (trx != nullptr) {
    Result res = trx->abort();
    delete trx;

    return res.errorNumber();
  }

  return TRI_ERROR_REPLICATION_UNEXPECTED_TRANSACTION;
}

/// @brief commits a transaction, based on the VelocyPack provided
int TailingSyncer::commitTransaction(VPackSlice const& slice) {
  // {"type":2201,"tid":"230920705812199","collections":[{"cid":"230920700700391","operations":10}]}
  std::string const id = VelocyPackHelper::getStringValue(slice, "tid", "");

  if (id.empty()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // transaction id
  // note: this is the remote trasnaction id!
  TRI_voc_tid_t const tid =
      static_cast<TRI_voc_tid_t>(StringUtils::uint64(id.c_str(), id.size()));

  auto it = _ongoingTransactions.find(tid);

  if (it == _ongoingTransactions.end()) {
    // invalid state, no transaction was started.
    return TRI_ERROR_REPLICATION_UNEXPECTED_TRANSACTION;
  }

  TRI_ASSERT(tid > 0);

  LOG_TOPIC(TRACE, Logger::REPLICATION) << "committing replication transaction "
                                        << tid;

  auto trx = (*it).second;
  _ongoingTransactions.erase(tid);

  if (trx != nullptr) {
    Result res = trx->commit();
    delete trx;

    return res.errorNumber();
  }

  return TRI_ERROR_REPLICATION_UNEXPECTED_TRANSACTION;
}

/// @brief renames a collection, based on the VelocyPack provided
int TailingSyncer::renameCollection(VPackSlice const& slice) {
  if (!slice.isObject()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  VPackSlice collection = slice.get("collection");
  if (!collection.isObject()) {
    collection = slice.get("data");
  }

  if (!collection.isObject()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  std::string const name =
      VelocyPackHelper::getStringValue(collection, "name", "");
  if (name.empty()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_vocbase_t* vocbase = resolveVocbase(slice);
  if (vocbase == nullptr) {
    return TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
  }
  arangodb::LogicalCollection* col = nullptr;
  if (slice.hasKey("cuid")) {
    col = resolveCollection(vocbase, slice);
  } else if (collection.hasKey("oldName")) {
    col = vocbase->lookupCollection(collection.get("oldName").copyString());
  }
  if (col == nullptr) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }
  return vocbase->renameCollection(col, name, true);
}

/// @brief changes the properties of a collection, based on the VelocyPack
/// provided
int TailingSyncer::changeCollection(VPackSlice const& slice) {
  if (!slice.isObject()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
  
  TRI_vocbase_t* vocbase = resolveVocbase(slice);
  if (vocbase == nullptr) {
    return TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
  }
  arangodb::LogicalCollection* col = resolveCollection(vocbase, slice);
  if (col == nullptr) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  VPackSlice data = slice.get("collection");
  if (!data.isObject()) {
    data = slice.get("data");
  }

  if (!data.isObject()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  arangodb::CollectionGuard guard(vocbase, col);
  bool doSync = DatabaseFeature::DATABASE->forceSyncProperties();
  return guard.collection()->updateProperties(data, doSync).errorNumber();
}

/// @brief apply a single marker from the continuous log
int TailingSyncer::applyLogMarker(VPackSlice const& slice,
                                  TRI_voc_tick_t firstRegularTick,
                                  std::string& errorMsg) {
  if (!slice.isObject()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // fetch marker "type"
  int typeValue = VelocyPackHelper::getNumericValue<int>(slice, "type", 0);

  // fetch "tick"
  std::string const tick = VelocyPackHelper::getStringValue(slice, "tick", "");

  if (!tick.empty()) {
    TRI_voc_tick_t newTick = static_cast<TRI_voc_tick_t>(
        StringUtils::uint64(tick.c_str(), tick.size()));
    preApplyMarker(firstRegularTick, newTick);
  }

  // handle marker type
  TRI_replication_operation_e type = (TRI_replication_operation_e)typeValue;

  if      (type == REPLICATION_DATABASE_CREATE ||
           type == REPLICATION_DATABASE_DROP) {
    if (_ignoreDatabaseMarkers) {
      return TRI_ERROR_NO_ERROR;
    }
    return processDBMarker(type, slice);
  }
  
  else if (type == REPLICATION_MARKER_DOCUMENT ||
             type == REPLICATION_MARKER_REMOVE) {
    return processDocument(type, slice, errorMsg);
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
      return TRI_ERROR_NO_ERROR;
    }
    TRI_vocbase_t* vocbase = resolveVocbase(slice);
    if (vocbase == nullptr) {
      return TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
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
      return TRI_ERROR_NO_ERROR;
    }
    return renameCollection(slice);
  }

  else if (type == REPLICATION_COLLECTION_CHANGE) {
    return changeCollection(slice);
  }

  else if (type == REPLICATION_INDEX_CREATE) {
    return createIndex(slice);
  }

  else if (type == REPLICATION_INDEX_DROP) {
    return dropIndex(slice);
  }

  else if (type == REPLICATION_VIEW_CREATE) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                   "view create not yet implemented");
  }

  else if (type == REPLICATION_VIEW_DROP) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                   "view drop not yet implemented");
  }

  else if (type == REPLICATION_VIEW_CHANGE) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                   "view change not yet implemented");
  }

  else if (_ignoreRenameCreateDrop) {
    return TRI_ERROR_NO_ERROR;
  }

  errorMsg = "unexpected marker type " + StringUtils::itoa(type);

  return TRI_ERROR_REPLICATION_UNEXPECTED_MARKER;
}

/// @brief apply the data from the continuous log
int TailingSyncer::applyLog(SimpleHttpResult* response,
                            TRI_voc_tick_t firstRegularTick,
                            std::string& errorMsg, uint64_t& processedMarkers,
                            uint64_t& ignoreCount) {
  StringBuffer& data = response->getBody();
  char const* p = data.begin();
  char const* end = p + data.length();

  // buffer must end with a NUL byte
  TRI_ASSERT(*end == '\0');

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
      return TRI_ERROR_NO_ERROR;
    }

    TRI_ASSERT(q <= end);

    processedMarkers++;

    builder->clear();
    try {
      VPackParser parser(builder);
      parser.parse(p, static_cast<size_t>(q - p));
    } catch (...) {
      // TODO: improve error reporting
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    p = q + 1;

    VPackSlice const slice = builder->slice();

    if (!slice.isObject()) {
      errorMsg = "received invalid JSON data";

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    int res;
    bool skipped;

    // LOG_TOPIC(ERR, Logger::FIXME) << slice.toJson();
    if (skipMarker(firstRegularTick, slice)) {
      // entry is skipped
      res = TRI_ERROR_NO_ERROR;
      skipped = true;
    } else {
      res = applyLogMarker(slice, firstRegularTick, errorMsg);
      skipped = false;
    }

    if (res != TRI_ERROR_NO_ERROR) {
      // apply error

      if (errorMsg.empty()) {
        // don't overwrite previous error message
        errorMsg = TRI_errno_string(res);
      }

      if (ignoreCount == 0) {
        if (lineLength > 1024) {
          errorMsg +=
              ", offending marker: " + std::string(lineStart, 1024) + "...";
        } else {
          errorMsg +=
              ", offending marker: " + std::string(lineStart, lineLength);
        }

        // LOG_TOPIC(ERR, Logger::REPLICATION) << "replication applier error: "
        // << errorMsg;

        return res;
      }

      ignoreCount--;
      LOG_TOPIC(WARN, Logger::REPLICATION)
          << "ignoring replication error for database '" << _databaseName
          << "': " << errorMsg;
      errorMsg = "";
    }

    // update tick value
    postApplyMarker(processedMarkers, skipped);
  }

  // reached the end
  return TRI_ERROR_NO_ERROR;
}
