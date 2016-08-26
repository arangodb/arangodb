////////////////////////////////////////////////////////////////////////////////
/// @brief replication syncer base class
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
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Syncer.h"

#include "Basics/files.h"
#include "Basics/json.h"
#include "Basics/tri-strings.h"
#include "Basics/JsonHelper.h"
#include "Basics/Exceptions.h"
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

using namespace std;
using namespace triagens::arango;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::httpclient;

// -----------------------------------------------------------------------------
// --SECTION--                                                  static variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief base url of the replication API
////////////////////////////////////////////////////////////////////////////////

const std::string Syncer::BaseUrl = "/_api/replication";

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

Syncer::Syncer (TRI_vocbase_t* vocbase,
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

  if (configuration->_database != nullptr) {
    // use name from configuration
    _databaseName = std::string(configuration->_database);
  }
  else {
    // use name of current database
    _databaseName = std::string(vocbase->_name);
  }

  // get our own server-id
  _localServerId       = TRI_GetIdServer();
  _localServerIdString = StringUtils::itoa(_localServerId);

  TRI_InitConfigurationReplicationApplier(&_configuration);
  TRI_CopyConfigurationReplicationApplier(configuration, &_configuration);

  _masterInfo._endpoint = configuration->_endpoint;

  _endpoint = Endpoint::clientFactory(_configuration._endpoint);

  if (_endpoint != nullptr) {
    _connection = GeneralClientConnection::factory(_endpoint,
                                                   _configuration._requestTimeout,
                                                   _configuration._connectTimeout,
                                                   (size_t) _configuration._maxConnectRetries,
                                                   (uint32_t) _configuration._sslProtocol);

    if (_connection != nullptr) {
      _client = new SimpleHttpClient(_connection, _configuration._requestTimeout, false);

      if (_client != nullptr) {
        string username;
        string password;

        if (_configuration._username != nullptr) {
          username = std::string(_configuration._username);
        }

        if (_configuration._password != nullptr) {
          password = std::string(_configuration._password);
        }

        _client->setUserNamePassword("/", username, password);
        _client->setLocationRewriter(this, &rewriteLocation);

        _client->_maxRetries = 2;
        _client->_retryWaitTime = 2 * 1000 * 1000;
        _client->_retryMessage = std::string("retrying failed HTTP request for endpoint '") + _configuration._endpoint + std::string("' for replication applier in database '" + std::string(_vocbase->_name) + "'");
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

Syncer::~Syncer () {
  try {
    sendRemoveBarrier();
  }
  catch (...) {
  }

  // shutdown everything properly
  delete _client;
  delete _connection;
  delete _endpoint;

  TRI_DestroyConfigurationReplicationApplier(&_configuration);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief request location rewriter (injects database name)
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

string Syncer::rewriteLocation (void* data, const string& location) {
  Syncer* s = static_cast<Syncer*>(data);

  TRI_ASSERT(s != nullptr);

  if (location.substr(0, 5) == "/_db/") {
    // location already contains /_db/
    return location;
  }

  if (location[0] == '/') {
    return "/_db/" + s->_databaseName + location;
  }
  else {
    return "/_db/" + s->_databaseName + "/" + location;
  }
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
  std::string const body = "{\"ttl\":" + StringUtils::itoa(_barrierTtl) + ",\"tick\":\"" + StringUtils::itoa(minTick) + "\"}";

  // send request
  std::unique_ptr<SimpleHttpResult> response(_client->retryRequest(
      HttpRequest::HTTP_REQUEST_POST, url, body.c_str(), body.size()));

  if (response == nullptr || !response->isComplete()) {
    errorMsg = "could not connect to master at " +
               _masterInfo._endpoint + ": " +
               _client->getErrorMessage();

    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }

  TRI_ASSERT(response != nullptr);

  int res = TRI_ERROR_NO_ERROR;

  if (response->wasHttpError()) {
    res = TRI_ERROR_REPLICATION_MASTER_ERROR;

    errorMsg = "got invalid response from master at " +
               _masterInfo._endpoint + ": HTTP " +
               StringUtils::itoa(response->getHttpReturnCode()) + ": " +
               response->getHttpReturnMessage();
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
        LOG_DEBUG("created WAL logfile barrier %llu", (unsigned long long) _barrierId);
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
  std::string const body = "{\"ttl\":" + StringUtils::itoa(_barrierTtl) + ",\"tick\"" + StringUtils::itoa(tick) + "\"}";

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
    std::string const url = BaseUrl + "/barrier/" + StringUtils::itoa(_barrierId);

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

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the collection id from JSON
////////////////////////////////////////////////////////////////////////////////

TRI_voc_cid_t Syncer::getCid (TRI_json_t const* json) const {
  if (! JsonHelper::isObject(json)) {
    return 0;
  }

  TRI_json_t const* id = JsonHelper::getObjectElement(json, "cid");

  if (JsonHelper::isString(id)) {
    // string cid, e.g. "9988488"
    return StringUtils::uint64(id->_value._string.data, id->_value._string.length - 1);
  }
  else if (JsonHelper::isNumber(id)) {
    // numeric cid, e.g. 9988488
    return (TRI_voc_cid_t) id->_value._number;
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the collection name from JSON
////////////////////////////////////////////////////////////////////////////////

char const* Syncer::getCName (TRI_json_t const* json) const {
  if (! JsonHelper::isObject(json)) {
    return nullptr;
  }

  TRI_json_t const* cname = JsonHelper::getObjectElement(json, "cname");

  if (JsonHelper::isString(cname)) {
    return cname->_value._string.data;
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply the data from a collection dump or the continuous log
////////////////////////////////////////////////////////////////////////////////

int Syncer::applyCollectionDumpMarker (TRI_transaction_collection_t* trxCollection,
                                       TRI_replication_operation_e type,
                                       const TRI_voc_key_t key,
                                       const TRI_voc_rid_t rid,
                                       TRI_json_t const* json,
                                       string& errorMsg) {

  if (type == REPLICATION_MARKER_DOCUMENT || 
      type == REPLICATION_MARKER_EDGE) {
    // {"type":2400,"key":"230274209405676","data":{"_key":"230274209405676","_rev":"230274209405676","foo":"bar"}}

    TRI_ASSERT(json != nullptr);

    TRI_document_collection_t* document = trxCollection->_collection->_collection;
    TRI_memory_zone_t* zone = document->getShaper()->memoryZone();  // PROTECTED by trx in trxCollection
    TRI_shaped_json_t* shaped = TRI_ShapedJsonJson(document->getShaper(), json, true);  // PROTECTED by trx in trxCollection
    
    if (shaped == nullptr) {
      errorMsg = TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY);

      return TRI_ERROR_OUT_OF_MEMORY;
    }

    try {
      TRI_doc_mptr_copy_t mptr;

      bool const isLocked = TRI_IsLockedCollectionTransaction(trxCollection);
      int res = TRI_ReadShapedJsonDocumentCollection(trxCollection, key, &mptr, ! isLocked);

      if (res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        // insert

        if (type == REPLICATION_MARKER_EDGE) {
          // edge
          if (document->_info._type != TRI_COL_TYPE_EDGE) {
            res = TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID;
          }
          else {
            res = TRI_ERROR_NO_ERROR;
          }

          string const from = JsonHelper::getStringValue(json, TRI_VOC_ATTRIBUTE_FROM, "");
          string const to   = JsonHelper::getStringValue(json, TRI_VOC_ATTRIBUTE_TO, "");

          CollectionNameResolver resolver(_vocbase);

          // parse _from
          TRI_document_edge_t edge;
          if (! DocumentHelper::parseDocumentId(resolver, from.c_str(), edge._fromCid, &edge._fromKey)) {
            res = TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
          }

          // parse _to
          if (! DocumentHelper::parseDocumentId(resolver, to.c_str(), edge._toCid, &edge._toKey)) {
            res = TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
          }

          if (res == TRI_ERROR_NO_ERROR) {
            res = TRI_InsertShapedJsonDocumentCollection(trxCollection, key, rid, nullptr, &mptr, shaped, &edge, ! isLocked, false, true);
          }
        }
        else {
          // document
          if (document->_info._type != TRI_COL_TYPE_DOCUMENT) {
            res = TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID;
          }
          else {
            res = TRI_InsertShapedJsonDocumentCollection(trxCollection, key, rid, nullptr, &mptr, shaped, nullptr, ! isLocked, false, true);
          }
        }
      }
      else {
        // update
        res = TRI_UpdateShapedJsonDocumentCollection(trxCollection, key, rid, nullptr, &mptr, shaped, &_policy, ! isLocked, false);
      }
    
      if (res != TRI_ERROR_NO_ERROR) {
        errorMsg = "document insert/update operation failed: " + string(TRI_errno_string(res));
      }

      TRI_FreeShapedJson(zone, shaped);

      return res;
    }
    catch (triagens::basics::Exception const& ex) {
      TRI_FreeShapedJson(zone, shaped);
      errorMsg = "document insert/update operation failed: ";
      errorMsg.append(ex.what());
      return ex.code();
    }
    catch (std::exception const& ex) {
      TRI_FreeShapedJson(zone, shaped);
      errorMsg = "document insert/update operation failed: ";
      errorMsg.append(ex.what());
      return TRI_ERROR_INTERNAL;
    }
    catch (...) {
      TRI_FreeShapedJson(zone, shaped);
      errorMsg = "document insert/update operation failed: unknown exception";
      return TRI_ERROR_INTERNAL;
    }
  }

  else if (type == REPLICATION_MARKER_REMOVE) {
    // {"type":2402,"key":"592063"}

    bool const isLocked = TRI_IsLockedCollectionTransaction(trxCollection);

    try {
      int res = TRI_RemoveShapedJsonDocumentCollection(trxCollection, key, rid, nullptr, &_policy, ! isLocked, false);

      if (res != TRI_ERROR_NO_ERROR && res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        // ignore this error
        res = TRI_ERROR_NO_ERROR;
      }
        
      if (res != TRI_ERROR_NO_ERROR) {
        errorMsg = "document removal operation failed: " + string(TRI_errno_string(res));
      }

      return res;
    }
    catch (triagens::basics::Exception const& ex) {
      errorMsg = "document removal operation failed: ";
      errorMsg.append(ex.what());
      return ex.code();
    }
    catch (std::exception const& ex) {
      errorMsg = "document removal operation failed: ";
      errorMsg.append(ex.what());
      return TRI_ERROR_INTERNAL;
    }
    catch (...) {
      errorMsg = "document removal operation failed: unknown exception";
      return TRI_ERROR_INTERNAL;
    }
  }

  else {
    errorMsg = "unexpected marker type " + StringUtils::itoa(type);

    return TRI_ERROR_REPLICATION_UNEXPECTED_MARKER;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////

int Syncer::createCollection (TRI_json_t const* json,
                              TRI_vocbase_col_t** dst) {
  if (dst != nullptr) {
    *dst = nullptr;
  }

  if (! JsonHelper::isObject(json)) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  string const name = JsonHelper::getStringValue(json, "name", "");

  if (name.empty()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_voc_cid_t const cid = getCid(json);

  if (cid == 0) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_col_type_e const type = (TRI_col_type_e) JsonHelper::getNumericValue<int>(json, "type", (int) TRI_COL_TYPE_DOCUMENT);

  TRI_vocbase_col_t* col = TRI_LookupCollectionByIdVocBase(_vocbase, cid);

  if (col == nullptr) {
    // try looking up the collection by name then
    col = TRI_LookupCollectionByNameVocBase(_vocbase, name.c_str());
  }

  if (col != nullptr &&
      (TRI_col_type_t) col->_type == (TRI_col_type_t) type) {
    // collection already exists. TODO: compare attributes
    return TRI_ERROR_NO_ERROR;
  }

  TRI_json_t* keyOptions = nullptr;

  if (JsonHelper::isObject(JsonHelper::getObjectElement(json, "keyOptions"))) {
    keyOptions = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, JsonHelper::getObjectElement(json, "keyOptions"));
  }

  TRI_col_info_t params;
  TRI_InitCollectionInfo(_vocbase,
                         &params,
                         name.c_str(),
                         type,
                         (TRI_voc_size_t) JsonHelper::getNumericValue<int64_t>(json, "maximalSize", (int64_t) TRI_JOURNAL_DEFAULT_MAXIMAL_SIZE),
                         keyOptions);

  if (keyOptions != nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, keyOptions);
  }

  params._doCompact    = JsonHelper::getBooleanValue(json, "doCompact", true);
  params._waitForSync  = JsonHelper::getBooleanValue(json, "waitForSync", _vocbase->_settings.defaultWaitForSync);
  params._isVolatile   = JsonHelper::getBooleanValue(json, "isVolatile", false);
  params._isSystem     = (name[0] == '_');
  params._planId       = 0;
  params._indexBuckets = JsonHelper::getNumericValue<uint32_t>(json, "indexBuckets", (uint32_t) TRI_DEFAULT_INDEX_BUCKETS);

  TRI_voc_cid_t planId = JsonHelper::stringUInt64(json, "planId");
  if (planId > 0) {
    params._planId = planId;
  }

  // wait for "old" collection to be dropped
  char* dirName = TRI_GetDirectoryCollection(_vocbase->_path,
                                             name.c_str(),
                                             type,
                                             cid);

  if (dirName != nullptr) {
    char* parameterName = TRI_Concatenate2File(dirName, TRI_VOC_PARAMETER_FILE);

    if (parameterName != nullptr) {
      int iterations = 0;

      while (TRI_IsDirectory(dirName) && TRI_ExistsFile(parameterName) && iterations++ < 1200) {
        usleep(1000 * 100);
      }

      TRI_FreeString(TRI_CORE_MEM_ZONE, parameterName);
    }

    TRI_FreeString(TRI_CORE_MEM_ZONE, dirName);
  }

  col = TRI_CreateCollectionVocBase(_vocbase, &params, cid, true);
  TRI_FreeCollectionInfoOptions(&params);

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

int Syncer::dropCollection (TRI_json_t const* json,
                            bool reportError) {
  TRI_voc_cid_t cid = getCid(json);
  TRI_vocbase_col_t* col = TRI_LookupCollectionByIdVocBase(_vocbase, cid);

  if (col == nullptr) {
    char const* cname = getCName(json);
    if (cname != nullptr) {
      col = TRI_LookupCollectionByNameVocBase(_vocbase, cname);
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

int Syncer::createIndex (TRI_json_t const* json) {
  TRI_json_t const* indexJson = JsonHelper::getObjectElement(json, "index");

  if (! JsonHelper::isObject(indexJson)) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_voc_cid_t cid = getCid(json);
  char const* cname = getCName(json);

  try {
    CollectionGuard guard(_vocbase, cid, cname);

    if (guard.collection() == nullptr) {
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    }

    TRI_document_collection_t* document = guard.collection()->_collection;
  
    SingleCollectionWriteTransaction<UINT64_MAX> trx(new StandaloneTransactionContext(), _vocbase, guard.collection()->_cid);

    int res = trx.begin();

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    triagens::arango::Index* idx = nullptr;
    res = TRI_FromJsonIndexDocumentCollection(document, indexJson, &idx);

    if (res == TRI_ERROR_NO_ERROR) {
      res = TRI_SaveIndex(document, idx, true);
    }

    res = trx.finish(res);

    return res;
  }
  catch (triagens::basics::Exception const& ex) {
    return ex.code();
  }
  catch (...) {
    return TRI_ERROR_INTERNAL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an index, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////

int Syncer::dropIndex (TRI_json_t const* json) {
  string const id = JsonHelper::getStringValue(json, "id", "");

  if (id.empty()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_idx_iid_t const iid = StringUtils::uint64(id);

  TRI_voc_cid_t cid = getCid(json);
  char const* cname = getCName(json);

  try {
    CollectionGuard guard(_vocbase, cid, cname);
    
    if (guard.collection() == nullptr) {
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    }

    TRI_document_collection_t* document = guard.collection()->_collection;

    bool result = TRI_DropIndexDocumentCollection(document, iid, true);

    if (! result) {
      // TODO: index not found, should we care??
      return TRI_ERROR_NO_ERROR;
    }

    return TRI_ERROR_NO_ERROR;
  }
  catch (triagens::basics::Exception const& ex) {
    return ex.code();
  }
  catch (...) {
    return TRI_ERROR_INTERNAL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get master state
////////////////////////////////////////////////////////////////////////////////

int Syncer::getMasterState (string& errorMsg) {
  string const url = BaseUrl + "/logger-state?serverId=" + _localServerIdString;

  // store old settings
  uint64_t maxRetries = _client->_maxRetries; 
  uint64_t retryWaitTime = _client->_retryWaitTime;

  // apply settings that prevent endless waiting here
  _client->_maxRetries    = 1;
  _client->_retryWaitTime = 500 * 1000;

  std::unique_ptr<SimpleHttpResult> response(_client->retryRequest(HttpRequest::HTTP_REQUEST_GET,
                                             url,
                                             nullptr,
                                             0));

  // restore old settings
  _client->_maxRetries = maxRetries;
  _client->_retryWaitTime = retryWaitTime;

  if (response == nullptr || ! response->isComplete()) {
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
  }
  else {
    std::unique_ptr<TRI_json_t> json(TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, response->getBody().c_str()));

    if (JsonHelper::isObject(json.get())) {
      res = handleStateResponse(json.get(), errorMsg);
    }
    else {
      res = TRI_ERROR_REPLICATION_INVALID_RESPONSE;

      errorMsg = "got invalid response from master at " + _masterInfo._endpoint +
        ": invalid JSON";
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle the state response of the master
////////////////////////////////////////////////////////////////////////////////

int Syncer::handleStateResponse (TRI_json_t const* json,
                                 string& errorMsg) {
  string const endpointString = " from endpoint '" + _masterInfo._endpoint + "'";

  // process "state" section
  TRI_json_t const* state = JsonHelper::getObjectElement(json, "state");

  if (! JsonHelper::isObject(state)) {
    errorMsg = "state section is missing in response" + endpointString;

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // state."lastLogTick"
  TRI_json_t const* tick = JsonHelper::getObjectElement(state, "lastLogTick");

  if (! JsonHelper::isString(tick)) {
    errorMsg = "lastLogTick is missing in response" + endpointString;

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_voc_tick_t const lastLogTick = StringUtils::uint64(tick->_value._string.data, tick->_value._string.length - 1);

  // state."running"
  bool running = JsonHelper::getBooleanValue(state, "running", false);

  // process "server" section
  TRI_json_t const* server = JsonHelper::getObjectElement(json, "server");

  if (! JsonHelper::isObject(server)) {
    errorMsg = "server section is missing in response" + endpointString;

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // server."version"
  TRI_json_t const* version = JsonHelper::getObjectElement(server, "version");

  if (! JsonHelper::isString(version)) {
    errorMsg = "server version is missing in response" + endpointString;

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // server."serverId"
  TRI_json_t const* serverId = JsonHelper::getObjectElement(server, "serverId");

  if (! JsonHelper::isString(serverId)) {
    errorMsg = "server id is missing in response" + endpointString;

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // validate all values we got
  string const masterIdString = string(serverId->_value._string.data, serverId->_value._string.length - 1);
  TRI_server_id_t const masterId = StringUtils::uint64(masterIdString);

  if (masterId == 0) {
    // invalid master id
    errorMsg = "invalid server id in response" + endpointString;

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  if (masterIdString == _localServerIdString) {
    // master and replica are the same instance. this is not supported.
    errorMsg = "got same server id (" + _localServerIdString + ")" + endpointString +
               " as the local applier server's id";

    return TRI_ERROR_REPLICATION_LOOP;
  }

  int major = 0;
  int minor = 0;

  string const versionString(version->_value._string.data, version->_value._string.length - 1);

  if (sscanf(versionString.c_str(), "%d.%d", &major, &minor) != 2) {
    errorMsg = "invalid master version info" + endpointString + ": '" + versionString + "'";

    return TRI_ERROR_REPLICATION_MASTER_INCOMPATIBLE;
  }

  if (major < 2 ||
      major > 2 ||
      (major == 2 && minor < 2)) {
    // we can connect to 1.4, 2.0 and higher only
    errorMsg = "got incompatible master version" + endpointString + ": '" + versionString + "'";

    return TRI_ERROR_REPLICATION_MASTER_INCOMPATIBLE;
  }

  _masterInfo._majorVersion        = major;
  _masterInfo._minorVersion        = minor;
  _masterInfo._serverId            = masterId;
  _masterInfo._lastLogTick         = lastLogTick;
  _masterInfo._active              = running;

  LOG_INFO("connected to master at %s, id %llu, %d.%d, last log tick %llu", _masterInfo._endpoint.c_str(), (unsigned long long) _masterInfo._serverId, (int) _masterInfo._majorVersion, (int) _masterInfo._minorVersion, (unsigned long long) _masterInfo._lastLogTick);

  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
