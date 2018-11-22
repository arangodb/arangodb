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

#include "Syncer.h"
#include "Basics/Exceptions.h"
#include "Basics/MutexLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Rest/HttpRequest.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionGuard.h"
#include "Utils/OperationOptions.h"
#include "Utils/OperationResult.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;
using namespace arangodb::httpclient;

/// @brief base url of the replication API
std::string const Syncer::ReplicationUrl = "/_api/replication";

Syncer::Syncer(ReplicationApplierConfiguration const& configuration)
    : _configuration(configuration),
      _masterInfo(),
      _endpoint(nullptr),
      _connection(nullptr),
      _client(nullptr),
      _barrierId(0),
      _barrierTtl(900),
      _barrierUpdateTime(0),
      _isChildSyncer(false) {
  
  MUTEX_LOCKER(locker, _clientMutex);
  TRI_ASSERT(ServerState::instance()->isSingleServer() ||
             ServerState::instance()->isDBServer());
  if (!_configuration._database.empty()) {
    // use name from configuration
    _databaseName = _configuration._database;
  }
 
  if (_configuration._chunkSize == 0) {
    _configuration._chunkSize = 2 * 1024 * 1024; // default: 2 MB
  }
  if (_configuration._chunkSize < 16 * 1024) {
    _configuration._chunkSize = 16 * 1024;
  }

  // get our own server-id
  _localServerId = ServerIdFeature::getId();
  _localServerIdString = StringUtils::itoa(_localServerId);

  _masterInfo._endpoint = _configuration._endpoint;

  _endpoint = Endpoint::clientFactory(_configuration._endpoint);

  if (_endpoint != nullptr) {
    _connection = GeneralClientConnection::factory(
        _endpoint, _configuration._requestTimeout,
        _configuration._connectTimeout,
        (size_t)_configuration._maxConnectRetries,
        (uint32_t)_configuration._sslProtocol);

    if (_connection != nullptr) {
      std::string retryMsg = std::string("retrying failed HTTP request for endpoint '") +
      _configuration._endpoint  + std::string("' for replication applier");
      if (!_databaseName.empty()) {
        retryMsg += std::string(" in database '") + _databaseName + std::string("'");
      }
                  
      SimpleHttpClientParams params(_configuration._requestTimeout, false);
      params.setMaxRetries(2);
      params.setRetryWaitTime(2 * 1000 * 1000); // 2s
      params.setRetryMessage(retryMsg);
      
      std::string username = _configuration._username;
      std::string password = _configuration._password;
      if (!username.empty()) {
        params.setUserNamePassword("/", username, password);
      } else {
        params.setJwt(_configuration._jwt);
      }
      params.setLocationRewriter(this, &rewriteLocation);
      _client = new SimpleHttpClient(_connection, params);
    }
  }
}

Syncer::~Syncer() {
  try {
    sendRemoveBarrier();
  } catch (...) {
  }

  MUTEX_LOCKER(locker, _clientMutex);
  // shutdown everything properly
  delete _client;
  delete _connection;
  delete _endpoint;
}

/// @brief parse a velocypack response
Result Syncer::parseResponse(VPackBuilder& builder,
                             SimpleHttpResult const* response) const {
  try {
    VPackParser parser(builder);
    parser.parse(response->getBody().begin(), response->getBody().length());
    return Result();
  }
  catch (...) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE);
  }
}

/// @brief request location rewriter (injects database name)
std::string Syncer::rewriteLocation(void* data, std::string const& location) {
  Syncer* s = static_cast<Syncer*>(data);
  TRI_ASSERT(s != nullptr);
    if (location.substr(0, 5) == "/_db/") {
      // location already contains /_db/
      return location;
    }
  TRI_ASSERT(!s->_databaseName.empty());
  if (location[0] == '/') {
    return "/_db/" + s->_databaseName + location;
  }
  return "/_db/" + s->_databaseName + "/" + location;
}

/// @brief steal the barrier id from the syncer
TRI_voc_tick_t Syncer::stealBarrier() {
  auto id = _barrierId;
  _barrierId = 0;
  _barrierUpdateTime = 0;
  return id;
}

