////////////////////////////////////////////////////////////////////////////////
/// @brief replication syncer base class
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
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Syncer.h"

#include "BasicsC/files.h"
#include "BasicsC/json.h"
#include "BasicsC/tri-strings.h"
#include "Basics/JsonHelper.h"
#include "Rest/HttpRequest.h"
#include "Rest/SslInterface.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Utils/DocumentHelper.h"
#include "VocBase/collection.h"
#include "VocBase/edge-collection.h"
#include "VocBase/primary-collection.h"
#include "VocBase/server-id.h"
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
/// @addtogroup Replication
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief base url of the replication API
////////////////////////////////////////////////////////////////////////////////

const string Syncer::BaseUrl = "/_api/replication";

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////
  
// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Replication
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

Syncer::Syncer (TRI_vocbase_t* vocbase,
                TRI_replication_applier_configuration_t const* configuration) : 
  _vocbase(vocbase),
  _configuration(),
  _masterInfo(),
  _policy(),
  _endpoint(0),
  _connection(0),
  _client(0) {
    
  // get our own server-id 
  _localServerId       = TRI_GetServerId();
  _localServerIdString = StringUtils::itoa(_localServerId);
 
  // init the update policy
  TRI_InitUpdatePolicy(&_policy, TRI_DOC_UPDATE_LAST_WRITE, 0, 0); 

  TRI_InitConfigurationReplicationApplier(&_configuration);
  TRI_CopyConfigurationReplicationApplier(configuration, &_configuration);
 
  TRI_InitMasterInfoReplication(&_masterInfo, configuration->_endpoint);
    
  _endpoint = Endpoint::clientFactory(_configuration._endpoint);

  if (_endpoint != 0) { 
    _connection = GeneralClientConnection::factory(_endpoint,
                                                   _configuration._requestTimeout,
                                                   _configuration._connectTimeout,
                                                   (size_t) _configuration._maxConnectRetries);

    if (_connection != 0) {
      _client = new SimpleHttpClient(_connection, _configuration._requestTimeout, false);

      if (_client != 0) {
        string username;
        string password;

        if (_configuration._username != 0) {
          username = string(_configuration._username);
        }
        
        if (_configuration._password != 0) {
          password = string(_configuration._password);
        }

        _client->setUserNamePassword("/", username, password);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

Syncer::~Syncer () {
  // shutdown everything properly
  if (_client != 0) {
    delete _client;
  }

  if (_connection != 0) {
    delete _connection;
  }

  if (_endpoint != 0) {
    delete _endpoint;
  }

  TRI_DestroyMasterInfoReplication(&_masterInfo);
  TRI_DestroyConfigurationReplicationApplier(&_configuration);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Replication
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the collection id from JSON
////////////////////////////////////////////////////////////////////////////////

TRI_voc_cid_t Syncer::getCid (TRI_json_t const* json) const {
  if (! JsonHelper::isArray(json)) {
    return 0;
  }

  TRI_json_t const* id = JsonHelper::getArrayElement(json, "cid");

  if (JsonHelper::isString(id)) {
    return StringUtils::uint64(id->_value._string.data, id->_value._string.length - 1);
  }
  else if (JsonHelper::isNumber(id)) {
    return (TRI_voc_cid_t) id->_value._number;
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a transaction hint
////////////////////////////////////////////////////////////////////////////////

TRI_transaction_hint_t Syncer::getHint (const size_t numOperations) const {
  if (numOperations <= 1) {
    return (TRI_transaction_hint_t) TRI_TRANSACTION_HINT_SINGLE_OPERATION;
  }

  return (TRI_transaction_hint_t) 0;
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

  if (type == MARKER_DOCUMENT || type == MARKER_EDGE) {
    // {"type":2400,"key":"230274209405676","data":{"_key":"230274209405676","_rev":"230274209405676","foo":"bar"}}

    assert(json != 0);

    TRI_primary_collection_t* primary = trxCollection->_collection->_collection;
    TRI_shaped_json_t* shaped = TRI_ShapedJsonJson(primary->_shaper, json);

    if (shaped != 0) {
      TRI_doc_mptr_t mptr;

      int res = primary->read(trxCollection, key, &mptr, false);

      if (res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        // insert

        if (type == MARKER_EDGE) {
          // edge
          if (primary->base._info._type != TRI_COL_TYPE_EDGE) {
            res = TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID;
          }
          else {
            res = TRI_ERROR_NO_ERROR;
          }

          const string from = JsonHelper::getStringValue(json, TRI_VOC_ATTRIBUTE_FROM, "");
          const string to   = JsonHelper::getStringValue(json, TRI_VOC_ATTRIBUTE_TO, "");
          

          // parse _from
          TRI_document_edge_t edge;
          if (! DocumentHelper::parseDocumentId(from.c_str(), edge._fromCid, &edge._fromKey)) {
            res = TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
          }
          
          // parse _to
          if (! DocumentHelper::parseDocumentId(to.c_str(), edge._toCid, &edge._toKey)) {
            res = TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
          }

          if (res == TRI_ERROR_NO_ERROR) {
            res = primary->insert(trxCollection, key, rid, &mptr, TRI_DOC_MARKER_KEY_EDGE, shaped, &edge, false, false);
          }
        }
        else {
          // document
          if (primary->base._info._type != TRI_COL_TYPE_DOCUMENT) {
            res = TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID;
          }
          else {
            res = primary->insert(trxCollection, key, rid, &mptr, TRI_DOC_MARKER_KEY_DOCUMENT, shaped, 0, false, false);
          }
        }
      }
      else {
        // update
        res = primary->update(trxCollection, key, rid, &mptr, shaped, &_policy, false, false);
      }
      
      TRI_FreeShapedJson(primary->_shaper, shaped);

      return res;
    }
    else {
      errorMsg = TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY);
      
      return TRI_ERROR_OUT_OF_MEMORY;
    }
  }

  else if (type == MARKER_REMOVE) {
    // {"type":2402,"key":"592063"}

    TRI_primary_collection_t* primary = trxCollection->_collection->_collection;
    int res = primary->remove(trxCollection, key, rid, &_policy, false, false);

    if (res != TRI_ERROR_NO_ERROR) {
      if (res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        // ignore this error
        res = TRI_ERROR_NO_ERROR;
      }
      else {
        errorMsg = "document removal operation failed: " + string(TRI_errno_string(res));
      }
    }

    return res;
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
  if (dst != 0) {
    *dst = 0;
  }

  if (! JsonHelper::isArray(json)) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
  
  const string name = JsonHelper::getStringValue(json, "name", "");

  if (name.empty()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  const TRI_voc_cid_t cid = getCid(json);

  if (cid == 0) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
  
  const TRI_col_type_e type = (TRI_col_type_e) JsonHelper::getNumericValue<int>(json, "type", (int) TRI_COL_TYPE_DOCUMENT);

  TRI_vocbase_col_t* col = TRI_LookupCollectionByIdVocBase(_vocbase, cid);

  if (col != 0 && 
      (TRI_col_type_t) col->_type == (TRI_col_type_t) type) {
    // collection already exists. TODO: compare attributes
    return TRI_ERROR_NO_ERROR;
  }


  TRI_json_t* keyOptions = 0;

  if (JsonHelper::isArray(JsonHelper::getArrayElement(json, "keyOptions"))) {
    keyOptions = TRI_CopyJson(TRI_CORE_MEM_ZONE, JsonHelper::getArrayElement(json, "keyOptions"));
  }
  
  TRI_col_info_t params;
  TRI_InitCollectionInfo(_vocbase, 
                         &params, 
                         name.c_str(),
                         type,
                         (TRI_voc_size_t) JsonHelper::getNumericValue<int64_t>(json, "maximalSize", (int64_t) TRI_JOURNAL_DEFAULT_MAXIMAL_SIZE),
                         keyOptions);

  params._doCompact =   JsonHelper::getBooleanValue(json, "doCompact", true); 
  params._waitForSync = JsonHelper::getBooleanValue(json, "waitForSync", _vocbase->_defaultWaitForSync);
  params._isVolatile =  JsonHelper::getBooleanValue(json, "isVolatile", false); 
  
  // wait for "old" collection to be dropped
  char* dirName = TRI_GetDirectoryCollection(_vocbase->_path,
                                             name.c_str(),
                                             type, 
                                             cid);

  if (dirName != 0) {
    char* parameterName = TRI_Concatenate2File(dirName, TRI_COL_PARAMETER_FILE);

    if (parameterName != 0) {
      int iterations = 0;

      while (TRI_IsDirectory(dirName) && TRI_ExistsFile(parameterName) && iterations++ < 120) {
        sleep(1);
      }

      TRI_FreeString(TRI_CORE_MEM_ZONE, parameterName);
    }

    TRI_FreeString(TRI_CORE_MEM_ZONE, dirName);
  }

  col = TRI_CreateCollectionVocBase(_vocbase, &params, cid, _masterInfo._serverId);
  TRI_FreeCollectionInfoOptions(&params);

  if (col == NULL) {
    return TRI_errno();
  }

  if (dst != 0) {
    *dst = col;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a collection, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////
    
int Syncer::dropCollection (TRI_json_t const* json,
                            bool reportError) {
  const TRI_voc_cid_t cid = getCid(json);

  if (cid == 0) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_vocbase_col_t* col = TRI_LookupCollectionByIdVocBase(_vocbase, cid);

  if (col == 0) {
    if (reportError) {
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    }

    return TRI_ERROR_NO_ERROR;
  }

  return TRI_DropCollectionVocBase(_vocbase, col, _masterInfo._serverId);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an index, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////
    
int Syncer::createIndex (TRI_json_t const* json) {
  const TRI_voc_cid_t cid = getCid(json);

  if (cid == 0) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_json_t const* indexJson = JsonHelper::getArrayElement(json, "index");

  if (! JsonHelper::isArray(indexJson)) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_vocbase_col_t* col = TRI_UseCollectionByIdVocBase(_vocbase, cid);

  if (col == 0 || col->_collection == 0) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  TRI_index_t* idx;
  TRI_primary_collection_t* primary = col->_collection;

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  int res = TRI_FromJsonIndexDocumentCollection((TRI_document_collection_t*) primary, indexJson, &idx);

  if (res == TRI_ERROR_NO_ERROR) {
    res = TRI_SaveIndex(primary, idx, _masterInfo._serverId);
  }

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

  TRI_ReleaseCollectionVocBase(_vocbase, col);
  
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an index, based on the JSON provided
////////////////////////////////////////////////////////////////////////////////
    
int Syncer::dropIndex (TRI_json_t const* json) {
  const TRI_voc_cid_t cid = getCid(json);

  if (cid == 0) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  const string id = JsonHelper::getStringValue(json, "id", "");

  if (id.empty()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
  
  const TRI_idx_iid_t iid = StringUtils::uint64(id);

  TRI_vocbase_col_t* col = TRI_UseCollectionByIdVocBase(_vocbase, cid);

  if (col == 0 || col->_collection == 0) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }
  
  TRI_document_collection_t* document = (TRI_document_collection_t*) col->_collection;

  bool result = TRI_DropIndexDocumentCollection(document, iid, _masterInfo._serverId);

  TRI_ReleaseCollectionVocBase(_vocbase, col);

  if (! result) {
    // TODO: index not found, should we care??
    return TRI_ERROR_NO_ERROR;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get master state
////////////////////////////////////////////////////////////////////////////////

int Syncer::getMasterState (string& errorMsg) {
  map<string, string> headers;
  static const string url = BaseUrl + 
                            "/logger-state" + 
                            "?serverId=" + _localServerIdString;

  SimpleHttpResult* response = _client->request(HttpRequest::HTTP_REQUEST_GET, 
                                                url,
                                                0, 
                                                0,  
                                                headers); 

  if (response == 0 || ! response->isComplete()) {
    errorMsg = "could not connect to master at " + string(_masterInfo._endpoint) +
               ": " + _client->getErrorMessage();

    if (response != 0) {
      delete response;
    }

    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }

  int res = TRI_ERROR_NO_ERROR;

  if (response->wasHttpError()) {
    res = TRI_ERROR_REPLICATION_MASTER_ERROR;
    
    errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) + 
               ": HTTP " + StringUtils::itoa(response->getHttpReturnCode()) + 
               ": " + response->getHttpReturnMessage();
  }
  else {
    TRI_json_t* json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, response->getBody().str().c_str());

    if (JsonHelper::isArray(json)) {
      res = handleStateResponse(json, errorMsg);

      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
    else {
      res = TRI_ERROR_REPLICATION_INVALID_RESPONSE;

      errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) + 
        ": invalid JSON";
    }
  }

  delete response;

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle the state response of the master
////////////////////////////////////////////////////////////////////////////////

int Syncer::handleStateResponse (TRI_json_t const* json, 
                                 string& errorMsg) {

  // process "state" section
  TRI_json_t const* state = JsonHelper::getArrayElement(json, "state");

  if (! JsonHelper::isArray(state)) {
    errorMsg = "state section is missing from response";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // state."lastLogTick"
  TRI_json_t const* tick = JsonHelper::getArrayElement(state, "lastLogTick");

  if (! JsonHelper::isString(tick)) {
    errorMsg = "lastLogTick is missing from response";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
  const TRI_voc_tick_t lastLogTick = StringUtils::uint64(tick->_value._string.data, tick->_value._string.length - 1);

  // state."running"
  bool running = JsonHelper::getBooleanValue(state, "running", false);

  // process "server" section
  TRI_json_t const* server = JsonHelper::getArrayElement(json, "server");

  if (! JsonHelper::isArray(server)) {
    errorMsg = "server section is missing from response";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // server."version"
  TRI_json_t const* version = JsonHelper::getArrayElement(server, "version");

  if (! JsonHelper::isString(version)) {
    errorMsg = "server version is missing from response";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
  
  // server."serverId"
  TRI_json_t const* serverId = JsonHelper::getArrayElement(server, "serverId");

  if (! JsonHelper::isString(serverId)) {
    errorMsg = "server id is missing from response";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // validate all values we got
  const string masterIdString = string(serverId->_value._string.data, serverId->_value._string.length - 1);
  const TRI_server_id_t masterId = StringUtils::uint64(masterIdString);

  if (masterId == 0) {
    // invalid master id
    errorMsg = "server id in response is invalid";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  if (masterIdString == _localServerIdString) {
    // master and replica are the same instance. this is not supported.
    errorMsg = "master's id is the same as the applier server's id";

    return TRI_ERROR_REPLICATION_LOOP;
  }

  int major = 0;
  int minor = 0;

  const string versionString = string(version->_value._string.data, version->_value._string.length - 1);

  if (sscanf(versionString.c_str(), "%d.%d", &major, &minor) != 2) {
    errorMsg = "invalid master version info: " + versionString;

    return TRI_ERROR_REPLICATION_MASTER_INCOMPATIBLE;
  }

  if (major != 1 ||
      (major == 1 && minor != 4)) {
    // we can connect to a 1.4 only
    errorMsg = "incompatible master version: " + versionString;

    return TRI_ERROR_REPLICATION_MASTER_INCOMPATIBLE;
  }

  _masterInfo._majorVersion        = major;
  _masterInfo._minorVersion        = minor;
  _masterInfo._serverId            = masterId;
  _masterInfo._state._lastLogTick  = lastLogTick;
  _masterInfo._state._active       = running;

  TRI_LogMasterInfoReplication(&_masterInfo, "connected to");

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
