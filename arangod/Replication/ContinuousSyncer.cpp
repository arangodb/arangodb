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

#include "ContinuousSyncer.h"

#include "Basics/Exceptions.h"
#include "Basics/json.h"
#include "Basics/JsonHelper.h"
#include "Basics/Logger.h"
#include "Basics/StringBuffer.h"
#include "Basics/WriteLocker.h"
#include "Replication/InitialSyncer.h"
#include "Rest/HttpRequest.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Utils/CollectionGuard.h"
#include "Utils/transactions.h"
#include "VocBase/document-collection.h"
#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::rest;

ContinuousSyncer::ContinuousSyncer(
    TRI_server_t* server, TRI_vocbase_t* vocbase,
    TRI_replication_applier_configuration_t const* configuration,
    TRI_voc_tick_t initialTick, bool useTick)
    : Syncer(vocbase, configuration),
      _server(server),
      _applier(vocbase->_replicationApplier),
      _chunkSize(),
      _restrictType(RESTRICT_NONE),
      _initialTick(initialTick),
      _useTick(useTick),
      _includeSystem(configuration->_includeSystem),
      _requireFromPresent(configuration->_requireFromPresent),
      _verbose(configuration->_verbose),
      _masterIs27OrHigher(false),
      _hasWrittenState(false) {
  uint64_t c = configuration->_chunkSize;
  if (c == 0) {
    c = (uint64_t)256 * 1024;  // 256 kb
  }

  TRI_ASSERT(c > 0);

  _chunkSize = StringUtils::itoa(c);

  if (configuration->_restrictType == "include") {
    _restrictType = RESTRICT_INCLUDE;
  } else if (configuration->_restrictType == "exclude") {
    _restrictType = RESTRICT_EXCLUDE;
  }
}

ContinuousSyncer::~ContinuousSyncer() { abortOngoingTransactions(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief run method, performs continuous synchronization
////////////////////////////////////////////////////////////////////////////////

int ContinuousSyncer::run() {
  if (_client == nullptr || _connection == nullptr || _endpoint == nullptr) {
    return TRI_ERROR_INTERNAL;
  }

  uint64_t shortTermFailsInRow = 0;

retry:
  double const start = TRI_microtime();
  std::string errorMsg;

  int res = TRI_ERROR_NO_ERROR;
  uint64_t connectRetries = 0;

  // reset failed connects
  {
    WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock, 1000);
    _applier->_state._failedConnects = 0;
  }

  while (_vocbase->_state < 2) {
    setProgress("fetching master state information");
    res = getMasterState(errorMsg);

    if (res == TRI_ERROR_REPLICATION_NO_RESPONSE) {
      // master error. try again after a sleep period
      connectRetries++;

      {
        WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock, 1000);
        _applier->_state._failedConnects = connectRetries;
        _applier->_state._totalRequests++;
        _applier->_state._totalFailedConnects++;
      }

      if (connectRetries <= _configuration._maxConnectRetries) {
        // check if we are aborted externally
        if (_applier->wait(_configuration._connectionRetryWaitTime)) {
          setProgress(
              "fetching master state information failed. will retry now. "
              "retries left: " +
              std::to_string(_configuration._maxConnectRetries -
                             connectRetries));
          continue;
        }

        // somebody stopped the applier
        res = TRI_ERROR_REPLICATION_APPLIER_STOPPED;
      }
    }

    // we either got a connection or an error
    break;
  }

  if (res == TRI_ERROR_NO_ERROR) {
    _masterIs27OrHigher =
        (_masterInfo._majorVersion > 2 ||
         (_masterInfo._majorVersion == 2 && _masterInfo._minorVersion >= 7));
    if (_requireFromPresent && !_masterIs27OrHigher) {
      LOG(WARN) << "requireFromPresent feature is not supported on master server < ArangoDB 2.7";
    }

    WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock, 1000);
    res = getLocalState(errorMsg);

    _applier->_state._failedConnects = 0;
    _applier->_state._totalRequests++;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    // stop ourselves
    _applier->stop(false);

    return _applier->setError(res, errorMsg.c_str());
  }

  if (res == TRI_ERROR_NO_ERROR) {
    res = runContinuousSync(errorMsg);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    _applier->setError(res, errorMsg.c_str());

    // stop ourselves
    _applier->stop(false);

    if (res == TRI_ERROR_REPLICATION_START_TICK_NOT_PRESENT ||
        res == TRI_ERROR_REPLICATION_NO_START_TICK) {
      if (res == TRI_ERROR_REPLICATION_START_TICK_NOT_PRESENT) {
        LOG(WARN) << "replication applier stopped for database '" << _vocbase->_name << "' because required tick is not present on master";
      }

      // remove previous applier state
      abortOngoingTransactions();

      TRI_RemoveStateReplicationApplier(_vocbase);

      {
        WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock, 1000);

        LOG(TRACE) << "stopped replication applier for database '" << _vocbase->_name << "' with lastProcessedContinuousTick: " << _applier->_state._lastProcessedContinuousTick << ", lastAppliedContinuousTick: " << _applier->_state._lastAppliedContinuousTick << ", safeResumeTick: " << _applier->_state._safeResumeTick;

        _applier->_state._lastProcessedContinuousTick = 0;
        _applier->_state._lastAppliedContinuousTick = 0;
        _applier->_state._safeResumeTick = 0;
        _applier->_state._failedConnects = 0;
        _applier->_state._totalRequests = 0;
        _applier->_state._totalFailedConnects = 0;

        saveApplierState();
      }

      if (!_configuration._autoResync) {
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
          LOG(WARN) << "aborting automatic resynchronization for database '" << _vocbase->_name << "' after " << _configuration._autoResyncRetries << " retries";
        }
        else {
          LOG(WARN) << "aborting automatic resynchronization for database '" << _vocbase->_name << "' because autoResyncRetries is 0";
        }

        // always abort if we get here
        return res;
      }

      // do an automatic full resync
      LOG(WARN) << "restarting initial synchronization for database '" << _vocbase->_name << "' because autoResync option is set. retry #" << shortTermFailsInRow;

      // start initial synchronization
      errorMsg = "";

      try {
        InitialSyncer syncer(
            _vocbase, &_configuration, _configuration._restrictCollections,
            _configuration._restrictType, _configuration._verbose);

        res = syncer.run(errorMsg, true);

        if (res == TRI_ERROR_NO_ERROR) {
          TRI_voc_tick_t lastLogTick = syncer.getLastLogTick();
          LOG(INFO) << "automatic resynchronization for database '" << _vocbase->_name << "' finished. restarting continuous replication applier from tick " << lastLogTick;
          _initialTick = lastLogTick;
          _useTick = true;
          goto retry;
        }
        // fall through otherwise
      } catch (...) {
        errorMsg = "caught an exception";
        res = TRI_ERROR_INTERNAL;
      }
    }

    return res;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief abort all ongoing transactions