/// @brief send a "create barrier" command
Result Syncer::sendCreateBarrier(TRI_voc_tick_t minTick) {
  if (_isChildSyncer) {
    return Result();
  }
  _barrierId = 0;

  std::string const url = ReplicationUrl + "/barrier";
  std::string const body = "{\"ttl\":" + StringUtils::itoa(_barrierTtl) +
                           ",\"tick\":\"" + StringUtils::itoa(minTick) + "\"}";

  // send request
  std::unique_ptr<SimpleHttpResult> response(_client->retryRequest(
      rest::RequestType::POST, url, body.c_str(), body.size()));
  
  if (hasFailed(response.get())) {
    return buildHttpError(response.get(), url);
  }

  VPackBuilder builder;
  Result r = parseResponse(builder, response.get());
  if (r.fail()) {
    return r;
  }

  VPackSlice const slice = builder.slice();
  std::string const id = VelocyPackHelper::getStringValue(slice, "id", "");

  if (id.empty()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, "id is missing in create barrier response");
  }

  _barrierId = StringUtils::uint64(id);
  _barrierUpdateTime = TRI_microtime();
  LOG_TOPIC(DEBUG, Logger::REPLICATION) << "created WAL logfile barrier "
                                        << _barrierId;

  return Result();
}

/// @brief send an "extend barrier" command
Result Syncer::sendExtendBarrier(TRI_voc_tick_t tick) {
  if (_isChildSyncer || _barrierId == 0) {
    return Result();
  }

  double now = TRI_microtime();

  if (now <= _barrierUpdateTime + _barrierTtl * 0.25) {
    // no need to extend the barrier yet
    return Result();
  }

  std::string const url = ReplicationUrl + "/barrier/" + StringUtils::itoa(_barrierId);
  std::string const body = "{\"ttl\":" + StringUtils::itoa(_barrierTtl) +
                           ",\"tick\"" + StringUtils::itoa(tick) + "\"}";

  // send request
  std::unique_ptr<SimpleHttpResult> response(_client->request(
      rest::RequestType::PUT, url, body.c_str(), body.size()));

  if (response == nullptr || !response->isComplete()) {
    return Result(TRI_ERROR_REPLICATION_NO_RESPONSE);
  }

  TRI_ASSERT(response != nullptr);

  if (response->wasHttpError()) {
    return Result(TRI_ERROR_REPLICATION_MASTER_ERROR);
  }

  _barrierUpdateTime = TRI_microtime();

  return Result();
}

/// @brief send a "remove barrier" command
Result Syncer::sendRemoveBarrier() {
  if (_isChildSyncer || _barrierId == 0) {
    return Result();
  }

  try {
    std::string const url =
        ReplicationUrl + "/barrier/" + StringUtils::itoa(_barrierId);

    // send request
    std::unique_ptr<SimpleHttpResult> response(_client->retryRequest(
        rest::RequestType::DELETE_REQ, url, nullptr, 0));

    if (hasFailed(response.get())) {
      return buildHttpError(response.get(), url);
    }
    _barrierId = 0;
    _barrierUpdateTime = 0;
    return Result();
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL);
  }
}

void Syncer::setAborted(bool value) {
  MUTEX_LOCKER(locker, _clientMutex);

  if (_client != nullptr) {
    _client->setAborted(value);
  }
}

bool Syncer::isAborted() const {
  MUTEX_LOCKER(locker, _clientMutex);
  if (_client != nullptr) {
    return _client->isAborted();
  }
  return true;
}

/// @brief extract the collection id from VelocyPack
TRI_voc_cid_t Syncer::getCid(VPackSlice const& slice) const {
  return VelocyPackHelper::extractIdValue(slice);
}

/// @brief extract the collection name from VelocyPack
std::string Syncer::getCName(VPackSlice const& slice) const {
  return arangodb::basics::VelocyPackHelper::getStringValue(slice, "cname", "");
}

