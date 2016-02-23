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
#include "Basics/json.h"
#include "Basics/JsonHelper.h"
#include "Rest/HttpRequest.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Utils/CollectionGuard.h"
#include "Utils/DocumentHelper.h"
#include "Utils/transactions.h"
#include "VocBase/collection.h"
#include "VocBase/document-collection.h"
#include "VocBase/edge-collection.h"
#include "VocBase/server.h"
#include "VocBase/transaction.h"
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
      _policy(TRI_DOC_UPDATE_LAST_WRITE, 0, nullptr),
      _endpoint(nullptr),
      _connection(nullptr),
      _client(nullptr),
      _barrierId(0),
      _barrierUpdateTime(0),
      _barrierTtl(600) {
  if (configuration->_database.empty()) {
    // use name of current database
    _databaseName = std::string(vocbase->_name);
  } else {
    // use name from configuration
    _databaseName = configuration->_database;
  }

  // get our own server-id
  _localServerId = TRI_GetIdServer();
  _localServerIdString = StringUtils::itoa(_localServerId);

  _configuration.update(configuration);

  _masterInfo._endpoint = configuration->_endpoint;

  _endpoint = Endpoint::clientFactory(_configuration._endpoint);

  if (_endpoint != nullptr) {
    _connection = GeneralClientConnection::factory(
        _endpoint, _configuration._requestTimeout,
        _configuration._connectTimeout,
        (size_t)_configuration._maxConnectRetries,
        (uint32_t)_configuration._sslProtocol);

    if (_connection != nullptr) {
      _client = new SimpleHttpClient(_connection,
                                     _configuration._requestTimeout, false);

      if (_client != nullptr) {
        std::string username = _configuration._username;
        std::string password = _configuration._password;

        _client->setUserNamePassword("/", username, password);
        _client->setLocationRewriter(this, &rewriteLocation);

        _client->_maxRetries = 2;
        _client->_retryWaitTime = 2 * 1000 * 1000;
        _client->_retryMessage =
            std::string("retrying failed HTTP request for endpoint '") +
            _configuration._endpoint +
            std::string("' for replication applier in database '" +
                        std::string(_vocbase->_name) + "'");
      }
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
      HttpRequest::HTTP_REQUEST_POST, url, body.c_str(), body.size()));

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
    std::unique_ptr<TRI_json_t> json(
        TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, response->getBody().c_str()));

    if (json == nullptr) {
      res = TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    } else {
      std::string const id = JsonHelper::getStringValue(json.get(), "id", "");

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
      HttpRequest::HTTP_REQUEST_PUT, url, body.c_str(), body.size()));

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
        HttpRequest::HTTP_REQUEST_DELETE, url, nullptr, 0));

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
/// @brief extract the collection id from JSON
////////////////////////////////////////////////////////////////////////////////

