////////////////////////////////////////////////////////////////////////////////
/// @brief replication initial data synchroniser
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

#include "InitialSyncer.h"

#include "Basics/json.h"
#include "Basics/logging.h"
#include "Basics/tri-strings.h"
#include "Basics/JsonHelper.h"
#include "Basics/StringUtils.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Utils/CollectionGuard.h"
#include "Utils/Exception.h"
#include "Utils/transactions.h"
#include "VocBase/index.h"
#include "VocBase/document-collection.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::arango;
using namespace triagens::httpclient;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                  helper functions
// -----------------------------------------------------------------------------

static inline void mylocalgetline (char const*& p, 
                                   string& line, 
                                   char delim) {
  char const* q = p;
  while (*p != 0 && *p != delim) {
    p++;
  }

  line.assign(q, p - q);
  if (*p == delim) {
    p++;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

InitialSyncer::InitialSyncer (TRI_vocbase_t* vocbase,
                              TRI_replication_applier_configuration_t const* configuration,
                              map<string, bool> const& restrictCollections,
                              string const& restrictType,
                              bool verbose) :
  Syncer(vocbase, configuration),
  _progress("not started"),
  _restrictCollections(restrictCollections),
  _restrictType(restrictType),
  _processedCollections(),
  _batchId(0),
  _batchUpdateTime(0),
  _batchTtl(180),
  _chunkSize(),
  _verbose(verbose) {

  uint64_t c = configuration->_chunkSize;
  if (c == 0) {
    c = (uint64_t) 8 * 1024 * 1024; // 8 mb
  }

  TRI_ASSERT(c > 0);

  _chunkSize = StringUtils::itoa(c);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

InitialSyncer::~InitialSyncer () {
  if (_batchId > 0) {
    sendFinishBatch();
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief run method, performs a full synchronisation
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::run (string& errorMsg) {
  if (_client == nullptr || 
      _connection == nullptr || 
      _endpoint == nullptr) {
    errorMsg = "invalid endpoint";

    return TRI_ERROR_INTERNAL;
  }

  setProgress("fetching master state");

  int res = getMasterState(errorMsg);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  res = sendStartBatch(errorMsg);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }


  map<string, string> headers;
  string const url = BaseUrl + "/inventory?serverId=" + _localServerIdString;

  // send request
  string const progress = "fetching master inventory from " + url;
  setProgress(progress);

  SimpleHttpResult* response = _client->request(HttpRequest::HTTP_REQUEST_GET,
                                                url,
                                                nullptr,
                                                0,
                                                headers);

  if (response == nullptr || ! response->isComplete()) {
    errorMsg = "could not connect to master at " + string(_masterInfo._endpoint) +
               ": " + _client->getErrorMessage();

    if (response != nullptr) {
      delete response;
    }

    sendFinishBatch();

    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }

  if (response->wasHttpError()) {
    res = TRI_ERROR_REPLICATION_MASTER_ERROR;

    errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) +
               ": HTTP " + StringUtils::itoa(response->getHttpReturnCode()) +
               ": " + response->getHttpReturnMessage();
  }
  else {
    TRI_json_t* json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE,
                                      response->getBody().c_str());

    if (JsonHelper::isArray(json)) {
      res = handleInventoryResponse(json, errorMsg);

      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
    else {
      res = TRI_ERROR_REPLICATION_INVALID_RESPONSE;

      errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) +
        ": invalid JSON";
    }
  }

  delete response;

  sendFinishBatch();

  return res;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief send a "start batch" command
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::sendStartBatch (string& errorMsg) {
  _batchId = 0;

  map<string, string> const headers;

  string const url = BaseUrl + "/batch";
  string const body = "{\"ttl\":" + StringUtils::itoa(_batchTtl) + "}";

  // send request
  string const progress = "send batch start command to url " + url;
  setProgress(progress);

  SimpleHttpResult* response = _client->request(HttpRequest::HTTP_REQUEST_POST,
                                                url,
                                                body.c_str(),
                                                body.size(),
                                                headers);

  if (response == nullptr || ! response->isComplete()) {
    errorMsg = "could not connect to master at " + string(_masterInfo._endpoint) +
               ": " + _client->getErrorMessage();

    if (response != nullptr) {
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

  if (res == TRI_ERROR_NO_ERROR) {
    TRI_json_t* json = TRI_JsonString(TRI_CORE_MEM_ZONE,
                                      response->getBody().c_str());

    if (json == nullptr) {
      res = TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }
    else {
      string const id = JsonHelper::getStringValue(json, "id", "");

      if (id.empty()) {
        res = TRI_ERROR_REPLICATION_INVALID_RESPONSE;
      }
      else {
        _batchId = StringUtils::uint64(id);
        _batchUpdateTime = TRI_microtime();
      }

      TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
    }
  }

  delete response;

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send an "extend batch" command
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::sendExtendBatch () {
  if (_batchId == 0) {
    return TRI_ERROR_NO_ERROR;
  }

  double now = TRI_microtime();

  if (now <= _batchUpdateTime + _batchTtl - 60) {
    // no need to extend the batch yet
    return TRI_ERROR_NO_ERROR;
  }

  map<string, string> const headers;

  string const url = BaseUrl + "/batch/" + StringUtils::itoa(_batchId);
  string const body = "{\"ttl\":" + StringUtils::itoa(_batchTtl) + "}";

  // send request
  string const progress = "send batch start command to url " + url;
  setProgress(progress);

  SimpleHttpResult* response = _client->request(HttpRequest::HTTP_REQUEST_PUT,
                                                url,
                                                body.c_str(),
                                                body.size(),
                                                headers);

  if (response == nullptr || ! response->isComplete()) {
    if (response != nullptr) {
      delete response;
    }

    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }

  int res = TRI_ERROR_NO_ERROR;

  if (response->wasHttpError()) {
    res = TRI_ERROR_REPLICATION_MASTER_ERROR;
  }
  else {
    _batchUpdateTime = TRI_microtime();
  }

  delete response;

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send a "finish batch" command
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::sendFinishBatch () {
  if (_batchId == 0) {
    return TRI_ERROR_NO_ERROR;
  }

  map<string, string> const headers;
  string const url = BaseUrl + "/batch/" + StringUtils::itoa(_batchId);

  // send request
  string const progress = "send batch finish command to url " + url;
  setProgress(progress);

  SimpleHttpResult* response = _client->request(HttpRequest::HTTP_REQUEST_DELETE,
                                                url,
                                                nullptr,
                                                0,
                                                headers);

  if (response == nullptr || ! response->isComplete()) {
    if (response != nullptr) {
      delete response;
    }

    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }

  int res = TRI_ERROR_NO_ERROR;

  if (response->wasHttpError()) {
    res = TRI_ERROR_REPLICATION_MASTER_ERROR;
  }
  else {
    _batchId = 0;
    _batchUpdateTime = 0;
  }

  delete response;

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply the data from a collection dump
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::applyCollectionDump (TRI_transaction_collection_t* trxCollection,
                                        SimpleHttpResult* response,
                                        string& errorMsg) {

  const string invalidMsg = "received invalid JSON data for collection " +
                            StringUtils::itoa(trxCollection->_cid);

  StringBuffer& data = response->getBody();
  char const* p = data.c_str();

  while (true) {
    string line;

    mylocalgetline(p, line, '\n');

    if (line.size() < 2) {
      // we are done
      return TRI_ERROR_NO_ERROR;
    }

    TRI_json_t* json = TRI_JsonString(TRI_CORE_MEM_ZONE, line.c_str());

    if (! JsonHelper::isArray(json)) {
      if (json != nullptr) {
        TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
      }

      errorMsg = invalidMsg;

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    TRI_replication_operation_e type = REPLICATION_INVALID;
    char const* key       = nullptr;
    TRI_json_t const* doc = nullptr;
    TRI_voc_rid_t rid     = 0;

    size_t const n = json->_value._objects._length;

    for (size_t i = 0; i < n; i += 2) {
      TRI_json_t const* element = static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i));

      if (! JsonHelper::isString(element)) {
        TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
        errorMsg = invalidMsg;

        return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
      }

      char const* attributeName = element->_value._string.data;
      TRI_json_t const* value = static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i + 1));

      if (TRI_EqualString(attributeName, "type")) {
        if (JsonHelper::isNumber(value)) {
          type = (TRI_replication_operation_e) (int) value->_value._number;
        }
      }

      else if (TRI_EqualString(attributeName, "key")) {
        if (JsonHelper::isString(value)) {
          key = value->_value._string.data;
        }
      }

      else if (TRI_EqualString(attributeName, "rev")) {
        if (JsonHelper::isString(value)) {
          rid = StringUtils::uint64(value->_value._string.data, value->_value._string.length - 1);
        }
      }

      else if (TRI_EqualString(attributeName, "data")) {
        if (JsonHelper::isArray(value)) {
          doc = value;
        }
      }
    }

    // key must not be 0, but doc can be 0!
    if (key == nullptr) {
      TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
      errorMsg = invalidMsg;

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    int res = applyCollectionDumpMarker(trxCollection, type, (const TRI_voc_key_t) key, rid, doc, errorMsg);

    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief incrementally fetch data from a collection
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::handleCollectionDump (TRI_transaction_collection_t* trxCollection,
                                         string const& collectionName,
                                         TRI_voc_tick_t maxTick,
                                         string& errorMsg) {

  string const cid = StringUtils::itoa(trxCollection->_cid);

  string const baseUrl = BaseUrl +
                         "/dump?collection=" + cid +
                         "&chunkSize=" + _chunkSize;

  map<string, string> headers;

  TRI_voc_tick_t fromTick = 0;
  int batch = 1;

  while (1) {
    sendExtendBatch();

    string url = baseUrl + "&from=" + StringUtils::itoa(fromTick);

    if (maxTick > 0) {
      url += "&to=" + StringUtils::itoa(maxTick);
    }

    url += "&serverId=" + _localServerIdString;

    // send request
    string const progress = "fetching master collection dump for collection '" + collectionName +
                            "', id " + cid + ", batch " + StringUtils::itoa(batch);

    setProgress(progress.c_str());

    SimpleHttpResult* response = _client->request(HttpRequest::HTTP_REQUEST_GET,
                                                  url,
                                                  nullptr,
                                                  0,
                                                  headers);

    if (response == nullptr || ! response->isComplete()) {
      errorMsg = "could not connect to master at " + string(_masterInfo._endpoint) +
                 ": " + _client->getErrorMessage();

      if (response != nullptr) {
        delete response;
      }

      return TRI_ERROR_REPLICATION_NO_RESPONSE;
    }

    if (response->wasHttpError()) {
      errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) +
                 ": HTTP " + StringUtils::itoa(response->getHttpReturnCode()) +
                 ": " + response->getHttpReturnMessage();

      delete response;

      return TRI_ERROR_REPLICATION_MASTER_ERROR;
    }

    int res;
    bool checkMore = false;
    bool found;
    TRI_voc_tick_t tick;

    string header = response->getHeaderField(TRI_REPLICATION_HEADER_CHECKMORE, found);
    if (found) {
      checkMore = StringUtils::boolean(header);
      res = TRI_ERROR_NO_ERROR;

      if (checkMore) {
        header = response->getHeaderField(TRI_REPLICATION_HEADER_LASTINCLUDED, found);
        if (found) {
          tick = StringUtils::uint64(header);

          if (tick > fromTick) {
            fromTick = tick;
          }
          else {
            // we got the same tick again, this indicates we're at the end
            checkMore = false;
          }
        }
      }
    }

    if (! found) {
      errorMsg = "got invalid response from master at " + string(_masterInfo._endpoint) +
                 ": required header is missing";
      res = TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    if (res == TRI_ERROR_NO_ERROR) {
      res = applyCollectionDump(trxCollection, response, errorMsg);
    }

    delete response;

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    if (! checkMore || fromTick == 0) {
      // done
      return res;
    }

    batch++;
  }

  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle the information about a collection
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::handleCollection (TRI_json_t const* parameters,
                                     TRI_json_t const* indexes,
                                     string& errorMsg,
                                     sync_phase_e phase) {

  sendExtendBatch();

  string const masterName = JsonHelper::getStringValue(parameters, "name", "");

  if (masterName.empty()) {
    errorMsg = "collection name is missing in response";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  if (TRI_ExcludeCollectionReplication(masterName.c_str())) {
    // we're not interested in this collection
    return TRI_ERROR_NO_ERROR;
  }

  if (JsonHelper::getBooleanValue(parameters, "deleted", false)) {
    // we don't care about deleted collections
    return TRI_ERROR_NO_ERROR;
  }

  TRI_json_t const* masterId = JsonHelper::getArrayElement(parameters, "cid");

  if (! JsonHelper::isString(masterId)) {
    errorMsg = "collection id is missing in response";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_voc_cid_t cid = StringUtils::uint64(masterId->_value._string.data, masterId->_value._string.length - 1);
  string const collectionMsg = "collection '" + masterName + "', id " + StringUtils::itoa(cid);


  if (! _restrictType.empty()) {
    map<string, bool>::const_iterator it = _restrictCollections.find(masterName);

    bool found = (it != _restrictCollections.end());

    if (_restrictType == "include" && ! found) {
      // collection should not be included
      return TRI_ERROR_NO_ERROR;
    }
    else if (_restrictType == "exclude" && found) {
      return TRI_ERROR_NO_ERROR;
    }
  }


  // phase handling
  if (phase == PHASE_VALIDATE) {
    // validation phase just returns ok if we got here (aborts above if data is invalid)
    _processedCollections.insert(std::pair<TRI_voc_cid_t, string>(cid, masterName));

    return TRI_ERROR_NO_ERROR;
  }

  // drop collections locally
  // -------------------------------------------------------------------------------------

  if (phase == PHASE_DROP) {
    // first look up the collection by the cid
    TRI_vocbase_col_t* col = TRI_LookupCollectionByIdVocBase(_vocbase, cid);

    if (col == nullptr && ! masterName.empty()) {
      // not found, try name next
      col = TRI_LookupCollectionByNameVocBase(_vocbase, masterName.c_str());
    }

    if (col != nullptr) {
      string const progress = "dropping " + collectionMsg;
      setProgress(progress.c_str());

      int res = TRI_DropCollectionVocBase(_vocbase, col, true);

      if (res != TRI_ERROR_NO_ERROR) {
        errorMsg = "unable to drop " + collectionMsg + ": " + TRI_errno_string(res);

        return res;
      }
    }

    return TRI_ERROR_NO_ERROR;
  }

  // re-create collections locally
  // -------------------------------------------------------------------------------------

  else if (phase == PHASE_CREATE) {
    TRI_vocbase_col_t* col = nullptr;

    string const progress = "creating " + collectionMsg;
    setProgress(progress.c_str());

    int res = createCollection(parameters, &col);

    if (res != TRI_ERROR_NO_ERROR) {
      errorMsg = "unable to create " + collectionMsg + ": " + TRI_errno_string(res);

      return res;
    }

    return TRI_ERROR_NO_ERROR;
  }

  // sync collection data
  // -------------------------------------------------------------------------------------

  else if (phase == PHASE_DUMP) {
    string const progress = "syncing data for " + collectionMsg;
    setProgress(progress.c_str());

    int res = TRI_ERROR_INTERNAL;

    {
      SingleCollectionWriteTransaction<RestTransactionContext, UINT64_MAX> trx(_vocbase, cid);

      res = trx.begin();

      if (res != TRI_ERROR_NO_ERROR) {
        errorMsg = "unable to start transaction: " + string(TRI_errno_string(res));

        return res;
      }

      TRI_transaction_collection_t* trxCollection = trx.trxCollection();

      if (trxCollection == nullptr) {
        res = TRI_ERROR_INTERNAL;
        errorMsg = "unable to start transaction: " + string(TRI_errno_string(res));
      }
      else {
        res = handleCollectionDump(trxCollection, masterName, _masterInfo._lastLogTick, errorMsg);
      }

      res = trx.finish(res);
    }

    if (res == TRI_ERROR_NO_ERROR) {
      // now create indexes
      size_t const n = indexes->_value._objects._length;

      if (n > 0) {
        string const progress = "creating indexes for " + collectionMsg;
        setProgress(progress.c_str());

        TRI_ReadLockReadWriteLock(&_vocbase->_inventoryLock);

        try {
          triagens::arango::CollectionGuard guard(_vocbase, cid, false);
          TRI_vocbase_col_t* col = guard.collection();

          if (col == nullptr) {
            res = TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
          }
          else {
            TRI_document_collection_t* document = col->_collection;
            TRI_ASSERT(document != nullptr);

            // create a fake transaction object to avoid assertions
            TransactionBase trx(true);
            TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

            for (size_t i = 0; i < n; ++i) {
              TRI_json_t const* idxDef = static_cast<TRI_json_t const*>(TRI_AtVector(&indexes->_value._objects, i));
              TRI_index_t* idx = nullptr;
 
              // {"id":"229907440927234","type":"hash","unique":false,"fields":["x","Y"]}
    
              res = TRI_FromJsonIndexDocumentCollection(document, idxDef, &idx);

              if (res != TRI_ERROR_NO_ERROR) {
                errorMsg = "could not create index: " + string(TRI_errno_string(res));
                break;
              }
              else {
                TRI_ASSERT(idx != nullptr);

                res = TRI_SaveIndex(document, idx, true);

                if (res != TRI_ERROR_NO_ERROR) {
                  errorMsg = "could not save index: " + string(TRI_errno_string(res));
                  break;
                }
              }
            }

            TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);
          }
        }
        catch (triagens::arango::Exception const& ex) {
          res = ex.code();
        }
        catch (...) {
          res = TRI_ERROR_INTERNAL;
        }

        TRI_ReadUnlockReadWriteLock(&_vocbase->_inventoryLock);
      }
    }

    return res;
  }


  // we won't get here
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle the inventory response of the master
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::handleInventoryResponse (TRI_json_t const* json,
                                            string& errorMsg) {
  TRI_json_t* collections = JsonHelper::getArrayElement(json, "collections");

  if (! JsonHelper::isList(collections)) {
    errorMsg = "collections section is missing from response";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  int res;

  // STEP 1: validate collection declarations from master
  // ----------------------------------------------------------------------------------

  // iterate over all collections from the master...
  res = iterateCollections(collections, errorMsg, PHASE_VALIDATE);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }


  // STEP 2: drop collections locally if they are also present on the master (clean up)
  // ----------------------------------------------------------------------------------

  res = iterateCollections(collections, errorMsg, PHASE_DROP);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }


  // STEP 3: re-create empty collections locally
  // ----------------------------------------------------------------------------------

  res = iterateCollections(collections, errorMsg, PHASE_CREATE);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }


  // STEP 4: sync collection data from master and create initial indexes
  // ----------------------------------------------------------------------------------

  res = iterateCollections(collections, errorMsg, PHASE_DUMP);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate over all collections from a list and apply an action
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::iterateCollections (TRI_json_t const* collections,
                                       string& errorMsg,
                                       sync_phase_e phase) {
  size_t const n = collections->_value._objects._length;

  for (size_t i = 0; i < n; ++i) {
    TRI_json_t const* collection = static_cast<TRI_json_t const*>(TRI_AtVector(&collections->_value._objects, i));

    if (! JsonHelper::isArray(collection)) {
      errorMsg = "collection declaration is invalid in response";

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    TRI_json_t const* parameters = JsonHelper::getArrayElement(collection, "parameters");

    if (! JsonHelper::isArray(parameters)) {
      errorMsg = "collection parameters declaration is invalid in response";

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    TRI_json_t const* indexes = JsonHelper::getArrayElement(collection, "indexes");

    if (! JsonHelper::isList(indexes)) {
      errorMsg = "collection indexes declaration is invalid in response";

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    int res = handleCollection(parameters, indexes, errorMsg, phase);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  // all ok
  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