/// @brief extract the collection by either id or name, may return nullptr!
LogicalCollection* Syncer::getCollectionByIdOrName(TRI_vocbase_t* vocbase,
                                                   TRI_voc_cid_t cid,
                                                   std::string const& name) {
  
  arangodb::LogicalCollection* idCol = vocbase->lookupCollection(cid);
  arangodb::LogicalCollection* nameCol = nullptr;

  if (!name.empty()) {
    // try looking up the collection by name then
    nameCol = vocbase->lookupCollection(name);
  }
  
  if (idCol != nullptr && nameCol != nullptr) {
    if (idCol->cid() == nameCol->cid()) {
      // found collection by id and name, and both are identical!
      return idCol;
    }
    // found different collections by id and name
    TRI_ASSERT(!name.empty());
    if (name[0] == '_') {
      // system collection. always return collection by name when in doubt
      return nameCol;
    }
    
    // no system collection. still prefer local collection
    return nameCol;
  }
  
  if (nameCol != nullptr) {
    TRI_ASSERT(idCol == nullptr);
    return nameCol;
  }
  
  // may be nullptr
  return idCol;
}

TRI_vocbase_t* Syncer::resolveVocbase(VPackSlice const& slice) {
  std::string name;
  if (slice.isObject()) {
    VPackSlice tmp;
    if ((tmp = slice.get("db")).isString()) { // wal access protocol
      name = tmp.copyString();
    } else if ((tmp = slice.get("database")).isString()) { // pre 3.3
      name = tmp.copyString();
    }
  } else if (slice.isString()) {
    name = slice.copyString();
  }
  if (name.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                                   "could not resolve vocbase id / name");
  }
  
  // will work with either names or id's
  auto const& it = _vocbases.find(name);
  if (it == _vocbases.end()) {
    // automatically checks for id in string
    TRI_vocbase_t* vocbase = DatabaseFeature::DATABASE->lookupDatabase(name);
    if (vocbase != nullptr) {
      _vocbases.emplace(name, DatabaseGuard(vocbase));
    } else {
      LOG_TOPIC(DEBUG, Logger::REPLICATION) << "could not find database '" << name << "'";
    }
    return vocbase;
  } else {
    return it->second.database();
  }
}

arangodb::LogicalCollection* Syncer::resolveCollection(TRI_vocbase_t* vocbase,
                                                       VPackSlice const& slice) {
  TRI_ASSERT(vocbase != nullptr);
  // extract "cid"
  TRI_voc_cid_t cid = getCid(slice);
  if (!simulate32Client() || cid == 0) {
    VPackSlice uuid;
    if ((uuid = slice.get("cuid")).isString()) {
      return vocbase->lookupCollectionByUuid(uuid.copyString());
    } else if ((uuid = slice.get("globallyUniqueId")).isString()) {
      return vocbase->lookupCollectionByUuid(uuid.copyString());
    }
  }
  
  if (cid == 0) {
    LOG_TOPIC(ERR, Logger::REPLICATION) <<
      TRI_errno_string(TRI_ERROR_REPLICATION_INVALID_RESPONSE);
    return nullptr;
  }
  // extract optional "cname"
  std::string cname = getCName(slice);
  if (cname.empty()) {
    cname = arangodb::basics::VelocyPackHelper::getStringValue(slice, "name", "");
  }
  return getCollectionByIdOrName(vocbase, cid, cname);
}

Result Syncer::applyCollectionDumpMarker(
    transaction::Methods& trx, LogicalCollection* coll,
    TRI_replication_operation_e type, VPackSlice const& old, 
    VPackSlice const& slice) {

  if (_configuration._lockTimeoutRetries > 0) {
    decltype(_configuration._lockTimeoutRetries) tries = 0;

    while (true) {
      Result res = applyCollectionDumpMarkerInternal(trx, coll, type, old, slice);

      if (res.errorNumber() != TRI_ERROR_LOCK_TIMEOUT) {
        return res;
      }

      // lock timeout
      if (++tries > _configuration._lockTimeoutRetries) {
        // timed out
        return res;
      }
     
      usleep(50000); 
      // retry
    }
  } else {
    return applyCollectionDumpMarkerInternal(trx, coll, type, old, slice);
  }
}