TRI_voc_cid_t Syncer::getCid(TRI_json_t const* json) const {
  // Only temporary
  std::shared_ptr<VPackBuilder> builder =
      arangodb::basics::JsonHelper::toVelocyPack(json);
  return getCid(builder->slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the collection id from VelocyPack
////////////////////////////////////////////////////////////////////////////////

TRI_voc_cid_t Syncer::getCid(VPackSlice const& slice) const {
  if (!slice.isObject()) {
    return 0;
  }
  VPackSlice id = slice.get("cid");

  if (id.isString()) {
    // string cid, e.g. "9988488"
    return StringUtils::uint64(id.copyString());
  } else if (id.isNumber()) {
    // numeric cid, e.g. 9988488
    return id.getNumericValue<TRI_voc_cid_t>();
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the collection name from JSON
////////////////////////////////////////////////////////////////////////////////

std::string Syncer::getCName(TRI_json_t const* json) const {
  // Only temporary
  std::shared_ptr<VPackBuilder> builder =
      arangodb::basics::JsonHelper::toVelocyPack(json);
  return getCName(builder->slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the collection name from VelocyPack
////////////////////////////////////////////////////////////////////////////////

std::string Syncer::getCName(VPackSlice const& slice) const {
  return arangodb::basics::VelocyPackHelper::getStringValue(slice, "cname", "");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply the data from a collection dump or the continuous log
////////////////////////////////////////////////////////////////////////////////

int Syncer::applyCollectionDumpMarker(
    arangodb::Transaction* trx, TRI_transaction_collection_t* trxCollection,
    TRI_replication_operation_e type, const TRI_voc_key_t key,
    const TRI_voc_rid_t rid, TRI_json_t const* json, std::string& errorMsg) {
  if (type == REPLICATION_MARKER_DOCUMENT || type == REPLICATION_MARKER_EDGE) {
    // {"type":2400,"key":"230274209405676","data":{"_key":"230274209405676","_rev":"230274209405676","foo":"bar"}}

    TRI_ASSERT(json != nullptr);

    TRI_document_collection_t* document =
        trxCollection->_collection->_collection;
    TRI_memory_zone_t* zone =
        document->getShaper()
            ->memoryZone();  // PROTECTED by trx in trxCollection
    TRI_shaped_json_t* shaped =
        TRI_ShapedJsonJson(document->getShaper(), json,
                           true);  // PROTECTED by trx in trxCollection

    if (shaped == nullptr) {
      errorMsg = TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY);

      return TRI_ERROR_OUT_OF_MEMORY;
    }

    try {
      TRI_doc_mptr_copy_t mptr;

      bool const isLocked = TRI_IsLockedCollectionTransaction(trxCollection);
      int res = TRI_ReadShapedJsonDocumentCollection(trx, trxCollection, key,
                                                     &mptr, !isLocked);

      if (res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        // insert

        if (type == REPLICATION_MARKER_EDGE) {
          // edge
          if (document->_info.type() != TRI_COL_TYPE_EDGE) {
            res = TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID;
          } else {
            res = TRI_ERROR_NO_ERROR;
          }

          std::string const from =
              JsonHelper::getStringValue(json, TRI_VOC_ATTRIBUTE_FROM, "");
          std::string const to =
              JsonHelper::getStringValue(json, TRI_VOC_ATTRIBUTE_TO, "");

          CollectionNameResolver resolver(_vocbase);

          // parse _from
          TRI_document_edge_t edge;
          if (!DocumentHelper::parseDocumentId(resolver, from.c_str(),
                                               edge._fromCid, &edge._fromKey)) {
            res = TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
          }

          // parse _to
          if (!DocumentHelper::parseDocumentId(resolver, to.c_str(),
                                               edge._toCid, &edge._toKey)) {
            res = TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
          }

          if (res == TRI_ERROR_NO_ERROR) {
            res = TRI_InsertShapedJsonDocumentCollection(
                trx, trxCollection, key, rid, nullptr, &mptr, shaped, &edge,
                !isLocked, false, true);
          }
        } else {
          // document
          if (document->_info.type() != TRI_COL_TYPE_DOCUMENT) {
            res = TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID;
          } else {
            res = TRI_InsertShapedJsonDocumentCollection(
                trx, trxCollection, key, rid, nullptr, &mptr, shaped, nullptr,
                !isLocked, false, true);
          }
        }
      } else {
        // update
        res = TRI_UpdateShapedJsonDocumentCollection(
            trx, trxCollection, key, rid, nullptr, &mptr, shaped, &_policy,
            !isLocked, false);
      }

      TRI_FreeShapedJson(zone, shaped);

      return res;
    } catch (arangodb::basics::Exception const& ex) {
      TRI_FreeShapedJson(zone, shaped);
      return ex.code();
    } catch (...) {
      TRI_FreeShapedJson(zone, shaped);
      return TRI_ERROR_INTERNAL;
    }
  }

  else if (type == REPLICATION_MARKER_REMOVE) {
    // {"type":2402,"key":"592063"}

    int res = TRI_ERROR_INTERNAL;
    bool const isLocked = TRI_IsLockedCollectionTransaction(trxCollection);

    try {
      res = TRI_RemoveShapedJsonDocumentCollection(
          trx, trxCollection, key, rid, nullptr, &_policy, !isLocked, false);

      if (res != TRI_ERROR_NO_ERROR &&
          res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        // ignore this error
        res = TRI_ERROR_NO_ERROR;
      }
    } catch (arangodb::basics::Exception const& ex) {
      res = ex.code();
    } catch (...) {
      res = TRI_ERROR_INTERNAL;
    }

    if (res != TRI_ERROR_NO_ERROR) {
      errorMsg = "document removal operation failed: " +
                 std::string(TRI_errno_string(res));
    }

    return res;
  }

  errorMsg = "unexpected marker type " + StringUtils::itoa(type);

  return TRI_ERROR_REPLICATION_UNEXPECTED_MARKER;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////

int Syncer::createCollection(TRI_json_t const* json, TRI_vocbase_col_t** dst) {
  if (dst != nullptr) {
    *dst = nullptr;
  }

  if (!JsonHelper::isObject(json)) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  std::string const name = JsonHelper::getStringValue(json, "name", "");

  if (name.empty()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_voc_cid_t const cid = getCid(json);

  if (cid == 0) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_col_type_e const type = (TRI_col_type_e)JsonHelper::getNumericValue<int>(
      json, "type", (int)TRI_COL_TYPE_DOCUMENT);

  TRI_vocbase_col_t* col = TRI_LookupCollectionByIdVocBase(_vocbase, cid);

  if (col == nullptr) {
    // try looking up the collection by name then
    col = TRI_LookupCollectionByNameVocBase(_vocbase, name.c_str());
  }

  if (col != nullptr && (TRI_col_type_t)col->_type == (TRI_col_type_t)type) {
    // collection already exists. TODO: compare attributes
    return TRI_ERROR_NO_ERROR;
  }

  std::shared_ptr<VPackBuilder> builder =
      arangodb::basics::JsonHelper::toVelocyPack(json);

  // merge in "isSystem" attribute
  VPackBuilder s;
  s.openObject();
  s.add("isSystem", VPackValue(true));
  s.close();

  VPackBuilder merged =
      VPackCollection::merge(s.slice(), builder->slice(), true);

  VocbaseCollectionInfo params(_vocbase, name.c_str(), merged.slice());

  col = TRI_CreateCollectionVocBase(_vocbase, params, cid, true);

  if (col == nullptr) {
    return TRI_errno();
  }

  if (dst != nullptr) {
    *dst = col;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a collection, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////

int Syncer::dropCollection(TRI_json_t const* json, bool reportError) {
  TRI_voc_cid_t cid = getCid(json);
  TRI_vocbase_col_t* col = TRI_LookupCollectionByIdVocBase(_vocbase, cid);

  if (col == nullptr) {
    std::string cname = getCName(json);
    if (!cname.empty()) {
      col = TRI_LookupCollectionByNameVocBase(_vocbase, cname.c_str());
    }
  }

  if (col == nullptr) {
    if (reportError) {
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    }

    return TRI_ERROR_NO_ERROR;
  }

  return TRI_DropCollectionVocBase(_vocbase, col, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an index, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////

int Syncer::createIndex(TRI_json_t const* json) {
  // Only temporary
  std::shared_ptr<VPackBuilder> builder =
      arangodb::basics::JsonHelper::toVelocyPack(json);
  return createIndex(builder->slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an index, based on the VelocyPack provided
////////////////////////////////////////////////////////////////////////////////

int Syncer::createIndex(VPackSlice const& slice) {
  VPackSlice const indexSlice = slice.get("index");
  if (!indexSlice.isObject()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_voc_cid_t cid = getCid(slice);
  std::string cnameString = getCName(slice);

  // TODO
  // Backwards compatibiltiy. old check to nullptr, new is empty string
  // Other api does not know yet.
  char const* cname = nullptr;
  if (!cnameString.empty()) {
    cname = cnameString.c_str();
  }

  try {
    CollectionGuard guard(_vocbase, cid, cname);

    if (guard.collection() == nullptr) {
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    }

    TRI_document_collection_t* document = guard.collection()->_collection;

    SingleCollectionTransaction trx(StandaloneTransactionContext::Create(_vocbase), _vocbase, guard.collection()->_cid, TRI_TRANSACTION_WRITE);

    int res = trx.begin();

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    arangodb::Index* idx = nullptr;
    res = TRI_FromVelocyPackIndexDocumentCollection(&trx, document, indexSlice,
                                                    &idx);

    if (res == TRI_ERROR_NO_ERROR) {
      res = TRI_SaveIndex(document, idx, true);
    }

    res = trx.finish(res);

    return res;
  } catch (arangodb::basics::Exception const& ex) {
    return ex.code();
  } catch (...) {
    return TRI_ERROR_INTERNAL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an index, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////

int Syncer::dropIndex(TRI_json_t const* json) {
  std::string const id = JsonHelper::getStringValue(json, "id", "");

  if (id.empty()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_idx_iid_t const iid = StringUtils::uint64(id);

  TRI_voc_cid_t cid = getCid(json);
  std::string cnameString = getCName(json);

  // TODO
  // Backwards compatibiltiy. old check to nullptr, new is empty string
  // Other api does not know yet.
  char const* cname = nullptr;
  if (!cnameString.empty()) {
    cname = cnameString.c_str();
  }

  try {
    CollectionGuard guard(_vocbase, cid, cname);

    if (guard.collection() == nullptr) {
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    }

    TRI_document_collection_t* document = guard.collection()->_collection;

    bool result = TRI_DropIndexDocumentCollection(document, iid, true);

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
  uint64_t maxRetries = _client->_maxRetries;
  uint64_t retryWaitTime = _client->_retryWaitTime;

  // apply settings that prevent endless waiting here
  _client->_maxRetries = 1;
  _client->_retryWaitTime = 500 * 1000;

  std::unique_ptr<SimpleHttpResult> response(
      _client->retryRequest(HttpRequest::HTTP_REQUEST_GET, url, nullptr, 0));

  // restore old settings
  _client->_maxRetries = maxRetries;
  _client->_retryWaitTime = retryWaitTime;

  if (response == nullptr || !response->isComplete()) {
    errorMsg = "could not connect to master at " + _masterInfo._endpoint +
               ": " + _client->getErrorMessage();

    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }

  int res = TRI_ERROR_NO_ERROR;

  if (response->wasHttpError()) {
    res = TRI_ERROR_REPLICATION_MASTER_ERROR;

    errorMsg = "got invalid response from master at " + _masterInfo._endpoint +
               ": HTTP " + StringUtils::itoa(response->getHttpReturnCode()) +
               ": " + response->getHttpReturnMessage();
  } else {
    std::unique_ptr<TRI_json_t> json(
        TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, response->getBody().c_str()));

    if (JsonHelper::isObject(json.get())) {
      res = handleStateResponse(json.get(), errorMsg);
    } else {
      res = TRI_ERROR_REPLICATION_INVALID_RESPONSE;

      errorMsg = "got invalid response from master at " +
                 _masterInfo._endpoint + ": invalid JSON";
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle the state response of the master
////////////////////////////////////////////////////////////////////////////////

int Syncer::handleStateResponse(TRI_json_t const* json, std::string& errorMsg) {
  std::string const endpointString =
      " from endpoint '" + _masterInfo._endpoint + "'";

  // process "state" section
  TRI_json_t const* state = JsonHelper::getObjectElement(json, "state");

  if (!JsonHelper::isObject(state)) {
    errorMsg = "state section is missing in response" + endpointString;

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // state."lastLogTick"
  TRI_json_t const* tick = JsonHelper::getObjectElement(state, "lastLogTick");

  if (!JsonHelper::isString(tick)) {
    errorMsg = "lastLogTick is missing in response" + endpointString;

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_voc_tick_t const lastLogTick = StringUtils::uint64(
      tick->_value._string.data, tick->_value._string.length - 1);

  // state."running"
  bool running = JsonHelper::getBooleanValue(state, "running", false);

  // process "server" section
  TRI_json_t const* server = JsonHelper::getObjectElement(json, "server");

  if (!JsonHelper::isObject(server)) {
    errorMsg = "server section is missing in response" + endpointString;

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // server."version"
  TRI_json_t const* version = JsonHelper::getObjectElement(server, "version");

  if (!JsonHelper::isString(version)) {
    errorMsg = "server version is missing in response" + endpointString;

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // server."serverId"
  TRI_json_t const* serverId = JsonHelper::getObjectElement(server, "serverId");

  if (!JsonHelper::isString(serverId)) {
    errorMsg = "server id is missing in response" + endpointString;

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // validate all values we got
  std::string const masterIdString = std::string(
      serverId->_value._string.data, serverId->_value._string.length - 1);
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

  std::string const versionString(version->_value._string.data,
                                  version->_value._string.length - 1);

  if (sscanf(versionString.c_str(), "%d.%d", &major, &minor) != 2) {
    errorMsg = "invalid master version info" + endpointString + ": '" +
               versionString + "'";

    return TRI_ERROR_REPLICATION_MASTER_INCOMPATIBLE;
  }

  if (major != 3) {
    // we can connect to 3.x onyl
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
