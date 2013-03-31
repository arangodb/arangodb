////////////////////////////////////////////////////////////////////////////////
/// @brief import request handler
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
/// @author Dr. Frank Celler
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestImportHandler.h"

#include "Basics/StringUtils.h"
#include "BasicsC/string-buffer.h"
#include "BasicsC/tri-strings.h"
#include "Rest/HttpRequest.h"
#include "Utilities/ResourceHolder.h"
#include "VocBase/document-collection.h"
#include "VocBase/edge-collection.h"
#include "VocBase/vocbase.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestImportHandler::RestImportHandler (HttpRequest* request, TRI_vocbase_t* vocbase)
  : RestVocbaseBaseHandler(request, vocbase) {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestImportHandler::isDirect () {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

string const& RestImportHandler::queue () {
  static string const client = "STANDARD";

  return client;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_e RestImportHandler::execute () {

  // extract the sub-request type
  HttpRequest::HttpRequestType type = _request->requestType();

  // prepare logging
  static LoggerData::Task const logCreate(DOCUMENT_IMPORT_PATH + " [create]");
  static LoggerData::Task const logIllegal(DOCUMENT_IMPORT_PATH + " [illegal]");

  LoggerData::Task const * task = &logCreate;

  switch (type) {
    case HttpRequest::HTTP_REQUEST_POST: task = &logCreate; break;
    default: task = &logIllegal; break;
  }

  _timing << *task;

  // if ifdef is not used, the compiler will complain
#ifdef TRI_ENABLE_LOGGER
  LOGGER_REQUEST_IN_START_I(_timing, "");
#endif

  bool res = false;

  switch (type) {
    case HttpRequest::HTTP_REQUEST_POST: {
        // extract the import type
        bool found;
        string documentType = _request->value("type", found);

        if (found && documentType == "documents") {
          // lines with individual JSON documents
          res = createByDocumentsLines();
        }
        else if (found && documentType == "array") {
          // one JSON array
          res = createByDocumentsList();
        }
        else {
          // CSV
          res = createByKeyValueList();
        }
      }
      break;
    default:
      res = false;
      generateNotImplemented("ILLEGAL " + DOCUMENT_IMPORT_PATH);
      break;
  }

  _timingResult = res ? RES_ERR : RES_OK;

  // this handler is done
  return HANDLER_DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief log an error document
////////////////////////////////////////////////////////////////////////////////

void RestImportHandler::logDocument (const TRI_json_t* const json) const {
  TRI_string_buffer_t buffer;

  TRI_InitStringBuffer(&buffer, TRI_UNKNOWN_MEM_ZONE);
  int res = TRI_StringifyJson(&buffer, json);
  if (res == TRI_ERROR_NO_ERROR) {
    LOGGER_WARNING("offending document" << buffer._buffer);
  }
  TRI_DestroyStringBuffer(&buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates documents
///
/// @REST{POST /_api/import?type=documents&collection=@FA{collection-name}}
///
/// Creates documents in the collection identified by @FA{collection-name}.
/// The JSON representations of the documents must be passed as the body of the
/// POST request.
///
/// If the documents were created successfully, then a @LIT{HTTP 201} is returned.
////////////////////////////////////////////////////////////////////////////////

bool RestImportHandler::createByDocumentsLines () {
  size_t numCreated = 0;
  size_t numError = 0;
  size_t numEmpty = 0;

  vector<string> const& suffix = _request->suffix();

  if (suffix.size() != 0) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "superfluous suffix, expecting " + DOCUMENT_IMPORT_PATH + "?type=documents&collection=<identifier>");
    return false;
  }

  const bool waitForSync = extractWaitForSync();

  // extract the collection name
  bool found;
  const string& collection = _request->value("collection", found);

  if (! found || collection.empty()) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,
                  "'collection' is missing, expecting " + DOCUMENT_IMPORT_PATH + "?collection=<identifier>");
    return false;
  }

  if (! checkCreateCollection(collection, TRI_COL_TYPE_DOCUMENT)) {
    return false;
  }

  // find and load collection given by name or identifier
  SingleCollectionWriteTransaction<StandaloneTransaction<RestTransactionContext>, UINT64_MAX> trx(_vocbase, _resolver, collection);

  // .............................................................................
  // inside write transaction
  // .............................................................................

  int res = trx.begin();
  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
    return false;
  }

  TRI_primary_collection_t* primary = trx.primaryCollection();
  const TRI_voc_cid_t cid = trx.cid();
  const bool isEdgeCollection = (primary->base._info._type == TRI_COL_TYPE_EDGE);

  trx.lockWrite();

  const char* ptr = _request->body();
  const char* end = ptr + _request->bodySize();
  string line;
  size_t i = 0;

  while (ptr < end) {
    i++;
    const char* pos = strchr(ptr, '\n');

    if (pos == 0) {
      line.assign(ptr, (size_t) (end - ptr));
      ptr = end;
    }
    else {
      line.assign(ptr, (size_t) (pos - ptr));
      ptr = pos + 1;
    }

    StringUtils::trimInPlace(line, "\r\n\t ");
    if (line.length() == 0) {
      ++numEmpty;
      continue;
    }

    TRI_json_t* values = parseJsonLine(line);

    if (values == 0 || values->_type != TRI_JSON_ARRAY) {
      LOGGER_WARNING("invalid JSON type (expecting array) at position " << i);
      ++numError;
    }
    else {
      // now save the document
      TRI_doc_mptr_t document;

      if (isEdgeCollection) {
        const char* from = extractJsonStringValue(values, "_from");

        if (from == 0) {
          LOGGER_WARNING("missing '_from' attribute at position " << i);
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, values);
          ++numError;
          continue;
        }

        const char* to = extractJsonStringValue(values, "_to");
        if (to == 0) {
          LOGGER_WARNING("missing '_to' attribute at position " << i);
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, values);
          ++numError;
          continue;
        }

        TRI_document_edge_t edge;

        edge._fromCid = cid;
        edge._toCid = cid;
        edge._fromKey = 0;
        edge._toKey = 0;

        int res1 = parseDocumentId(from, edge._fromCid, edge._fromKey);
        int res2 = parseDocumentId(to, edge._toCid, edge._toKey);

        if (res1 == TRI_ERROR_NO_ERROR && res2 == TRI_ERROR_NO_ERROR) {
          res = trx.createEdge(&document, values, waitForSync, &edge, false);
        }

        if (edge._fromKey != 0) {
          TRI_Free(TRI_CORE_MEM_ZONE, edge._fromKey);
        }
        if (edge._toKey != 0) {
          TRI_Free(TRI_CORE_MEM_ZONE, edge._toKey);
        }
      }
      else {
        // do not acquire an extra lock
        res = trx.createDocument(&document, values, waitForSync, false);
      }


      if (res == TRI_ERROR_NO_ERROR) {
        ++numCreated;
      }
      else {
        LOGGER_WARNING("creating document failed with error: " << TRI_errno_string(res));
        logDocument(values);
        ++numError;
      }

      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, values);
    }
  }

  // this will commit, even if previous errors occurred
  res = trx.commit();

  // .............................................................................
  // outside write transaction
  // .............................................................................

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
  }
  else {
    // generate result
    generateDocumentsCreated(numCreated, numError, numEmpty);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates documents
///
/// @REST{POST /_api/import?type=array&collection=@FA{collection-name}}
///
/// Creates documents in the collection identified by @FA{collection-name}.
/// The JSON representations of the documents must be passed as the body of the
/// POST request.
///
/// If the documents were created successfully, then a @LIT{HTTP 201} is returned.
////////////////////////////////////////////////////////////////////////////////

bool RestImportHandler::createByDocumentsList () {
  size_t numCreated = 0;
  size_t numError = 0;

  vector<string> const& suffix = _request->suffix();

  if (suffix.size() != 0) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "superfluous suffix, expecting " + DOCUMENT_IMPORT_PATH + "?type=array&collection=<identifier>");
    return false;
  }

  const bool waitForSync = extractWaitForSync();

  // extract the collection name
  bool found;
  const string& collection = _request->value("collection", found);

  if (! found || collection.empty()) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,
                  "'collection' is missing, expecting " + DOCUMENT_IMPORT_PATH + "?collection=<identifier>");
    return false;
  }

  ResourceHolder holder;

  char* errmsg = 0;
  TRI_json_t* documents = TRI_Json2String(TRI_UNKNOWN_MEM_ZONE, _request->body(), &errmsg);

  if (! holder.registerJson(TRI_UNKNOWN_MEM_ZONE, documents)) {
    if (errmsg == 0) {
      generateError(HttpResponse::BAD,
                    TRI_ERROR_HTTP_CORRUPTED_JSON,
                    "cannot parse json object");
    }
    else {
      generateError(HttpResponse::BAD,
                    TRI_ERROR_HTTP_CORRUPTED_JSON,
                    errmsg);

      TRI_FreeString(TRI_CORE_MEM_ZONE, errmsg);
    }
    return false;
  }

  if (documents->_type != TRI_JSON_LIST) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting a JSON array in the request");
    return false;
  }

  if (! checkCreateCollection(collection, TRI_COL_TYPE_DOCUMENT)) {
    return false;
  }

  // find and load collection given by name or identifier
  SingleCollectionWriteTransaction<StandaloneTransaction<RestTransactionContext>, UINT64_MAX> trx(_vocbase, _resolver, collection);

  // .............................................................................
  // inside write transaction
  // .............................................................................

  int res = trx.begin();
  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
    return false;
  }

  TRI_primary_collection_t* primary = trx.primaryCollection();
  const TRI_voc_cid_t cid = trx.cid();
  const bool isEdgeCollection = (primary->base._info._type == TRI_COL_TYPE_EDGE);

  trx.lockWrite();

  for (size_t i = 0; i < documents->_value._objects._length; ++i) {
    TRI_json_t* values = (TRI_json_t*) TRI_AtVector(&documents->_value._objects, i);

    if (values == 0 || values->_type != TRI_JSON_ARRAY) {
      LOGGER_WARNING("invalid JSON type (expecting array) at position " << (i + 1));
      ++numError;
    }
    else {
      // now save the document
      TRI_doc_mptr_t document;

      if (isEdgeCollection) {
        const char* from = extractJsonStringValue(values, "_from");

        if (from == 0) {
          LOGGER_WARNING("missing '_from' attribute at position " << (i + 1));
          ++numError;
          continue;
        }

        const char* to = extractJsonStringValue(values, "_to");
        if (to == 0) {
          LOGGER_WARNING("missing '_to' attribute at position " << (i + 1));
          ++numError;
          continue;
        }

        TRI_document_edge_t edge;

        edge._fromCid = cid;
        edge._toCid = cid;
        edge._fromKey = 0;
        edge._toKey = 0;

        int res1 = parseDocumentId(from, edge._fromCid, edge._fromKey);
        int res2 = parseDocumentId(to, edge._toCid, edge._toKey);

        if (res1 == TRI_ERROR_NO_ERROR && res2 == TRI_ERROR_NO_ERROR) {
          res = trx.createEdge(&document, values, waitForSync, &edge, false);
        }

        if (edge._fromKey != 0) {
          TRI_Free(TRI_CORE_MEM_ZONE, edge._fromKey);
        }
        if (edge._toKey != 0) {
          TRI_Free(TRI_CORE_MEM_ZONE, edge._toKey);
        }
      }
      else {
        // do not acquire an extra lock
        res = trx.createDocument(&document, values, waitForSync, false);
      }


      if (res == TRI_ERROR_NO_ERROR) {
        ++numCreated;
      }
      else {
        LOGGER_WARNING("creating document failed with error: " << TRI_errno_string(res));
        logDocument(values);
        ++numError;
      }
    }
  }

  // this will commit, even if previous errors occurred
  res = trx.commit();

  // .............................................................................
  // outside write transaction
  // .............................................................................

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
  }
  else {
    // generate result
    size_t numEmpty = 0;
    generateDocumentsCreated(numCreated, numError, numEmpty);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates documents
///
/// @REST{POST /_api/import?collection=@FA{collection-name}}
///
/// Creates documents in the collection identified by @FA{collection-name}.
/// The JSON representations of the documents must be passed as the body of the
/// POST request.
///
/// If the documents were created successfully, then a @LIT{HTTP 201} is returned.
////////////////////////////////////////////////////////////////////////////////

bool RestImportHandler::createByKeyValueList () {
  size_t numCreated = 0;
  size_t numError = 0;
  size_t numEmpty = 0;

  vector<string> const& suffix = _request->suffix();

  if (suffix.size() != 0) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "superfluous suffix, expecting " + DOCUMENT_IMPORT_PATH + "?collection=<identifier>");
    return false;
  }

  const bool waitForSync = extractWaitForSync();

  // extract the collection name
  bool found;
  const string& collection = _request->value("collection", found);

  if (! found || collection.empty()) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,
                  "'collection' is missing, expecting " + DOCUMENT_IMPORT_PATH + "?collection=<identifier>");
    return false;
  }

  // read line number (optional)
  const string& lineNumValue = _request->value("line", found);
  int64_t lineNumber = 0;
  if (found) {
    lineNumber = StringUtils::int64(lineNumValue);
  }


  size_t start = 0;
  string body(_request->body(), _request->bodySize());
  size_t next = body.find('\n', start);

  if (next == string::npos) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "no JSON list in second line found");
    return false;
  }

  ResourceHolder holder;
  TRI_json_t* keys = 0;

  string line = body.substr(start, next);
  StringUtils::trimInPlace(line, "\r\n\t ");

  // get first line
  if (line != "") {
    keys = parseJsonLine(line);
    if (! holder.registerJson(TRI_UNKNOWN_MEM_ZONE, keys)) {
      LOGGER_WARNING("no JSON data in first line");
      generateError(HttpResponse::BAD,
                    TRI_ERROR_HTTP_BAD_PARAMETER,
                    "no JSON string list in first line found");
      return false;
    }

    if (keys->_type == TRI_JSON_LIST) {
      if (! checkKeys(keys)) {
        LOGGER_WARNING("no JSON string list in first line found");
        generateError(HttpResponse::BAD,
                      TRI_ERROR_HTTP_BAD_PARAMETER,
                      "no JSON string list in first line found");
        return false;
      }
      start = next + 1;
    }
    else {
      LOGGER_WARNING("no JSON string list in first line found");
      generateError(HttpResponse::BAD,
                    TRI_ERROR_HTTP_BAD_PARAMETER,
                    "no JSON string list in first line found");
      return false;
    }
  }
  else {
    LOGGER_WARNING("no JSON string list in first line found");
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "no JSON string list in first line found");
    return false;
  }

  if (! checkCreateCollection(collection, TRI_COL_TYPE_DOCUMENT)) {
    return false;
  }


  // find and load collection given by name or identifier
  SingleCollectionWriteTransaction<StandaloneTransaction<RestTransactionContext>, UINT64_MAX> trx(_vocbase, _resolver, collection);

  // .............................................................................
  // inside write transaction
  // .............................................................................

  int res = trx.begin();
  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
    return false;
  }

  TRI_primary_collection_t* primary = trx.primaryCollection();
  const TRI_voc_cid_t cid = trx.cid();
  const bool isEdgeCollection = (primary->base._info._type == TRI_COL_TYPE_EDGE);

  trx.lockWrite();

  // .............................................................................
  // inside write transaction
  // .............................................................................

  while (next != string::npos && start < body.length()) {
    lineNumber++;

    next = body.find('\n', start);

    if (next == string::npos) {
      line = body.substr(start);
    }
    else {
      line = body.substr(start, next - start);
      start = next + 1;
    }

    StringUtils::trimInPlace(line, "\r\n\t ");
    if (line.length() == 0) {
      ++numEmpty;
      continue;
    }

    TRI_json_t* values = parseJsonLine(line);

    if (values) {
      // got a json document or list
      TRI_json_t* json;

      // build the json object from the list
      json = createJsonObject(keys, values, line);
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, values);

      if (json == 0) {
        ++numError;
        continue;
      }

      // now save the document
      TRI_doc_mptr_t document;

      if (isEdgeCollection) {
        const char* from = extractJsonStringValue(json, "_from");

        if (from == 0) {
          LOGGER_WARNING("missing '_from' attribute at line " << lineNumber);
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
          ++numError;
          continue;
        }

        const char* to = extractJsonStringValue(json, "_to");
        if (to == 0) {
          LOGGER_WARNING("missing '_to' attribute at line " << lineNumber);
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
          ++numError;
          continue;
        }

        TRI_document_edge_t edge;

        edge._fromCid = cid;
        edge._toCid = cid;
        edge._fromKey = 0;
        edge._toKey = 0;

        int res1 = parseDocumentId(from, edge._fromCid, edge._fromKey);
        int res2 = parseDocumentId(to, edge._toCid, edge._toKey);

        if (res1 == TRI_ERROR_NO_ERROR && res2 == TRI_ERROR_NO_ERROR) {
          res = trx.createEdge(&document, json, waitForSync, &edge, false);
        }

        if (edge._fromKey != 0) {
          TRI_Free(TRI_CORE_MEM_ZONE, edge._fromKey);
        }
        if (edge._toKey != 0) {
          TRI_Free(TRI_CORE_MEM_ZONE, edge._toKey);
        }
      }
      else {
        // do not acquire an extra lock
        res = trx.createDocument(&document, json, waitForSync, false);
      }


      if (res == TRI_ERROR_NO_ERROR) {
        ++numCreated;
      }
      else {
        LOGGER_WARNING("creating document failed with error: " << TRI_errno_string(res));
        logDocument(json);
        ++numError;
      }

      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
    else {
      LOGGER_WARNING("no valid JSON data in line: " << line);
      ++numError;
    }

  }

  // we'll always commit, even if previous errors occurred
  res = trx.commit();

  // .............................................................................
  // outside write transaction
  // .............................................................................

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
  }
  else {
    // generate result
    generateDocumentsCreated(numCreated, numError, numEmpty);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create response for number of documents created / failed
////////////////////////////////////////////////////////////////////////////////

void RestImportHandler::generateDocumentsCreated (size_t numCreated, size_t numError, size_t numEmpty) {
  _response = createResponse(HttpResponse::CREATED);
  _response->setContentType("application/json; charset=utf-8");

  _response->body()
    .appendText("{\"error\":false,\"created\":")
    .appendInteger(numCreated)
    .appendText(",\"errors\":")
    .appendInteger(numError)
    .appendText(",\"empty\":")
    .appendInteger(numEmpty)
    .appendText("}");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse a single document line
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* RestImportHandler::parseJsonLine (const string& line) {
  char* errmsg = 0;
  TRI_json_t* json = TRI_Json2String(TRI_UNKNOWN_MEM_ZONE, line.c_str(), &errmsg);

  if (errmsg != 0) {
    // must free this error message, otherwise we'll have a memleak
    TRI_FreeString(TRI_CORE_MEM_ZONE, errmsg);
  }
  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a JSON object from a line containing a document
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* RestImportHandler::createJsonObject (const TRI_json_t* keys,
                                                 const TRI_json_t* values,
                                                 const string& line) {

  if (values->_type != TRI_JSON_LIST) {
    LOGGER_WARNING("no valid JSON list data in line: " << line);
    return 0;
  }

  if (keys->_value._objects._length !=  values->_value._objects._length) {
    LOGGER_WARNING("wrong number of JSON values in line: " << line);
    return 0;
  }

  TRI_json_t* result = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);
  if (result == 0) {
    LOGGER_ERROR("out of memory");
    return 0;
  }

  size_t n = keys->_value._objects._length;

  for (size_t i = 0;  i < n;  ++i) {

    TRI_json_t* key   = (TRI_json_t*) TRI_AtVector(&keys->_value._objects, i);
    TRI_json_t* value = (TRI_json_t*) TRI_AtVector(&values->_value._objects, i);

    if (key->_type == TRI_JSON_STRING && value->_type > TRI_JSON_NULL) {
      TRI_InsertArrayJson(TRI_UNKNOWN_MEM_ZONE, result, key->_value._string.data, value);
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate keys
////////////////////////////////////////////////////////////////////////////////

bool RestImportHandler::checkKeys (TRI_json_t* keys) {
  if (keys->_type != TRI_JSON_LIST) {
    return false;
  }

  const size_t n = keys->_value._objects._length;

  if (n == 0) {
    return false;
  }

  for (size_t i = 0;  i < n;  ++i) {
    TRI_json_t* key = (TRI_json_t*) TRI_AtVector(&keys->_value._objects, i);

    if (key->_type != TRI_JSON_STRING) {
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