/// @brief apply the data from a collection dump or the continuous log
Result Syncer::applyCollectionDumpMarkerInternal(
      transaction::Methods& trx, LogicalCollection* coll,
      TRI_replication_operation_e type, VPackSlice const& old, 
      VPackSlice const& slice) {

  if (type == REPLICATION_MARKER_DOCUMENT) {
    // {"type":2400,"key":"230274209405676","data":{"_key":"230274209405676","_rev":"230274209405676","foo":"bar"}}

    OperationOptions options;
    options.silent = true;
    options.ignoreRevs = true;
    options.isRestore = true;
    if (!_leaderId.empty()) {
      options.isSynchronousReplicationFrom = _leaderId;
    }
    // we want the conflicting other key returned in case of unique constraint violation
    options.indexOperationMode = Index::OperationMode::internal;

    try {
      // try insert first
      OperationResult opRes = trx.insert(coll->name(), slice, options); 

      if (opRes.is(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED)) {
        bool useReplace = true;

        // conflicting key is contained in opRes.errorMessage() now
        VPackSlice keySlice = slice.get(StaticStrings::KeyString);
        
        if (keySlice.isString()) {
          // let's check if the key we have got is the same as the one
          // that we would like to insert
          if (keySlice.copyString() != opRes.errorMessage()) {
            // different key
            VPackBuilder tmp;
            tmp.add(VPackValue(opRes.errorMessage()));

            opRes = trx.remove(coll->name(), tmp.slice(), options);
            if (opRes.ok() || !opRes.is(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
              useReplace = false;
            }
          }
        }
    
        options.indexOperationMode = Index::OperationMode::normal;

        if (useReplace) {
          // perform a replace
          opRes = trx.replace(coll->name(), slice, options);
        } else {
          // perform a re-insert
          opRes = trx.insert(coll->name(), slice, options); 
        }
      }
   
      return Result(opRes.result);
    } catch (arangodb::basics::Exception const& ex) {
      return Result(ex.code(), std::string("document insert/replace operation failed: ") + ex.what());
    } catch (std::exception const& ex) {
      return Result(TRI_ERROR_INTERNAL, std::string("document insert/replace operation failed: ") + ex.what());
    } catch (...) {
      return Result(TRI_ERROR_INTERNAL, std::string("document insert/replace operation failed: unknown exception"));
    }
  }

  else if (type == REPLICATION_MARKER_REMOVE) {
    // {"type":2402,"key":"592063"}
    
    try {
      OperationOptions options;
      options.silent = true;
      options.ignoreRevs = true;
      if (!_leaderId.empty()) {
        options.isSynchronousReplicationFrom = _leaderId;
      }
      OperationResult opRes = trx.remove(coll->name(), old, options);

      if (opRes.ok() ||
          opRes.is(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
        // ignore document not found errors
        return Result();
      }
      
      return Result(opRes.result);
    } catch (arangodb::basics::Exception const& ex) {
      return Result(ex.code(), std::string("document remove operation failed: ") + ex.what());
    } catch (std::exception const& ex) {
      return Result(TRI_ERROR_INTERNAL, std::string("document remove operation failed: ") + ex.what());
    } catch (...) {
      return Result(TRI_ERROR_INTERNAL, std::string("document remove operation failed: unknown exception"));
    }
  }
    
  return Result(TRI_ERROR_REPLICATION_UNEXPECTED_MARKER, std::string("unexpected marker type ") + StringUtils::itoa(type));
}

/// @brief creates a collection, based on the VelocyPack provided
Result Syncer::createCollection(TRI_vocbase_t* vocbase, 
                                VPackSlice const& slice,
                                LogicalCollection** dst) {
  if (dst != nullptr) {
    *dst = nullptr;
  }

  if (!slice.isObject()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, "collection slice is no object");
  }

  std::string const name = VelocyPackHelper::getStringValue(slice, "name", "");
  if (name.empty()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, "no name specified for collection");
  }

  TRI_col_type_e const type = static_cast<TRI_col_type_e>(VelocyPackHelper::getNumericValue<int>(
      slice, "type", TRI_COL_TYPE_DOCUMENT));

  // resolve collection by uuid, name, cid (in that order of preference)
  arangodb::LogicalCollection* col = resolveCollection(vocbase, slice);

  if (col != nullptr && 
      col->type() == type && 
      (!simulate32Client() || col->name() == name)) {
    // collection already exists. TODO: compare attributes
    return Result();
  }
   
  bool forceRemoveCid = false;
  if (col != nullptr && simulate32Client()) {
    forceRemoveCid = true;
    LOG_TOPIC(TRACE, Logger::REPLICATION) << "would have got a wrong collection!";
    // go on now and truncate or drop/re-create the collection
  }
  
  // conflicting collections need to be dropped from 3.3 onwards
  col = vocbase->lookupCollection(name);
  if (col != nullptr) {
    if (col->isSystem()) {
      TRI_ASSERT(!simulate32Client() || col->globallyUniqueId() == col->name());
      
      SingleCollectionTransaction trx(transaction::StandaloneContext::Create(vocbase),
                                      col->cid(), AccessMode::Type::WRITE);
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
    } else {
      vocbase->dropCollection(col, false, -1.0);
    }
  }
  
  VPackSlice uuid = slice.get("globallyUniqueId");
  // merge in "isSystem" attribute, doesn't matter if name does not start with '_'
  VPackBuilder s;
  s.openObject();
  s.add("isSystem", VPackValue(true));
  s.add("objectId", VPackSlice::nullSlice());
  if ((uuid.isString() && !simulate32Client()) || forceRemoveCid) { // need to use cid for 3.2 master
    // if we received a globallyUniqueId from the remote, then we will always use this id
    // so we can discard the "cid" and "id" values for the collection
    s.add("id", VPackSlice::nullSlice());
    s.add("cid", VPackSlice::nullSlice());
  }
  s.close();

  VPackBuilder merged = VPackCollection::merge(slice, s.slice(),
                                               /*mergeValues*/true, /*nullMeansRemove*/true);
  
  try {
    col = vocbase->createCollection(merged.slice());
  } catch (basics::Exception const& ex) {
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL);
  }

  TRI_ASSERT(col != nullptr);
  TRI_ASSERT(!uuid.isString() ||
             uuid.compareString(col->globallyUniqueId()) == 0);

  if (dst != nullptr) {
    *dst = col;
  }
  return Result();
}

