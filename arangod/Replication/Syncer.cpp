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
#include "Basics/VelocyPackHelper.h"
#include "Rest/HttpRequest.h"
#include "RestServer/ServerIdFeature.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
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

////////////////////////////////////////////////////////////////////////////////
/// @brief base url of the replication API
////////////////////////////////////////////////////////////////////////////////

std::string const Syncer::BaseUrl = "/_api/replication";

Syncer::Syncer(TRI_vocbase_t* vocbase,
               TRI_replication_applier_configuration_t const* configuration)
    : _vocbase(vocbase),
      _configuration(),
      _masterInfo(),
      _endpoint(nullptr),
      _connection(nullptr),
      _client(nullptr),
      _barrierId(0),
      _barrierUpdateTime(0),
      _barrierTtl(600) {
  if (configuration->_database.empty()) {
    // use name of current database
    _databaseName = vocbase->name();
  } else {
    // use name from configuration
    _databaseName = configuration->_database;
  }

  // get our own server-id
  _localServerId = ServerIdFeature::getId();
  _localServerIdString = StringUtils::itoa(_localServerId);

  _configuration.update(configuration);
  _useCollectionId = _configuration._useCollectionId;

  _masterInfo._endpoint = configuration->_endpoint;

  _endpoint = Endpoint::clientFactory(_configuration._endpoint);

  if (_endpoint != nullptr) {
    _connection = GeneralClientConnection::factory(
        _endpoint, _configuration._requestTimeout,
        _configuration._connectTimeout,
        (size_t)_configuration._maxConnectRetries,
        (uint32_t)_configuration._sslProtocol);

    if (_connection != nullptr) {
      SimpleHttpClientParams params(_configuration._requestTimeout, false);
      params.setMaxRetries(2);
      params.setRetryWaitTime(2 * 1000 * 1000);
      params.setRetryMessage(std::string("retrying failed HTTP request for endpoint '") +
                             _configuration._endpoint +
                             std::string("' for replication applier in database '" +
                                         _vocbase->name() + "'"));
      
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

  // shutdown everything properly
  delete _client;
  delete _connection;
  delete _endpoint;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse a velocypack response
////////////////////////////////////////////////////////////////////////////////

int Syncer::parseResponse(std::shared_ptr<VPackBuilder> builder, 
                          SimpleHttpResult const* response) const {
  try {
    VPackParser parser(builder);
    parser.parse(response->getBody().begin(), response->getBody().length());
    return TRI_ERROR_NO_ERROR;
  }
  catch (...) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief request location rewriter (injects database name)
////////////////////////////////////////////////////////////////////////////////

std::string Syncer::rewriteLocation(void* data, std::string const& location) {
  Syncer* s = static_cast<Syncer*>(data);

  TRI_ASSERT(s != nullptr);

  if (location.substr(0, 5) == "/_db/") {
    // location already contains /_db/
    return location;
  }

  if (location[0] == '/') {
    return "/_db/" + s->_databaseName + location;
  }
  return "/_db/" + s->_databaseName + "/" + location;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief steal the barrier id from the syncer
////////////////////////////////////////////////////////////////////////////////

TRI_voc_tick_t Syncer::stealBarrier() {
  auto id = _barrierId;
  _barrierId = 0;
  _barrierUpdateTime = 0;
  return id;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send a "create barrier" command
////////////////////////////////////////////////////////////////////////////////

int Syncer::sendCreateBarrier(std::string& errorMsg, TRI_voc_tick_t minTick) {
  _barrierId = 0;

  std::string const url = BaseUrl + "/barrier";
  std::string const body = "{\"ttl\":" + StringUtils::itoa(_barrierTtl) +
                           ",\"tick\":\"" + StringUtils::itoa(minTick) + "\"}";

  // send request
  std::unique_ptr<SimpleHttpResult> response(_client->retryRequest(
      rest::RequestType::POST, url, body.c_str(), body.size()));

  if (response == nullptr || !response->isComplete()) {
    errorMsg = "could not connect to master at " + _masterInfo._endpoint +
               ": " + _client->getErrorMessage();

    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }

  TRI_ASSERT(response != nullptr);

  int res = TRI_ERROR_NO_ERROR;

  if (response->wasHttpError()) {
    res = TRI_ERROR_REPLICATION_MASTER_ERROR;

    errorMsg = "got invalid response from master at " + _masterInfo._endpoint +
               ": HTTP " + StringUtils::itoa(response->getHttpReturnCode()) +
               ": " + response->getHttpReturnMessage();
  } else {
    auto builder = std::make_shared<VPackBuilder>();
    res = parseResponse(builder, response.get());

    if (res == TRI_ERROR_NO_ERROR) {
      VPackSlice const slice = builder->slice();
      std::string const id = VelocyPackHelper::getStringValue(slice, "id", "");

      if (id.empty()) {
        res = TRI_ERROR_REPLICATION_INVALID_RESPONSE;
      } else {
        _barrierId = StringUtils::uint64(id);
        _barrierUpdateTime = TRI_microtime();
        LOG_TOPIC(DEBUG, Logger::REPLICATION) << "created WAL logfile barrier "
                                              << _barrierId;
      }
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send an "extend barrier" command
////////////////////////////////////////////////////////////////////////////////

int Syncer::sendExtendBarrier(TRI_voc_tick_t tick) {
  if (_barrierId == 0) {
    return TRI_ERROR_NO_ERROR;
  }

  double now = TRI_microtime();

  if (now <= _barrierUpdateTime + _barrierTtl - 120.0) {
    // no need to extend the barrier yet
    return TRI_ERROR_NO_ERROR;
  }

  std::string const url = BaseUrl + "/barrier/" + StringUtils::itoa(_barrierId);
  std::string const body = "{\"ttl\":" + StringUtils::itoa(_barrierTtl) +
                           ",\"tick\"" + StringUtils::itoa(tick) + "\"}";

  // send request
  std::unique_ptr<SimpleHttpResult> response(_client->request(
      rest::RequestType::PUT, url, body.c_str(), body.size()));

  if (response == nullptr || !response->isComplete()) {
    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }

  TRI_ASSERT(response != nullptr);

  int res = TRI_ERROR_NO_ERROR;

  if (response->wasHttpError()) {
    res = TRI_ERROR_REPLICATION_MASTER_ERROR;
  } else {
    _barrierUpdateTime = TRI_microtime();
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send a "remove barrier" command
////////////////////////////////////////////////////////////////////////////////

int Syncer::sendRemoveBarrier() {
  if (_barrierId == 0) {
    return TRI_ERROR_NO_ERROR;
  }

  try {
    std::string const url =
        BaseUrl + "/barrier/" + StringUtils::itoa(_barrierId);

    // send request
    std::unique_ptr<SimpleHttpResult> response(_client->retryRequest(
        rest::RequestType::DELETE_REQ, url, nullptr, 0));

    if (response == nullptr || !response->isComplete()) {
      return TRI_ERROR_REPLICATION_NO_RESPONSE;
    }

    TRI_ASSERT(response != nullptr);

    int res = TRI_ERROR_NO_ERROR;

    if (response->wasHttpError()) {
      res = TRI_ERROR_REPLICATION_MASTER_ERROR;
    } else {
      _barrierId = 0;
      _barrierUpdateTime = 0;
    }
    return res;
  } catch (...) {
    return TRI_ERROR_INTERNAL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the collection id from VelocyPack
////////////////////////////////////////////////////////////////////////////////

TRI_voc_cid_t Syncer::getCid(VPackSlice const& slice) const {
  return VelocyPackHelper::extractIdValue(slice);
}

///////////////////////////////////////////////////////////////////////////////
/// @brief extract the collection name from VelocyPack
////////////////////////////////////////////////////////////////////////////////

std::string Syncer::getCName(VPackSlice const& slice) const {
  return arangodb::basics::VelocyPackHelper::getStringValue(slice, "cname", "");
}

///////////////////////////////////////////////////////////////////////////////
/// @brief extract the collection by either id or name, may return nullptr!
////////////////////////////////////////////////////////////////////////////////
  
arangodb::LogicalCollection* Syncer::getCollectionByIdOrName(TRI_voc_cid_t cid, std::string const& name) { 
  arangodb::LogicalCollection* idCol = nullptr;
  arangodb::LogicalCollection* nameCol = nullptr;

  if (_useCollectionId) {
    idCol = _vocbase->lookupCollection(cid);
  }

  if (!name.empty()) {
    // try looking up the collection by name then
    nameCol = _vocbase->lookupCollection(name);
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

////////////////////////////////////////////////////////////////////////////////
/// @brief apply the data from a collection dump or the continuous log
////////////////////////////////////////////////////////////////////////////////

int Syncer::applyCollectionDumpMarker(
    transaction::Methods& trx, std::string const& collectionName,
    TRI_replication_operation_e type, VPackSlice const& old, 
    VPackSlice const& slice, std::string& errorMsg) {

  int res = TRI_ERROR_INTERNAL;

  if (type == REPLICATION_MARKER_DOCUMENT) {
    // {"type":2400,"key":"230274209405676","data":{"_key":"230274209405676","_rev":"230274209405676","foo":"bar"}}

    OperationOptions options;
    options.silent = true;
    options.ignoreRevs = true;
    options.isRestore = true;
    if (!_leaderId.empty()) {
      options.isSynchronousReplicationFrom = _leaderId;
    }

    try {
      // try insert first
      OperationResult opRes = trx.insert(collectionName, slice, options); 

      if (opRes.code == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) {
        // perform an update
        opRes = trx.replace(collectionName, slice, options); 
      }
    
      res = opRes.code;
    } catch (arangodb::basics::Exception const& ex) {
      res = ex.code();
      errorMsg = "document insert/replace operation failed: " +
                 std::string(TRI_errno_string(res));
    } catch (std::exception const& ex) {
      res = TRI_ERROR_INTERNAL;
      errorMsg = "document insert/replace operation failed: " +
                 std::string(ex.what());
    } catch (...) {
      res = TRI_ERROR_INTERNAL;
      errorMsg = "document insert/replace operation failed: unknown exception";
    }

    return res;
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
      OperationResult opRes = trx.remove(collectionName, old, options);

      if (opRes.successful() ||
          opRes.code == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        // ignore document not found errors
        return TRI_ERROR_NO_ERROR;
      }

      res = opRes.code;
    } catch (arangodb::basics::Exception const& ex) {
      res = ex.code();
      errorMsg = "document remove operation failed: " +
                 std::string(TRI_errno_string(res));
    } catch (std::exception const& ex) {
      errorMsg = "document remove operation failed: " +
                 std::string(ex.what());
    } catch (...) {
      res = TRI_ERROR_INTERNAL;
      errorMsg = "document remove operation failed: unknown exception";
    }
    
    return res;
  }
    
  res = TRI_ERROR_REPLICATION_UNEXPECTED_MARKER;
  errorMsg = "unexpected marker type " + StringUtils::itoa(type);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection, based on the VelocyPack provided
////////////////////////////////////////////////////////////////////////////////

int Syncer::createCollection(VPackSlice const& slice, arangodb::LogicalCollection** dst) {
  if (dst != nullptr) {
    *dst = nullptr;
  }

  if (!slice.isObject()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  std::string const name = VelocyPackHelper::getStringValue(slice, "name", "");

  if (name.empty()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_voc_cid_t const cid = getCid(slice);

  if (cid == 0) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_col_type_e const type = static_cast<TRI_col_type_e>(VelocyPackHelper::getNumericValue<int>(
      slice, "type", TRI_COL_TYPE_DOCUMENT));

  arangodb::LogicalCollection* col = getCollectionByIdOrName(cid, name);

  if (col != nullptr && col->type() == type) {
    // collection already exists. TODO: compare attributes
    return TRI_ERROR_NO_ERROR;
  }

  // merge in "isSystem" attribute
  VPackBuilder s;
  s.openObject();
  s.add("isSystem", VPackValue(true));
  s.close();

  VPackBuilder merged = VPackCollection::merge(s.slice(), slice, true);

  int res = TRI_ERROR_NO_ERROR;
  try {
    col = _vocbase->createCollection(merged.slice());
  } catch (basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  TRI_ASSERT(col != nullptr);

  if (dst != nullptr) {
    *dst = col;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a collection, based on the VelocyPack provided
////////////////////////////////////////////////////////////////////////////////

int Syncer::dropCollection(VPackSlice const& slice, bool reportError) {
  arangodb::LogicalCollection* col = getCollectionByIdOrName(getCid(slice), getCName(slice));

  if (col == nullptr) {
    if (reportError) {
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    }

    return TRI_ERROR_NO_ERROR;
  }

  return _vocbase->dropCollection(col, true, -1.0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an index, based on the VelocyPack provided
////////////////////////////////////////////////////////////////////////////////

int Syncer::createIndex(VPackSlice const& slice) {
  VPackSlice indexSlice = slice.get("index");
  if (!indexSlice.isObject()) {
    indexSlice = slice.get("data");
  }

  if (!indexSlice.isObject()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_voc_cid_t cid = getCid(slice);
  std::string cnameString = getCName(slice);

  try {
    CollectionGuard guard(_vocbase, cid, cnameString);

    if (guard.collection() == nullptr) {
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    }

    LogicalCollection* collection = guard.collection();

    SingleCollectionTransaction trx(transaction::StandaloneContext::Create(_vocbase), guard.collection()->cid(), AccessMode::Type::WRITE);

    Result res = trx.begin();

    if (!res.ok()) {
      return res.errorNumber();
    }

    auto physical = collection->getPhysical();
    TRI_ASSERT(physical != nullptr);
    std::shared_ptr<arangodb::Index> idx;
    res = physical->restoreIndex(&trx, indexSlice, idx);
    res = trx.finish(res);

    return res.errorNumber();
  } catch (arangodb::basics::Exception const& ex) {
    return ex.code();
  } catch (...) {
    return TRI_ERROR_INTERNAL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an index, based on the VelocyPack provided
////////////////////////////////////////////////////////////////////////////////

int Syncer::dropIndex(arangodb::velocypack::Slice const& slice) {
  std::string id;
  if (slice.hasKey("data")) {
    id = VelocyPackHelper::getStringValue(slice.get("data"), "id", "");
  } else {
    id = VelocyPackHelper::getStringValue(slice, "id", "");
  }

  if (id.empty()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_idx_iid_t const iid = StringUtils::uint64(id);

  TRI_voc_cid_t const cid = getCid(slice);
  std::string cnameString = getCName(slice);

  try {
    CollectionGuard guard(_vocbase, cid, cnameString);

    if (guard.collection() == nullptr) {
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    }

    LogicalCollection* collection = guard.collection();

    bool result = collection->dropIndex(iid);

    if (!result) {
      return TRI_ERROR_NO_ERROR;
    }

    return TRI_ERROR_NO_ERROR;
  } catch (arangodb::basics::Exception const& ex) {
    return ex.code();
  } catch (...) {
    return TRI_ERROR_INTERNAL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get master state
////////////////////////////////////////////////////////////////////////////////

int Syncer::getMasterState(std::string& errorMsg) {
  std::string const url =
      BaseUrl + "/logger-state?serverId=" + _localServerIdString;

  // store old settings
  size_t maxRetries = _client->params().getMaxRetries();
  uint64_t retryWaitTime = _client->params().getRetryWaitTime();

  // apply settings that prevent endless waiting here
  _client->params().setMaxRetries(1);
  _client->params().setRetryWaitTime(500 * 1000);

  std::unique_ptr<SimpleHttpResult> response(
      _client->retryRequest(rest::RequestType::GET, url, nullptr, 0));

  // restore old settings
  _client->params().setMaxRetries(maxRetries);
  _client->params().setRetryWaitTime(retryWaitTime);

  if (response == nullptr || !response->isComplete()) {
    errorMsg = "could not connect to master at " + _masterInfo._endpoint +
               ": " + _client->getErrorMessage();

    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }

  if (response->wasHttpError()) {
    errorMsg = "got invalid response from master at " + _masterInfo._endpoint +
               ": HTTP " + StringUtils::itoa(response->getHttpReturnCode()) +
               ": " + response->getHttpReturnMessage();
    return TRI_ERROR_REPLICATION_MASTER_ERROR;
  }

  auto builder = std::make_shared<VPackBuilder>();
  int res = parseResponse(builder, response.get());

  if (res == TRI_ERROR_NO_ERROR) {
    VPackSlice const slice = builder->slice();

    if (!slice.isObject()) {
      LOG_TOPIC(DEBUG, Logger::REPLICATION) << "syncer::getMasterState - state is not an object";
      res = TRI_ERROR_REPLICATION_INVALID_RESPONSE;
      errorMsg = "got invalid response from master at " +
                 _masterInfo._endpoint + ": invalid JSON";
    }
    else {
      res = handleStateResponse(slice, errorMsg);
    }
  }

  if (res != TRI_ERROR_NO_ERROR){
    LOG_TOPIC(DEBUG, Logger::REPLICATION) << "syncer::getMasterState - handleStateResponse failed";
  }
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle the state response of the master
////////////////////////////////////////////////////////////////////////////////

int Syncer::handleStateResponse(VPackSlice const& slice, std::string& errorMsg) {
  std::string const endpointString =
      " from endpoint '" + _masterInfo._endpoint + "'";

  // process "state" section
  VPackSlice const state = slice.get("state");

  if (!state.isObject()) {
    errorMsg = "state section is missing in response" + endpointString;

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // state."lastLogTick"
  VPackSlice const tick = state.get("lastLogTick");

  if (!tick.isString()) {
    errorMsg = "lastLogTick is missing in response" + endpointString;

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_voc_tick_t const lastLogTick = VelocyPackHelper::stringUInt64(tick);

  if (lastLogTick == 0) {
    errorMsg = "lastLogTick is 0 in response" + endpointString;
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // state."running"
  bool running = VelocyPackHelper::getBooleanValue(state, "running", false);

  // process "server" section
  VPackSlice const server = slice.get("server");

  if (!server.isObject()) {
    errorMsg = "server section is missing in response" + endpointString;

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // server."version"
  VPackSlice const version = server.get("version");

  if (!version.isString()) {
    errorMsg = "server version is missing in response" + endpointString;

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // server."serverId"
  VPackSlice const serverId = server.get("serverId");

  if (!serverId.isString()) {
    errorMsg = "server id is missing in response" + endpointString;

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // validate all values we got
  std::string const masterIdString(serverId.copyString());
  TRI_server_id_t const masterId = StringUtils::uint64(masterIdString);

  if (masterId == 0) {
    // invalid master id
    errorMsg = "invalid server id in response" + endpointString;

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  if (masterIdString == _localServerIdString) {
    // master and replica are the same instance. this is not supported.
    errorMsg = "got same server id (" + _localServerIdString + ")" +
               endpointString + " as the local applier server's id";

    return TRI_ERROR_REPLICATION_LOOP;
  }

  int major = 0;
  int minor = 0;

  std::string const versionString(version.copyString());

  if (sscanf(versionString.c_str(), "%d.%d", &major, &minor) != 2) {
    errorMsg = "invalid master version info" + endpointString + ": '" +
               versionString + "'";

    return TRI_ERROR_REPLICATION_MASTER_INCOMPATIBLE;
  }

  if (major != 3) {
    // we can connect to 3.x only
    errorMsg = "got incompatible master version" + endpointString + ": '" +
               versionString + "'";

    return TRI_ERROR_REPLICATION_MASTER_INCOMPATIBLE;
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

  return TRI_ERROR_NO_ERROR;
}