////////////////////////////////////////////////////////////////////////////////

void ContinuousSyncer::abortOngoingTransactions() {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief set the applier progress
////////////////////////////////////////////////////////////////////////////////

void ContinuousSyncer::setProgress(char const* msg) {
  _applier->setProgress(msg, true);

  if (_verbose) {
    LOG(INFO) << "applier progress: " << msg;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the applier progress
////////////////////////////////////////////////////////////////////////////////

void ContinuousSyncer::setProgress(std::string const& msg) {
  setProgress(msg.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief save the current applier state
////////////////////////////////////////////////////////////////////////////////

int ContinuousSyncer::saveApplierState() {
  LOG(TRACE) << "saving replication applier state. last applied continuous tick: " << _applier->_state._lastAppliedContinuousTick << ", safe resume tick: " << _applier->_state._safeResumeTick;

  int res = TRI_SaveStateReplicationApplier(_vocbase, &_applier->_state, false);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(WARN) << "unable to save replication applier state: " << TRI_errno_string(res);
  }

  return res;
}
////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a marker should be skipped
////////////////////////////////////////////////////////////////////////////////

bool ContinuousSyncer::skipMarker(TRI_voc_tick_t firstRegularTick,
                                  TRI_json_t const* json) const {
  bool tooOld = false;
  std::string const tick = JsonHelper::getStringValue(json, "tick", "");

  if (!tick.empty()) {
    tooOld = (static_cast<TRI_voc_tick_t>(StringUtils::uint64(
                  tick.c_str(), tick.size())) < firstRegularTick);

    if (tooOld) {
      int typeValue = JsonHelper::getNumericValue<int>(json, "type", 0);
      // handle marker type
      TRI_replication_operation_e type = (TRI_replication_operation_e)typeValue;

      if (type == REPLICATION_MARKER_DOCUMENT ||
          type == REPLICATION_MARKER_EDGE ||
          type == REPLICATION_MARKER_REMOVE ||
          type == REPLICATION_TRANSACTION_START ||
          type == REPLICATION_TRANSACTION_ABORT ||
          type == REPLICATION_TRANSACTION_COMMIT) {
        // read "tid" entry from marker
        std::string const id = JsonHelper::getStringValue(json, "tid", "");

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

  if (_restrictType == RESTRICT_NONE && _includeSystem) {
    return false;
  }

  TRI_json_t const* name = TRI_LookupObjectJson(json, "cname");

  if (TRI_IsStringJson(name)) {
    return excludeCollection(std::string(name->_value._string.data,
                                         name->_value._string.length - 1));
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a collection should be excluded
////////////////////////////////////////////////////////////////////////////////

bool ContinuousSyncer::excludeCollection(std::string const& masterName) const {
  if (masterName[0] == '_' && !_includeSystem) {
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

  if (TRI_ExcludeCollectionReplication(masterName.c_str(), true)) {
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get local replication apply state
////////////////////////////////////////////////////////////////////////////////

int ContinuousSyncer::getLocalState(std::string& errorMsg) {
  uint64_t oldTotalRequests = _applier->_state._totalRequests;
  uint64_t oldTotalFailedConnects = _applier->_state._totalFailedConnects;

  int res = TRI_LoadStateReplicationApplier(_vocbase, &_applier->_state);
  _applier->_state._active = true;
  _applier->_state._totalRequests = oldTotalRequests;
  _applier->_state._totalFailedConnects = oldTotalFailedConnects;

  if (res == TRI_ERROR_FILE_NOT_FOUND) {
    // no state file found, so this is the initialization
    _applier->_state._serverId = _masterInfo._serverId;

    res = TRI_SaveStateReplicationApplier(_vocbase, &_applier->_state, true);

    if (res != TRI_ERROR_NO_ERROR) {
      errorMsg = "could not save replication state information";
    }
  } else if (res == TRI_ERROR_NO_ERROR) {
    if (_masterInfo._serverId != _applier->_state._serverId &&
        _applier->_state._serverId != 0) {
      res = TRI_ERROR_REPLICATION_MASTER_CHANGE;
      errorMsg =
          "encountered wrong master id in replication state file. "
          "found: " +
          StringUtils::itoa(_masterInfo._serverId) +
          ", "
          "expected: " +
          StringUtils::itoa(_applier->_state._serverId);
    }
  } else {
    // some error occurred
    TRI_ASSERT(res != TRI_ERROR_NO_ERROR);

    errorMsg = TRI_errno_string(res);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a document, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////

int ContinuousSyncer::processDocument(TRI_replication_operation_e type,
                                      TRI_json_t const* json,
                                      std::string& errorMsg) {
  // extract "cid"
  TRI_voc_cid_t cid = getCid(json);

  if (cid == 0) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  // extract optional "cname"
  bool isSystem = false;
  TRI_json_t const* cnameJson = JsonHelper::getObjectElement(json, "cname");

  if (JsonHelper::isString(cnameJson)) {
    std::string const cnameString =
        JsonHelper::getStringValue(json, "cname", "");
    isSystem = (!cnameString.empty() && cnameString[0] == '_');

    if (!cnameString.empty()) {
      TRI_vocbase_col_t* col =
          TRI_LookupCollectionByNameVocBase(_vocbase, cnameString.c_str());

      if (col != nullptr && col->_cid != cid) {
        // cid change? this may happen for system collections or if we restored
        // from a dump
        cid = col->_cid;
      }
    }
  }

  // extract "key"
  TRI_json_t const* keyJson = JsonHelper::getObjectElement(json, "key");

  if (!JsonHelper::isString(keyJson)) {
    errorMsg = "invalid document key format";
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // extract "rev"
  TRI_voc_rid_t rid;

  std::string const ridString = JsonHelper::getStringValue(json, "rev", "");
  if (ridString.empty()) {
    rid = 0;
  } else {
    rid = StringUtils::uint64(ridString.c_str(), ridString.size());
  }

  // extract "data"
  TRI_json_t const* doc = JsonHelper::getObjectElement(json, "data");

  // extract "tid"
  std::string const id = JsonHelper::getStringValue(json, "tid", "");
  TRI_voc_tid_t tid;

  if (id.empty()) {
    // standalone operation
    tid = 0;
  } else {
    // operation is part of a transaction
    tid =
        static_cast<TRI_voc_tid_t>(StringUtils::uint64(id.c_str(), id.size()));
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

    TRI_transaction_collection_t* trxCollection = trx->trxCollection(cid);

    if (trxCollection == nullptr) {
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    }

    int res = applyCollectionDumpMarker(
        trx, trxCollection, type,
        (const TRI_voc_key_t)keyJson->_value._string.data, rid, doc, errorMsg);

    if (res == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED && isSystem) {
      // ignore unique constraint violations for system collections
      res = TRI_ERROR_NO_ERROR;
    }

    return res;
  }

  else {
    // standalone operation
    // update the apply tick for all standalone operations
    SingleCollectionWriteTransaction<1> trx(new StandaloneTransactionContext(),
                                            _vocbase, cid);

    int res = trx.begin();

    if (res != TRI_ERROR_NO_ERROR) {
      errorMsg = "unable to create replication transaction: " +
                 std::string(TRI_errno_string(res));

      return res;
    }

    TRI_transaction_collection_t* trxCollection = trx.trxCollection();

    if (trxCollection == nullptr) {
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    }

    res = applyCollectionDumpMarker(
        &trx, trxCollection, type,
        (const TRI_voc_key_t)keyJson->_value._string.data, rid, doc, errorMsg);

    if (res == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED && isSystem) {
      // ignore unique constraint violations for system collections
      res = TRI_ERROR_NO_ERROR;
    }

    res = trx.finish(res);

    return res;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts a transaction, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////

int ContinuousSyncer::startTransaction(TRI_json_t const* json) {
  // {"type":2200,"tid":"230920705812199","collections":[{"cid":"230920700700391","operations":10}]}

  std::string const id = JsonHelper::getStringValue(json, "tid", "");

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

  LOG(TRACE) << "starting transaction " << tid;

  auto trx = std::make_unique<ReplicationTransaction>(_server, _vocbase, tid);

  int res = trx->begin();

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  _ongoingTransactions[tid] = trx.get();
  trx.release();

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief aborts a transaction, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////

int ContinuousSyncer::abortTransaction(TRI_json_t const* json) {
  // {"type":2201,"tid":"230920705812199","collections":[{"cid":"230920700700391","operations":10}]}
  std::string const id = JsonHelper::getStringValue(json, "tid", "");

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

  LOG(TRACE) << "abort replication transaction " << tid;

  auto trx = (*it).second;
  _ongoingTransactions.erase(tid);

  if (trx != nullptr) {
    int res = trx->abort();
    delete trx;

    return res;
  }

  return TRI_ERROR_REPLICATION_UNEXPECTED_TRANSACTION;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief commits a transaction, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////

int ContinuousSyncer::commitTransaction(TRI_json_t const* json) {
  // {"type":2201,"tid":"230920705812199","collections":[{"cid":"230920700700391","operations":10}]}
  std::string const id = JsonHelper::getStringValue(json, "tid", "");

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

  LOG(TRACE) << "committing replication transaction " << tid;

  auto trx = (*it).second;
  _ongoingTransactions.erase(tid);

  if (trx != nullptr) {
    int res = trx->commit();
    delete trx;

    return res;
  }

  return TRI_ERROR_REPLICATION_UNEXPECTED_TRANSACTION;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a collection, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////

int ContinuousSyncer::renameCollection(TRI_json_t const* json) {
  TRI_json_t const* collectionJson = TRI_LookupObjectJson(json, "collection");
  std::string const name =
      JsonHelper::getStringValue(collectionJson, "name", "");
  std::string cname = getCName(json);

  if (name.empty()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_voc_cid_t cid = getCid(json);
  TRI_vocbase_col_t* col = TRI_LookupCollectionByIdVocBase(_vocbase, cid);

  if (col == nullptr && !cname.empty()) {
    col = TRI_LookupCollectionByNameVocBase(_vocbase, cname.c_str());
  }

  if (col == nullptr) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  return TRI_RenameCollectionVocBase(_vocbase, col, name.c_str(), true, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief changes the properties of a collection, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////

int ContinuousSyncer::changeCollection(TRI_json_t const* json) {
  TRI_voc_cid_t cid = getCid(json);
  std::string cname = getCName(json);
  TRI_vocbase_col_t* col = TRI_LookupCollectionByIdVocBase(_vocbase, cid);

  if (col == nullptr && !cname.empty()) {
    col = TRI_LookupCollectionByNameVocBase(_vocbase, cname.c_str());
    if (col != nullptr) {
      cid = col->_cid;
    }
  }

  if (col == nullptr) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  try {
    TRI_json_t const* collectionJson = TRI_LookupObjectJson(json, "collection");
    std::shared_ptr<arangodb::velocypack::Builder> tmp =
        arangodb::basics::JsonHelper::toVelocyPack(collectionJson);
    arangodb::CollectionGuard guard(_vocbase, cid);
    bool doSync = _vocbase->_settings.forceSyncProperties;

    return TRI_UpdateCollectionInfo(_vocbase, guard.collection()->_collection,
                                    tmp->slice(), doSync);
  } catch (arangodb::basics::Exception const& ex) {
    return ex.code();
  } catch (...) {
    return TRI_ERROR_INTERNAL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply a single marker from the continuous log
////////////////////////////////////////////////////////////////////////////////

int ContinuousSyncer::applyLogMarker(TRI_json_t const* json,
                                     TRI_voc_tick_t firstRegularTick,
                                     std::string& errorMsg) {
  // fetch marker "type"
  int typeValue = JsonHelper::getNumericValue<int>(json, "type", 0);

  // fetch "tick"
  std::string const tick = JsonHelper::getStringValue(json, "tick", "");

  if (!tick.empty()) {
    TRI_voc_tick_t newTick = static_cast<TRI_voc_tick_t>(
        StringUtils::uint64(tick.c_str(), tick.size()));

    WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock, 1000);

    if (newTick >= firstRegularTick &&
        newTick > _applier->_state._lastProcessedContinuousTick) {
      _applier->_state._lastProcessedContinuousTick = newTick;
    }
  }

  // handle marker type
  TRI_replication_operation_e type = (TRI_replication_operation_e)typeValue;

  if (type == REPLICATION_MARKER_DOCUMENT || type == REPLICATION_MARKER_EDGE ||
      type == REPLICATION_MARKER_REMOVE) {
    return processDocument(type, json, errorMsg);
  }

  else if (type == REPLICATION_TRANSACTION_START) {
    return startTransaction(json);
  }

  else if (type == REPLICATION_TRANSACTION_ABORT) {
    return abortTransaction(json);
  }

  else if (type == REPLICATION_TRANSACTION_COMMIT) {
    return commitTransaction(json);
  }

  else if (type == REPLICATION_COLLECTION_CREATE) {
    TRI_json_t const* collectionJson = TRI_LookupObjectJson(json, "collection");

    return createCollection(collectionJson, nullptr);
  }

  else if (type == REPLICATION_COLLECTION_DROP) {
    return dropCollection(json, false);
  }

  else if (type == REPLICATION_COLLECTION_RENAME) {
    return renameCollection(json);
  }

  else if (type == REPLICATION_COLLECTION_CHANGE) {
    return changeCollection(json);
  }

  else if (type == REPLICATION_INDEX_CREATE) {
    return createIndex(json);
  }

  else if (type == REPLICATION_INDEX_DROP) {
    return dropIndex(json);
  }

  errorMsg = "unexpected marker type " + StringUtils::itoa(type);

  return TRI_ERROR_REPLICATION_UNEXPECTED_MARKER;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply the data from the continuous log
////////////////////////////////////////////////////////////////////////////////

int ContinuousSyncer::applyLog(SimpleHttpResult* response,
                               TRI_voc_tick_t firstRegularTick,
                               std::string& errorMsg,
                               uint64_t& processedMarkers,
                               uint64_t& ignoreCount) {
  StringBuffer& data = response->getBody();
  char* p = data.begin();
  char* end = p + data.length();

  // buffer must end with a NUL byte
  TRI_ASSERT(*end == '\0');

  while (p < end) {
    char* q = strchr(p, '\n');

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
    *q = '\0';

    processedMarkers++;

    std::unique_ptr<TRI_json_t> json(TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, p));

    p = q + 1;

    if (json == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    if (!TRI_IsObjectJson(json.get())) {
      errorMsg = "received invalid JSON data";

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    int res;
    bool skipped;
    if (skipMarker(firstRegularTick, json.get())) {
      // entry is skipped
      res = TRI_ERROR_NO_ERROR;
      skipped = true;
    } else {
      res = applyLogMarker(json.get(), firstRegularTick, errorMsg);
      skipped = false;
    }

    if (res != TRI_ERROR_NO_ERROR) {
      // apply error

      if (errorMsg.empty()) {
        // don't overwrite previous error message
        errorMsg = TRI_errno_string(res);
      }

      if (ignoreCount == 0) {
        if (lineLength > 256) {
          errorMsg +=
              ", offending marker: " + std::string(lineStart, 256) + "...";
        } else {
          errorMsg +=
              ", offending marker: " + std::string(lineStart, lineLength);
        }

        return res;
      }

      ignoreCount--;
      LOG(WARN) << "ignoring replication error for database '" << _applier->databaseName() << "': " << errorMsg.c_str();
      errorMsg = "";
    }

    // update tick value
    WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock, 1000);

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
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform a continuous sync with the master
////////////////////////////////////////////////////////////////////////////////

int ContinuousSyncer::runContinuousSync(std::string& errorMsg) {
  static uint64_t const MinWaitTime = 300 * 1000;        // 0.30 seconds
  static uint64_t const MaxWaitTime = 60 * 1000 * 1000;  // 60 seconds
  uint64_t connectRetries = 0;
  uint64_t inactiveCycles = 0;

  // get start tick
  // ---------------------------------------
  TRI_voc_tick_t fromTick = 0;
  TRI_voc_tick_t safeResumeTick = 0;

  {
    WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock, 1000);

    if (_useTick) {
      // use user-defined tick
      fromTick = _initialTick;
      _applier->_state._lastAppliedContinuousTick = 0;
      _applier->_state._lastProcessedContinuousTick = 0;
      saveApplierState();
    } else {
      // if we already transferred some data, we'll use the last applied tick
      if (_applier->_state._lastAppliedContinuousTick > fromTick) {
        fromTick = _applier->_state._lastAppliedContinuousTick;
      }
      safeResumeTick = _applier->_state._safeResumeTick;
    }
  }

  if (fromTick == 0) {
    return TRI_ERROR_REPLICATION_NO_START_TICK;
  }

  // get the applier into a sensible start state by fetching the list of
  // open transactions from the master
  TRI_voc_tick_t fetchTick = safeResumeTick;

  if (safeResumeTick > 0 && safeResumeTick == fromTick) {
    // special case in which from and to are equal
    fetchTick = safeResumeTick;
  } else {
    if (_masterIs27OrHigher) {
      int res = fetchMasterState(errorMsg, safeResumeTick, fromTick, fetchTick);

      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }
    } else {
      fetchTick = fromTick;
    }
  }

  if (fetchTick > fromTick) {
    // must not happen
    return TRI_ERROR_INTERNAL;
  }

  LOG(TRACE) << "starting with from tick " << fromTick << ", fetch tick " << fetchTick << ", open transactions: " << _ongoingTransactions.size();

  std::string const progress =
      "starting with from tick " + StringUtils::itoa(fromTick) +
      ", fetch tick " + StringUtils::itoa(fetchTick) + ", open transactions: " +
      StringUtils::itoa(_ongoingTransactions.size());
  setProgress(progress);

  // run in a loop. the loop is terminated when the applier is stopped or an
  // error occurs
  while (true) {
    bool worked;
    bool masterActive = false;
    errorMsg = "";

    // fetchTick is passed by reference!
    int res =
        followMasterLog(errorMsg, fetchTick, fromTick,
                        _configuration._ignoreErrors, worked, masterActive);

    uint64_t sleepTime;

    if (res == TRI_ERROR_REPLICATION_NO_RESPONSE ||
        res == TRI_ERROR_REPLICATION_MASTER_ERROR) {
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
        WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock, 1000);

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
        WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock, 1000);

        _applier->_state._failedConnects = connectRetries;
        _applier->_state._totalRequests++;
      }

      if (res != TRI_ERROR_NO_ERROR) {
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

    LOG(TRACE) << "master active: " << masterActive << ", worked: " << worked << ", sleepTime: " << sleepTime;

    // this will make the applier thread sleep if there is nothing to do,
    // but will also check for cancelation
    if (!_applier->wait(sleepTime)) {
      return TRI_ERROR_REPLICATION_APPLIER_STOPPED;
    }
  }

  // won't be reached
  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch the initial master state
////////////////////////////////////////////////////////////////////////////////

int ContinuousSyncer::fetchMasterState(std::string& errorMsg,
                                       TRI_voc_tick_t fromTick,
                                       TRI_voc_tick_t toTick,
                                       TRI_voc_tick_t& startTick) {
  std::string const baseUrl = BaseUrl + "/determine-open-transactions";
  std::string const url = baseUrl + "?serverId=" + _localServerIdString +
                          "&from=" + StringUtils::itoa(fromTick) + "&to=" +
                          StringUtils::itoa(toTick);

  std::string const progress = "fetching initial master state with from tick " +
                               StringUtils::itoa(fromTick) + ", to tick " +
                               StringUtils::itoa(toTick);

  setProgress(progress);

  LOG(TRACE) << "fetching initial master state with from tick " << fromTick << ", to tick " << toTick << ", url " << url.c_str();

  // send request
  std::unique_ptr<SimpleHttpResult> response(
      _client->request(HttpRequest::HTTP_REQUEST_GET, url, nullptr, 0));

  if (response == nullptr || !response->isComplete()) {
    errorMsg = "got invalid response from master at " +
               std::string(_masterInfo._endpoint) + ": " +
               _client->getErrorMessage();

    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }

  TRI_ASSERT(response != nullptr);

  if (response->wasHttpError()) {
    errorMsg = "got invalid response from master at " +
               std::string(_masterInfo._endpoint) + ": HTTP " +
               StringUtils::itoa(response->getHttpReturnCode()) + ": " +
               response->getHttpReturnMessage();

    return TRI_ERROR_REPLICATION_MASTER_ERROR;
  }

  bool fromIncluded = false;

  bool found;
  std::string header =
      response->getHeaderField(TRI_REPLICATION_HEADER_FROMPRESENT, found);

  if (found) {
    fromIncluded = StringUtils::boolean(header);
  }

  // fetch the tick from where we need to start scanning later
  header = response->getHeaderField(TRI_REPLICATION_HEADER_LASTTICK, found);

  if (!found) {
    errorMsg = "got invalid response from master at " +
               std::string(_masterInfo._endpoint) + ": required header " +
               TRI_REPLICATION_HEADER_LASTTICK + " is missing";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_voc_tick_t readTick = StringUtils::uint64(header);

  if (!fromIncluded && _requireFromPresent && fromTick > 0) {
    errorMsg = "required tick value '" + StringUtils::itoa(fromTick) +
               "' is not present (anymore?) on master at " +
               std::string(_masterInfo._endpoint) +
               ". Last tick available on master is " +
               StringUtils::itoa(readTick) +
               ". It may be required to do a full resync and increase the "
               "number of historic logfiles on the master.";

    return TRI_ERROR_REPLICATION_START_TICK_NOT_PRESENT;
  }

  startTick = readTick;
  if (startTick == 0) {
    startTick = toTick;
  }

  StringBuffer& data = response->getBody();
  std::unique_ptr<TRI_json_t> json(
      TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, data.begin()));

  if (!TRI_IsArrayJson(json.get())) {
    errorMsg = "got invalid response from master at " +
               std::string(_masterInfo._endpoint) +
               ": invalid response type for initial data. expecting array";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  for (size_t i = 0; i < TRI_LengthArrayJson(json.get()); ++i) {
    auto id = static_cast<TRI_json_t const*>(
        TRI_AtVector(&(json.get()->_value._objects), i));

    if (!TRI_IsStringJson(id)) {
      errorMsg =
          "got invalid response from master at " +
          std::string(_masterInfo._endpoint) +
          ": invalid response type for initial data. expecting array of ids";

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    _ongoingTransactions.emplace(
        StringUtils::uint64(id->_value._string.data,
                            id->_value._string.length - 1),
        nullptr);
  }

  {
    std::string const progress =
        "fetched initial master state for from tick " +
        StringUtils::itoa(fromTick) + ", to tick " + StringUtils::itoa(toTick) +
        ", got start tick: " + StringUtils::itoa(readTick) +
        ", open transactions: " + std::to_string(_ongoingTransactions.size());

    setProgress(progress);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run the continuous synchronization
////////////////////////////////////////////////////////////////////////////////

int ContinuousSyncer::followMasterLog(std::string& errorMsg,
                                      TRI_voc_tick_t& fetchTick,
                                      TRI_voc_tick_t firstRegularTick,
                                      uint64_t& ignoreCount, bool& worked,
                                      bool& masterActive) {
  std::string const baseUrl =
      BaseUrl + "/logger-follow?chunkSize=" + _chunkSize;

  worked = false;

  std::string const url = baseUrl + "&from=" + StringUtils::itoa(fetchTick) +
                          "&firstRegular=" +
                          StringUtils::itoa(firstRegularTick) + "&serverId=" +
                          _localServerIdString + "&includeSystem=" +
                          (_includeSystem ? "true" : "false");

  LOG(TRACE) << "running continuous replication request with from tick " << fetchTick << ", first regular tick " << firstRegularTick << ", url " << url.c_str();

  // send request
  std::string const progress =
      "fetching master log from tick " + StringUtils::itoa(fetchTick) +
      ", open transactions: " + std::to_string(_ongoingTransactions.size());
  setProgress(progress);

  std::string body;

  if (_masterIs27OrHigher) {
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
  }

  std::unique_ptr<SimpleHttpResult> response(
      _client->request(_masterIs27OrHigher ? HttpRequest::HTTP_REQUEST_PUT
                                           : HttpRequest::HTTP_REQUEST_GET,
                       url, body.c_str(), body.size()));

  if (response == nullptr || !response->isComplete()) {
    errorMsg = "got invalid response from master at " +
               std::string(_masterInfo._endpoint) + ": " +
               _client->getErrorMessage();

    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }

  TRI_ASSERT(response != nullptr);

  if (response->wasHttpError()) {
    errorMsg = "got invalid response from master at " +
               std::string(_masterInfo._endpoint) + ": HTTP " +
               StringUtils::itoa(response->getHttpReturnCode()) + ": " +
               response->getHttpReturnMessage();

    return TRI_ERROR_REPLICATION_MASTER_ERROR;
  }

  int res = TRI_ERROR_NO_ERROR;
  bool checkMore = false;
  bool active = false;
  bool fromIncluded = false;
  TRI_voc_tick_t tick = 0;

  bool found;
  std::string header =
      response->getHeaderField(TRI_REPLICATION_HEADER_CHECKMORE, found);

  if (found) {
    checkMore = StringUtils::boolean(header);
    res = TRI_ERROR_NO_ERROR;

    // was the specified from value included the result?
    header =
        response->getHeaderField(TRI_REPLICATION_HEADER_FROMPRESENT, found);
    if (found) {
      fromIncluded = StringUtils::boolean(header);
    }

    header = response->getHeaderField(TRI_REPLICATION_HEADER_ACTIVE, found);
    if (found) {
      active = StringUtils::boolean(header);
    }

    header =
        response->getHeaderField(TRI_REPLICATION_HEADER_LASTINCLUDED, found);
    if (found) {
      tick = StringUtils::uint64(header);

      if (tick > fetchTick) {
        fetchTick = tick;
        worked = true;
      } else {
        // we got the same tick again, this indicates we're at the end
        checkMore = false;
      }

      header = response->getHeaderField(TRI_REPLICATION_HEADER_LASTTICK, found);
      if (found) {
        tick = StringUtils::uint64(header);

        WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock, 1000);
        _applier->_state._lastAvailableContinuousTick = tick;
      }
    }
  }

  if (!found) {
    res = TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    errorMsg = "got invalid response from master at " +
               std::string(_masterInfo._endpoint) +
               ": required header is missing";
  }

  if (res == TRI_ERROR_NO_ERROR && !fromIncluded && _requireFromPresent &&
      fetchTick > 0) {
    TRI_ASSERT(_masterIs27OrHigher);
    res = TRI_ERROR_REPLICATION_START_TICK_NOT_PRESENT;
    errorMsg = "required tick value '" + StringUtils::itoa(fetchTick) +
               "' is not present (anymore?) on master at " +
               std::string(_masterInfo._endpoint) +
               ". Last tick available on master is " + StringUtils::itoa(tick) +
               ". It may be required to do a full resync and increase the "
               "number of historic logfiles on the master.";
  }

  if (res == TRI_ERROR_NO_ERROR) {
    TRI_voc_tick_t lastAppliedTick;

    {
      WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock, 1000);
      lastAppliedTick = _applier->_state._lastAppliedContinuousTick;
    }

    uint64_t processedMarkers = 0;
    res = applyLog(response.get(), firstRegularTick, errorMsg, processedMarkers,
                   ignoreCount);

    if (processedMarkers > 0) {
      worked = true;

      WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock, 1000);
      _applier->_state._totalEvents += processedMarkers;

      if (_applier->_state._lastAppliedContinuousTick != lastAppliedTick) {
        _hasWrittenState = true;
        saveApplierState();
      }
    }

    if (!_hasWrittenState && _useTick) {
      // write state at least once so the start tick gets saved
      _hasWrittenState = true;

      WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock, 1000);

      _applier->_state._lastAppliedContinuousTick = firstRegularTick;
      _applier->_state._lastProcessedContinuousTick = firstRegularTick;

      if (_ongoingTransactions.empty() &&
          _applier->_state._safeResumeTick == 0) {
        _applier->_state._safeResumeTick = firstRegularTick;
      }

      saveApplierState();
    }
  }

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  masterActive = active;

  if (!worked) {
    if (checkMore) {
      worked = true;
    }
  }

  return TRI_ERROR_NO_ERROR;
}