/// @brief drops a collection, based on the VelocyPack provided
Result Syncer::dropCollection(VPackSlice const& slice, bool reportError) {
  TRI_vocbase_t* vocbase = resolveVocbase(slice);
  if (vocbase == nullptr) {
    return Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }
  arangodb::LogicalCollection* col = resolveCollection(vocbase, slice);
  if (col == nullptr) {
    if (reportError) {
      return Result(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    }

    return Result();
  }
    
  return Result(vocbase->dropCollection(col, true, -1.0));
}

/// @brief creates an index, based on the VelocyPack provided
Result Syncer::createIndex(VPackSlice const& slice) {
  VPackSlice indexSlice = slice.get("index");
  if (!indexSlice.isObject()) {
    indexSlice = slice.get("data");
  }
  
  if (!indexSlice.isObject()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, "index slice is not an object");
  }

  TRI_vocbase_t* vocbase = resolveVocbase(slice);
  if (vocbase == nullptr) {
    return Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }
  arangodb::LogicalCollection* col = resolveCollection(vocbase, slice);
  if (col == nullptr) {
    return Result(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                  "did not find collection for index");
  }
  
  VPackBuilder s;
  s.openObject();
  s.add("objectId", VPackSlice::nullSlice());
  s.close();
  VPackBuilder merged = VPackCollection::merge(indexSlice, s.slice(),
                                               /*mergeValues*/true, /*nullMeansRemove*/true);

  try {
    SingleCollectionTransaction trx(transaction::StandaloneContext::Create(vocbase),
                                    col->cid(), AccessMode::Type::WRITE);

    Result res = trx.begin();

    if (!res.ok()) {
      return res;
    }

    auto physical = trx.documentCollection()->getPhysical();
    TRI_ASSERT(physical != nullptr);
    std::shared_ptr<arangodb::Index> idx;
    res = physical->restoreIndex(&trx, merged.slice(), idx);
    res = trx.finish(res);

    return res;
  } catch (arangodb::basics::Exception const& ex) {
    return Result(ex.code(), std::string("caught exception while creating index: ") + ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, std::string("caught exception while creating index: ") + ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL, "caught unknown exception while creating index");
  }
}

Result Syncer::dropIndex(arangodb::velocypack::Slice const& slice) {
  auto cb = [&](VPackSlice const& slice) {
    std::string id;
    if (slice.hasKey("data")) {
      id = VelocyPackHelper::getStringValue(slice.get("data"), "id", "");
    } else {
      id = VelocyPackHelper::getStringValue(slice, "id", "");
    }
    
    if (id.empty()) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, "id not found in index drop slice");
    }

    TRI_idx_iid_t const iid = StringUtils::uint64(id);

    TRI_vocbase_t* vocbase = resolveVocbase(slice);
    if (vocbase == nullptr) {
      return Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    }
    arangodb::LogicalCollection* col = resolveCollection(vocbase, slice);
    if (col == nullptr) {
      return Result(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    }

    try {
      CollectionGuard guard(vocbase, col);
      bool result = guard.collection()->dropIndex(iid);
      if (!result) {
        return Result(); // TODO: why do we ignore failures here?
      }

      return Result();
    } catch (arangodb::basics::Exception const& ex) {
      return Result(ex.code(), ex.what());
    } catch (std::exception const& ex) {
      return Result(TRI_ERROR_INTERNAL, ex.what());
    } catch (...) {
      return Result(TRI_ERROR_INTERNAL);
    }
  };

  Result r = cb(slice);
  if (r.fail() &&
      (r.is(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) ||
       r.is(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND))) {
    // if dropping an index for a non-existing database or collection fails, this is not a real problem
    return Result();
  }

  return r;
}

/// @brief get master state
Result Syncer::getMasterState() {
  if (_isChildSyncer) {
    TRI_ASSERT(!_masterInfo._endpoint.empty());
    TRI_ASSERT(_masterInfo._serverId != 0);
    TRI_ASSERT(_masterInfo._majorVersion != 0);
    return Result();
  }
  
  std::string const url =
      ReplicationUrl + "/logger-state?serverId=" + _localServerIdString;

  // store old settings
  size_t maxRetries = _client->params().getMaxRetries();
  uint64_t retryWaitTime = _client->params().getRetryWaitTime();

  // apply settings that prevent endless waiting here
  _client->params().setMaxRetries(1);
  _client->params().setRetryWaitTime(500 * 1000); // 0.5s

  std::unique_ptr<SimpleHttpResult> response(
      _client->retryRequest(rest::RequestType::GET, url, nullptr, 0));

  // restore old settings
  _client->params().setMaxRetries(maxRetries);
  _client->params().setRetryWaitTime(retryWaitTime);
    
  if (hasFailed(response.get())) {
    return buildHttpError(response.get(), url);
  }

  VPackBuilder builder;
  Result r = parseResponse(builder, response.get());
  if (r.fail()) {
    return r;
  }
    
  VPackSlice const slice = builder.slice();

  if (!slice.isObject()) {
    LOG_TOPIC(DEBUG, Logger::REPLICATION) << "syncer::getMasterState - state is not an object";
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") + _masterInfo._endpoint + ": invalid JSON");
  }
  
  return handleStateResponse(slice);
}

/// @brief handle the state response of the master
Result Syncer::handleStateResponse(VPackSlice const& slice) {
  std::string const endpointString =
      " from endpoint '" + _masterInfo._endpoint + "'";

  // process "state" section
  VPackSlice const state = slice.get("state");

  if (!state.isObject()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("state section is missing in response") + endpointString);
  }

  // state."lastLogTick"
  VPackSlice const tick = state.get("lastLogTick");

  if (!tick.isString()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("lastLogTick is missing in response") + endpointString);
  }

  TRI_voc_tick_t const lastLogTick = VelocyPackHelper::stringUInt64(tick);

  if (lastLogTick == 0) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("lastLogTick is 0 in response") + endpointString);
  }

  // state."running"
  bool running = VelocyPackHelper::getBooleanValue(state, "running", false);

  // process "server" section
  VPackSlice const server = slice.get("server");

  if (!server.isObject()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("server section is missing in response") + endpointString);
  }

  // server."version"
  VPackSlice const version = server.get("version");

  if (!version.isString()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("server version is missing in response") + endpointString);
  }

  // server."serverId"
  VPackSlice const serverId = server.get("serverId");

  if (!serverId.isString()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("server id is missing in response") + endpointString);
  }

  // validate all values we got
  std::string const masterIdString(serverId.copyString());
  TRI_server_id_t const masterId = StringUtils::uint64(masterIdString);

  if (masterId == 0) {
    // invalid master id
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("invalid server id in response") + endpointString);
  }

  if (masterIdString == _localServerIdString) {
    // master and replica are the same instance. this is not supported.
    return Result(TRI_ERROR_REPLICATION_LOOP, std::string("got same server id (") + _localServerIdString + ")" + endpointString + " as the local applier server's id");
  }

  int major = 0;
  int minor = 0;

  std::string const versionString(version.copyString());

  if (sscanf(versionString.c_str(), "%d.%d", &major, &minor) != 2) {
    return Result(TRI_ERROR_REPLICATION_MASTER_INCOMPATIBLE, std::string("invalid master version info") + endpointString + ": '" + versionString + "'");
  }

  if (major != 3) {
    // we can connect to 3.x only
    return Result(TRI_ERROR_REPLICATION_MASTER_INCOMPATIBLE, std::string("got incompatible master version") + endpointString + ": '" + versionString + "'");
  }

  _masterInfo._majorVersion = major;
  _masterInfo._minorVersion = minor;
  _masterInfo._serverId = masterId;
  _masterInfo._lastLogTick = lastLogTick;
  _masterInfo._active = running;

  LOG_TOPIC(INFO, Logger::REPLICATION)
      << "connected to master at " << _masterInfo._endpoint << ", id "
      << _masterInfo._serverId << ", version " << _masterInfo._majorVersion
      << "." << _masterInfo._minorVersion << ", last log tick "
      << _masterInfo._lastLogTick;

  return Result();
}

void Syncer::reloadUsers() {
  AuthenticationFeature* af = AuthenticationFeature::instance();
  auth::UserManager* um = af->userManager();
  if (um != nullptr) {
    um->outdate();
  }
}
  
bool Syncer::hasFailed(SimpleHttpResult* response) const {
  return (response == nullptr || !response->isComplete() || response->wasHttpError());
}

Result Syncer::buildHttpError(SimpleHttpResult* response, std::string const& url) const {
  TRI_ASSERT(hasFailed(response));

  if (response == nullptr || !response->isComplete()) {
    return Result(TRI_ERROR_REPLICATION_NO_RESPONSE, std::string("could not connect to master at ") + _masterInfo._endpoint + " for URL " + url + ": " + _client->getErrorMessage());
  }

  TRI_ASSERT(response->wasHttpError());
  return Result(TRI_ERROR_REPLICATION_MASTER_ERROR, std::string("got invalid response from master at ") +
                _masterInfo._endpoint + " for URL " + url + ": HTTP " + StringUtils::itoa(response->getHttpReturnCode()) + ": " +
                response->getHttpReturnMessage() + " - " + response->getBody().toString());
}

bool Syncer::simulate32Client() const {
  TRI_ASSERT(!_masterInfo._endpoint.empty() && _masterInfo._serverId != 0 &&
             _masterInfo._majorVersion != 0);
  bool is33 = (_masterInfo._majorVersion > 3 ||
               (_masterInfo._majorVersion >= 3 && _masterInfo._minorVersion >= 3));
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // allows us to test the old replication API
  return !is33 || _configuration._force32mode;
#else
  return !is33;
#endif
}
