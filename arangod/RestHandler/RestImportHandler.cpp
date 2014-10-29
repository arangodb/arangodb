////////////////////////////////////////////////////////////////////////////////
/// @brief import request handler
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestImportHandler.h"

#include "Basics/JsonHelper.h"
#include "Basics/StringUtils.h"
#include "Basics/tri-strings.h"
#include "Rest/HttpRequest.h"
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
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestImportHandler::RestImportHandler (HttpRequest* request)
  : RestVocbaseBaseHandler(request) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_t RestImportHandler::execute () {
  if (ServerState::instance()->isCoordinator()) {
    generateError(HttpResponse::NOT_IMPLEMENTED,
                  TRI_ERROR_CLUSTER_UNSUPPORTED,
                  "'/_api/import' is not yet supported in a cluster");
    return status_t(HANDLER_DONE);
  }

  // extract the sub-request type
  HttpRequest::HttpRequestType type = _request->requestType();

  switch (type) {
    case HttpRequest::HTTP_REQUEST_POST: {
      // extract the import type
      bool found;
      string const documentType = _request->value("type", found);

      if (found &&
          (documentType == "documents" ||
           documentType == "array" ||
           documentType == "list" ||
           documentType == "auto")) {
        createFromJson(documentType);
      }
      else {
        // CSV
        createFromKeyValueList();
      }
      break;
    }

    default:
      generateNotImplemented("ILLEGAL " + DOCUMENT_IMPORT_PATH);
      break;
  }

  // this handler is done
  return status_t(HANDLER_DONE);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the "overwrite" value
////////////////////////////////////////////////////////////////////////////////

bool RestImportHandler::extractOverwrite () const {
  bool found;
  char const* overwrite = _request->value("overwrite", found);

  if (found) {
    return StringUtils::boolean(overwrite);
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the "complete" value
////////////////////////////////////////////////////////////////////////////////

bool RestImportHandler::extractComplete () const {
  bool found;
  char const* forceStr = _request->value("complete", found);

  if (found) {
    return StringUtils::boolean(forceStr);
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a position string
////////////////////////////////////////////////////////////////////////////////

std::string RestImportHandler::positionise (size_t i) const {
  return string("at position " + StringUtils::itoa(i) + ": ");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register an error
////////////////////////////////////////////////////////////////////////////////

void RestImportHandler::registerError (RestImportResult& result,
                                       std::string const& errorMsg) {
  ++result._numErrors;

  result._errors.push_back(errorMsg);

  LOG_WARNING("%s", errorMsg.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process a single JSON document
////////////////////////////////////////////////////////////////////////////////

int RestImportHandler::handleSingleDocument (RestImportTransaction& trx,
                                             TRI_json_t const* json,
                                             string& errorMsg,
                                             bool isEdgeCollection,
                                             bool waitForSync,
                                             size_t i) {
  if (! TRI_IsArrayJson(json)) {
    errorMsg = positionise(i) + "invalid JSON type (expecting array)";

    return TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
  }

  // document ok, now import it
  TRI_doc_mptr_copy_t document;
  int res = TRI_ERROR_NO_ERROR;

  if (isEdgeCollection) {
    char const* from = extractJsonStringValue(json, TRI_VOC_ATTRIBUTE_FROM);
    char const* to   = extractJsonStringValue(json, TRI_VOC_ATTRIBUTE_TO);

    if (from == nullptr || to == nullptr) {
      errorMsg = positionise(i) + "missing '_from' or '_to' attribute";

      return TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE;
    }

    TRI_document_edge_t edge;

    edge._fromCid = 0;
    edge._toCid   = 0;
    edge._fromKey = nullptr;
    edge._toKey   = nullptr;

    // Note that in a DBserver in a cluster the following two calls will
    // parse the first part as a cluster-wide collection name:
    int res1 = parseDocumentId(trx.resolver(), from, edge._fromCid, edge._fromKey);
    int res2 = parseDocumentId(trx.resolver(), to, edge._toCid, edge._toKey);

    if (res1 == TRI_ERROR_NO_ERROR && res2 == TRI_ERROR_NO_ERROR) {
      res = trx.createEdge(&document, json, waitForSync, &edge);
    }
    else {
      res = (res1 != TRI_ERROR_NO_ERROR ? res1 : res2);
    }

    if (edge._fromKey != nullptr) {
      TRI_Free(TRI_CORE_MEM_ZONE, edge._fromKey);
    }
    if (edge._toKey != nullptr) {
      TRI_Free(TRI_CORE_MEM_ZONE, edge._toKey);
    }
  }
  else {
    // do not acquire an extra lock
    res = trx.createDocument(&document, json, waitForSync);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    string part = JsonHelper::toString(json);
    if (part.size() > 255) {
      // UTF-8 chars in string will be escaped so we can truncate it at any point
      part = part.substr(0, 255) + "...";
    }

    errorMsg = positionise(i) +
               "creating document failed with error '" + TRI_errno_string(res) +
               "', offending document: " + part;
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief imports documents from JSON
///
/// @RESTHEADER{POST /_api/import,imports documents from JSON}
///
/// @RESTBODYPARAM{documents,string,required}
/// The body must either be a JSON-encoded list of documents or a string with
/// multiple JSON documents separated by newlines.
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{type,string,required}
/// Determines how the body of the request will be interpreted. `type` can have
/// the following values:
/// - `documents`: when this type is used, each line in the request body is
///   expected to be an individual JSON-encoded document. Multiple JSON documents
///   in the request body need to be separated by newlines.
/// - `list`: when this type is used, the request body must contain a single
///   JSON-encoded list of individual documents to import.
/// - `auto`: if set, this will automatically determine the body type (either
///   `documents` or `list`).
///
/// @RESTQUERYPARAM{collection,string,required}
/// The collection name.
///
/// @RESTQUERYPARAM{createCollection,boolean,optional}
/// If this parameter has a value of `true` or `yes`, then the collection is
/// created if it does not yet exist. Other values will be ignored so the
/// collection must be present for the operation to succeed.
///
/// @RESTQUERYPARAM{overwrite,boolean,optional}
/// If this parameter has a value of `true` or `yes`, then all data in the
/// collection will be removed prior to the import. Note that any existing
/// index definitions will be preseved.
///
/// @RESTQUERYPARAM{waitForSync,boolean,optional}
/// Wait until documents have been synced to disk before returning.
///
/// @RESTQUERYPARAM{complete,boolean,optional}
/// If set to `true` or `yes`, it will make the whole import fail if any error
/// occurs. Otherwise the import will continue even if some documents cannot
/// be imported.
///
/// @RESTQUERYPARAM{details,boolean,optional}
/// If set to `true` or `yes`, the result will include an attribute `details`
/// with details about documents that could not be imported.
///
/// @RESTDESCRIPTION
/// Creates documents in the collection identified by `collection-name`.
/// The JSON representations of the documents must be passed as the body of the
/// POST request. The request body can either consist of multiple lines, with
/// each line being a single stand-alone JSON document, or a JSON list.
///
/// The response is a JSON object with the following attributes:
///
/// - `created`: number of documents imported.
///
/// - `errors`: number of documents that were not imported due to an error.
///
/// - `empty`: number of empty lines found in the input (will only contain a
///   value greater zero for types `documents` or `auto`).
///
/// - `details`: if URL parameter `details` is set to true, the result will
///   contain a `details` attribute which is a list with more detailed
///   information about which documents could not be inserted.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if all documents could be imported successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if `type` contains an invalid value, no `collection` is
/// specified, the documents are incorrectly encoded, or the request
/// is malformed.
///
/// @RESTRETURNCODE{404}
/// is returned if `collection` or the `_from` or `_to` attributes of an
/// imported edge refer to an unknown collection.
///
/// @RESTRETURNCODE{409}
/// is returned if the import would trigger a unique key violation and
/// `complete` is set to `true`.
///
/// @RESTRETURNCODE{500}
/// is returned if the server cannot auto-generate a document key (out of keys
/// error) for a document with no user-defined key.
///
/// @EXAMPLES
///
/// Importing documents with heterogenous attributes from a JSON list:
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportJsonList}
///     db._flushCache();
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///     db._flushCache();
///
///     var body = [
///       { _key: "abc", value1: 25, value2: "test", allowed: true },
///       { _key: "foo", name: "baz" },
///       { name: { detailed: "detailed name", short: "short name" } }
///     ];
///
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn + "&type=list", JSON.stringify(body));
///
///     assert(response.code === 201);
///     var r = JSON.parse(response.body);
///     assert(r.created === 3);
///     assert(r.errors === 0);
///     assert(r.empty === 0);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Importing documents from individual JSON lines:
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportJsonLines}
///     db._flushCache();
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///     db._flushCache();
///
///     var body = '{ "_key": "abc", "value1": 25, "value2": "test", "allowed": true }\n{ "_key": "foo", "name": "baz" }\n\n{ "name": { "detailed": "detailed name", "short": "short name" } }\n';
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn + "&type=documents", body);
///
///     assert(response.code === 201);
///     var r = JSON.parse(response.body);
///     assert(r.created === 3);
///     assert(r.errors === 0);
///     assert(r.empty === 1);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Using the auto type detection:
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportJsonType}
///     db._flushCache();
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///     db._flushCache();
///
///     var body = [
///       { _key: "abc", value1: 25, value2: "test", allowed: true },
///       { _key: "foo", name: "baz" },
///       { name: { detailed: "detailed name", short: "short name" } }
///     ];
///
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn + "&type=auto", JSON.stringify(body));
///
///     assert(response.code === 201);
///     var r = JSON.parse(response.body);
///     assert(r.created === 3);
///     assert(r.errors === 0);
///     assert(r.empty === 0);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Importing documents into a new collection from a JSON list:
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportJsonCreate}
///     db._flushCache();
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///     db._flushCache();
///
///     var body = [
///       { id: "12553", active: true },
///       { id: "4433", active: false },
///       { id: "55932", count: 4334 },
///     ];
///
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn + "&createCollection=true&type=list", JSON.stringify(body));
///
///     assert(response.code === 201);
///     var r = JSON.parse(response.body);
///     assert(r.created === 3);
///     assert(r.errors === 0);
///     assert(r.empty === 0);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Importing into an edge collection, with attributes `_from`, `_to` and `name`:
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportJsonEdge}
///     db._flushCache();
///     var cn = "links";
///     db._drop(cn);
///     db._createEdgeCollection(cn);
///     db._drop("products");
///     db._create("products");
///     db._flushCache();
///
///     var body = '{ "_from": "products/123", "_to": "products/234" }\n{ "_from": "products/332", "_to": "products/abc", "name": "other name" }';
///
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn + "&type=documents", body);
///
///     assert(response.code === 201);
///     var r = JSON.parse(response.body);
///     assert(r.created === 2);
///     assert(r.errors === 0);
///     assert(r.empty === 0);
///
///     logJsonResponse(response);
///     db._drop(cn);
///     db._drop("products");
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Importing into an edge collection, omitting `_from` or `_to`:
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportJsonEdgeInvalid}
///     db._flushCache();
///     var cn = "links";
///     db._drop(cn);
///     db._createEdgeCollection(cn);
///     db._flushCache();
///
///     var body = [ { name: "some name" } ];
///
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn + "&type=list&details=true", JSON.stringify(body));
///
///     assert(response.code === 201);
///     var r = JSON.parse(response.body);
///     assert(r.created === 0);
///     assert(r.errors === 1);
///     assert(r.empty === 0);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Violating a unique constraint, but allow partial imports:
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportJsonUniqueContinue}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///     db._flushCache();
///
///     var body = '{ "_key": "abc", "value1": 25, "value2": "test" }\n{ "_key": "abc", "value1": "bar", "value2": "baz" }';
///
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn + "&type=documents&details=true", body);
///
///     assert(response.code === 201);
///     var r = JSON.parse(response.body);
///     assert(r.created === 1);
///     assert(r.errors === 1);
///     assert(r.empty === 0);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Violating a unique constraint, not allowing partial imports:
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportJsonUniqueFail}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///     db._flushCache();
///
///     var body = '{ "_key": "abc", "value1": 25, "value2": "test" }\n{ "_key": "abc", "value1": "bar", "value2": "baz" }';
///
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn + "&type=documents&complete=true", body);
///
///     assert(response.code === 409);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Using a non-existing collection:
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportJsonInvalidCollection}
///     var cn = "products";
///     db._drop(cn);
///
///     var body = '{ "name": "test" }';
///
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn + "&type=documents", body);
///
///     assert(response.code === 404);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Using a malformed body:
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportJsonInvalidBody}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///     db._flushCache();
///
///     var body = '{ }';
///
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn + "&type=list", body);
///
///     assert(response.code === 400);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

bool RestImportHandler::createFromJson (string const& type) {
  RestImportResult result;

  vector<string> const& suffix = _request->suffix();

  if (suffix.size() != 0) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "superfluous suffix, expecting " + DOCUMENT_IMPORT_PATH + "?collection=<identifier>");
    return false;
  }

  bool const waitForSync = extractWaitForSync();
  bool const complete = extractComplete();
  bool const overwrite = extractOverwrite();

  // extract the collection name
  bool found;
  string const& collection = _request->value("collection", found);

  if (! found || collection.empty()) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,
                  "'collection' is missing, expecting " + DOCUMENT_IMPORT_PATH + "?collection=<identifier>");
    return false;
  }

  if (! checkCreateCollection(collection, TRI_COL_TYPE_DOCUMENT)) {
    return false;
  }

  bool linewise;

  if (type == "documents") {
    // linewise import
    linewise = true;
  }
  else if (type == "array" || type == "list") {
    // non-linewise import
    linewise = false;
  }
  else if (type == "auto") {
    linewise = true;

    // auto detect import type by peeking at first character
    char const* ptr = _request->body();
    char const* end = ptr + _request->bodySize();

    while (ptr < end) {
      char const c = *ptr;
      if (c == '\r' || c == '\n' || c == '\t' || c == ' ') {
        ptr++;
        continue;
      }
      else if (c == '[') {
        linewise = false;
      }

      break;
    }
  }
  else {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_BAD_PARAMETER,
                  "invalid value for 'type'");
    return false;
  }

  // find and load collection given by name or identifier
  RestImportTransaction trx(new StandaloneTransactionContext(), _vocbase, collection);

  // .............................................................................
  // inside write transaction
  // .............................................................................

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
    return false;
  }

  TRI_document_collection_t* document = trx.documentCollection();
  bool const isEdgeCollection = (document->_info._type == TRI_COL_TYPE_EDGE);

  trx.lockWrite();

  if (overwrite) {
    // truncate collection first
    trx.truncate(false);
  }

  if (linewise) {
    // each line is a separate JSON document
    char const* ptr = _request->body();
    char const* end = ptr + _request->bodySize();
    size_t i = 0;
    string errorMsg;

    while (ptr < end) {
      // read line until done
      i++;
        
      TRI_ASSERT(ptr != nullptr);

      // trim whitespace at start of line
      while (ptr < end && 
             (*ptr == ' ' || *ptr == '\t' || *ptr == '\r')) {
        ++ptr;
      }

      if (ptr == end || *ptr == '\0') {
        break;
      }

      // now find end of line
      char const* pos = strchr(ptr, '\n');

      TRI_json_t* json = nullptr;

      if (pos == ptr) {
        // line starting with \n, i.e. empty line
        ptr = pos + 1;
        ++result._numEmpty;
        continue;
      }
      else if (pos != nullptr) {
        // non-empty line
        *(const_cast<char*>(pos)) = '\0';
        TRI_ASSERT(ptr != nullptr);
        json = parseJsonLine(ptr);
        ptr = pos + 1;
      }
      else {
        // last-line, non-empty
        TRI_ASSERT(pos == nullptr);
        TRI_ASSERT(ptr != nullptr);
        json = parseJsonLine(ptr);
        ptr = end;
      }

      res = handleSingleDocument(trx, json, errorMsg, isEdgeCollection, waitForSync, i);

      if (json != nullptr) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      }

      if (res == TRI_ERROR_NO_ERROR) {
        ++result._numCreated;
      }
      else {
        registerError(result, errorMsg);

        if (complete) {
          // only perform a full import: abort
          break;
        }
        // perform partial import: continue
        res = TRI_ERROR_NO_ERROR;
      }
    }
  }

  else {
    // the entire request body is one JSON document
    TRI_json_t* documents = TRI_Json2String(TRI_UNKNOWN_MEM_ZONE, _request->body(), nullptr);

    if (! TRI_IsListJson(documents)) {
      if (documents != nullptr) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, documents);
      }

      generateError(HttpResponse::BAD,
                    TRI_ERROR_HTTP_BAD_PARAMETER,
                    "expecting a JSON list in the request");
      return false;
    }

    string errorMsg;
    size_t const n = documents->_value._objects._length;

    for (size_t i = 0; i < n; ++i) {
      TRI_json_t const* json = static_cast<TRI_json_t const*>(TRI_AtVector(&documents->_value._objects, i));

      res = handleSingleDocument(trx, json, errorMsg, isEdgeCollection, waitForSync, i + 1);

      if (res == TRI_ERROR_NO_ERROR) {
        ++result._numCreated;
      }
      else {
        registerError(result, errorMsg);

        if (complete) {
          // only perform a full import: abort
          break;
        }
        // perform partial import: continue
        res = TRI_ERROR_NO_ERROR;
      }
    }

    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, documents);
  }


  // this may commit, even if previous errors occurred
  res = trx.finish(res);

  // .............................................................................
  // outside write transaction
  // .............................................................................

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
  }
  else {
    // generate result
    generateDocumentsCreated(result);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief imports documents from JSON-encoded key-value lists
///
/// @RESTHEADER{POST /_api/import,imports document values}
///
/// @RESTBODYPARAM{documents,string,required}
/// The body must consist of JSON-encoded lists of attribute values, with one
/// line per per document. The first row of the request must be a JSON-encoded
/// list of attribute names. These attribute names are used for the data in the
/// subsequent rows.
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{collection,string,required}
/// The collection name.
///
/// @RESTQUERYPARAM{createCollection,boolean,optional}
/// If this parameter has a value of `true` or `yes`, then the collection is
/// created if it does not yet exist. Other values will be ignored so the
/// collection must be present for the operation to succeed.
///
/// @RESTQUERYPARAM{overwrite,boolean,optional}
/// If this parameter has a value of `true` or `yes`, then all data in the
/// collection will be removed prior to the import. Note that any existing
/// index definitions will be preseved.
///
/// @RESTQUERYPARAM{waitForSync,boolean,optional}
/// Wait until documents have been synced to disk before returning.
///
/// @RESTQUERYPARAM{complete,boolean,optional}
/// If set to `true` or `yes`, it will make the whole import fail if any error
/// occurs. Otherwise the import will continue even if some documents cannot
/// be imported.
///
/// @RESTQUERYPARAM{details,boolean,optional}
/// If set to `true` or `yes`, the result will include an attribute `details`
/// with details about documents that could not be imported.
///
/// @RESTDESCRIPTION
/// Creates documents in the collection identified by `collection-name`.
/// The first line of the request body must contain a JSON-encoded list of
/// attribute names. All following lines in the request body must contain
/// JSON-encoded lists of attribute values. Each line is interpreted as a
/// separate document, and the values specified will be mapped to the list
/// of attribute names specified in the first header line.
///
/// The response is a JSON object with the following attributes:
///
/// - `created`: number of documents imported.
///
/// - `errors`: number of documents that were not imported due to an error.
///
/// - `empty`: number of empty lines found in the input (will only contain a
///   value greater zero for types `documents` or `auto`).
///
/// - `details`: if URL parameter `details` is set to true, the result will
///   contain a `details` attribute which is a list with more detailed
///   information about which documents could not be inserted.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if all documents could be imported successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if `type` contains an invalid value, no `collection` is
/// specified, the documents are incorrectly encoded, or the request
/// is malformed.
///
/// @RESTRETURNCODE{404}
/// is returned if `collection` or the `_from` or `_to` attributes of an
/// imported edge refer to an unknown collection.
///
/// @RESTRETURNCODE{409}
/// is returned if the import would trigger a unique key violation and
/// `complete` is set to `true`.
///
/// @RESTRETURNCODE{500}
/// is returned if the server cannot auto-generate a document key (out of keys
/// error) for a document with no user-defined key.
///
/// @EXAMPLES
///
/// Importing two documents, with attributes `_key`, `value1` and `value2` each. One
/// line in the import data is empty:
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportCsvExample}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     var body = '[ "_key", "value1", "value2" ]\n[ "abc", 25, "test" ]\n\n[ "foo", "bar", "baz" ]';
///
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn, body);
///
///     assert(response.code === 201);
///     var r = JSON.parse(response.body)
///     assert(r.created === 2);
///     assert(r.errors === 0);
///     assert(r.empty === 1);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Importing two documents into a new collection:
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportCsvCreate}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     var body = '[ "value1", "value2" ]\n[ 1234, null ]\n[ "foo", "bar" ]\n[ 534.55, true ]';
///
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn + "&createCollection=true", body);
///
///     assert(response.code === 201);
///     var r = JSON.parse(response.body)
///     assert(r.created === 3);
///     assert(r.errors === 0);
///     assert(r.empty === 0);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Importing into an edge collection, with attributes `_from`, `_to` and `name`:
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportCsvEdge}
///     var cn = "links";
///     db._drop(cn);
///     db._createEdgeCollection(cn);
///     db._drop("products");
///     db._create("products");
///
///     var body = '[ "_from", "_to", "name" ]\n[ "products/123", "products/234", "some name" ]\n[ "products/332", "products/abc", "other name" ]';
///
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn, body);
///
///     assert(response.code === 201);
///     var r = JSON.parse(response.body)
///     assert(r.created === 2);
///     assert(r.errors === 0);
///     assert(r.empty === 0);
///
///     logJsonResponse(response);
///     db._drop(cn);
///     db._drop("products");
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Importing into an edge collection, omitting `_from` or `_to`:
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportCsvEdgeInvalid}
///     var cn = "links";
///     db._drop(cn);
///     db._createEdgeCollection(cn);
///
///     var body = '[ "name" ]\n[ "some name" ]\n[ "other name" ]';
///
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn + "&details=true", body);
///
///     assert(response.code === 201);
///     var r = JSON.parse(response.body)
///     assert(r.created === 0);
///     assert(r.errors === 2);
///     assert(r.empty === 0);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Violating a unique constraint, but allow partial imports:
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportCsvUniqueContinue}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     var body = '[ "_key", "value1", "value2" ]\n[ "abc", 25, "test" ]\n[ "abc", "bar", "baz" ]';
///
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn + "&details=true", body);
///
///     assert(response.code === 201);
///     var r = JSON.parse(response.body)
///     assert(r.created === 1);
///     assert(r.errors === 1);
///     assert(r.empty === 0);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Violating a unique constraint, not allowing partial imports:
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportCsvUniqueFail}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     var body = '[ "_key", "value1", "value2" ]\n[ "abc", 25, "test" ]\n[ "abc", "bar", "baz" ]';
///
///     var response = logCurlRequest('POST', "/_api/import?collection=" + cn + "&complete=true", body);
///
///     assert(response.code === 409);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Using a non-existing collection:
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportCsvInvalidCollection}
///     var cn = "products";
///     db._drop(cn);
///
///     var body = '[ "_key", "value1", "value2" ]\n[ "abc", 25, "test" ]\n[ "foo", "bar", "baz" ]';
///
///     var response = logCurlRequest('POST', "/_api/import?collection=" + cn, body);
///
///     assert(response.code === 404);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Using a malformed body:
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportCsvInvalidBody}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     var body = '{ "_key": "foo", "value1": "bar" }';
///
///     var response = logCurlRequest('POST', "/_api/import?collection=" + cn, body);
///
///     assert(response.code === 400);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

bool RestImportHandler::createFromKeyValueList () {
  RestImportResult result;

  vector<string> const& suffix = _request->suffix();

  if (suffix.size() != 0) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "superfluous suffix, expecting " + DOCUMENT_IMPORT_PATH + "?collection=<identifier>");
    return false;
  }

  bool const waitForSync = extractWaitForSync();
  bool const complete = extractComplete();
  bool const overwrite = extractOverwrite();

  // extract the collection name
  bool found;
  string const& collection = _request->value("collection", found);

  if (! found || collection.empty()) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,
                  "'collection' is missing, expecting " + DOCUMENT_IMPORT_PATH + "?collection=<identifier>");
    return false;
  }

  if (! checkCreateCollection(collection, TRI_COL_TYPE_DOCUMENT)) {
    return false;
  }

  // read line number (optional)
  int64_t lineNumber = 0;
  string const& lineNumValue = _request->value("line", found);
  if (found) {
    lineNumber = StringUtils::int64(lineNumValue);
  }

  char const* current = _request->body();
  char const* bodyEnd = current + _request->bodySize();

  // process header
  char const* next = strchr(current, '\n');

  if (next == nullptr) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "no JSON list found in second line");
    return false;
  }
  
  char const* lineStart = current;
  char const* lineEnd   = next;
    
  // trim line
  while (lineStart < bodyEnd &&
         (*lineStart == ' ' || *lineStart == '\t' || *lineStart == '\r' || *lineStart == '\n')) {
    ++lineStart;
  }
    
  while (lineEnd > lineStart &&
         (*(lineEnd - 1) == ' ' || *(lineEnd - 1) == '\t' || *(lineEnd - 1) == '\r' || *(lineEnd - 1) == '\n')) {
    --lineEnd;
  }

  *(const_cast<char*>(lineEnd)) = '\0';
  TRI_json_t* keys = parseJsonLine(lineStart, lineEnd);

  if (! checkKeys(keys)) {
    LOG_WARNING("no JSON string list in first line found");
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "no JSON string list in first line found");

    if (keys != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, keys);
    }
    return false;
  }

  current = next + 1;


  // find and load collection given by name or identifier
  RestImportTransaction trx(new StandaloneTransactionContext(), _vocbase, collection);

  // .............................................................................
  // inside write transaction
  // .............................................................................

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, keys);
    generateTransactionError(collection, res);
    return false;
  }

  TRI_document_collection_t* document = trx.documentCollection();
  bool const isEdgeCollection = (document->_info._type == TRI_COL_TYPE_EDGE);

  trx.lockWrite();

  if (overwrite) {
    // truncate collection first
    trx.truncate(false);
  }

  size_t i = (size_t) lineNumber;

  while (current != nullptr && current < bodyEnd) {
    i++;

    next = static_cast<char const*>(memchr(current, '\n', bodyEnd - current));

    char const* lineStart = current;
    char const* lineEnd   = next;

    if (next == nullptr) {
      // reached the end
      lineEnd = bodyEnd;
      current = nullptr;
    }
    else {
      // got more to read
      current = next + 1;
      *(const_cast<char*>(lineEnd)) = '\0';
    }

    // trim line
    while (lineStart < bodyEnd &&
           (*lineStart == ' ' || *lineStart == '\t' || *lineStart == '\r' || *lineStart == '\n')) {
      ++lineStart;
    }
    
    while (lineEnd > lineStart &&
           (*(lineEnd - 1) == ' ' || *(lineEnd - 1) == '\t' || *(lineEnd - 1) == '\r' || *(lineEnd - 1) == '\n')) {
      --lineEnd;
    }

    if (lineStart == lineEnd) {
      ++result._numEmpty;
      continue;
    }

    TRI_json_t* values = parseJsonLine(lineStart, lineEnd);

    if (values != nullptr) {
      // build the json object from the list
      string errorMsg;

      TRI_json_t* json = createJsonObject(keys, values, errorMsg, i);
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, values);

      if (json != nullptr) {
        res = handleSingleDocument(trx, json, errorMsg, isEdgeCollection, waitForSync, i);
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      }
      else {
        // raise any error
        res = TRI_ERROR_INTERNAL;
      }

      if (res == TRI_ERROR_NO_ERROR) {
        ++result._numCreated;
      }
      else {
        registerError(result, errorMsg);

        if (complete) {
          // only perform a full import: abort
          break;
        }

        // perform partial import: continue
        res = TRI_ERROR_NO_ERROR;
      }
    }
    else {
      string errorMsg = positionise(i) + "no valid JSON data";
      registerError(result, errorMsg);
    }
  }

  // we'll always commit, even if previous errors occurred
  res = trx.finish(res);

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, keys);

  // .............................................................................
  // outside write transaction
  // .............................................................................

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
  }
  else {
    // generate result
    generateDocumentsCreated(result);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create response for number of documents created / failed
////////////////////////////////////////////////////////////////////////////////

void RestImportHandler::generateDocumentsCreated (RestImportResult const& result) {
  _response = createResponse(HttpResponse::CREATED);
  _response->setContentType("application/json; charset=utf-8");

  TRI_json_t json;

  TRI_InitArrayJson(TRI_CORE_MEM_ZONE, &json);
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &json, "error", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, false));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &json, "created", TRI_CreateNumberJson(TRI_CORE_MEM_ZONE, (double) result._numCreated));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &json, "errors", TRI_CreateNumberJson(TRI_CORE_MEM_ZONE, (double) result._numErrors));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &json, "empty", TRI_CreateNumberJson(TRI_CORE_MEM_ZONE, (double) result._numEmpty));

  bool found;
  char const* detailsStr = _request->value("details", found);

  // include failure details?
  if (found && StringUtils::boolean(detailsStr)) {
    TRI_json_t* messages = TRI_CreateListJson(TRI_CORE_MEM_ZONE);

    for (size_t i = 0, n = result._errors.size(); i < n; ++i) {
      string const& msg = result._errors[i];
      TRI_PushBack3ListJson(TRI_CORE_MEM_ZONE, messages, TRI_CreateString2CopyJson(TRI_CORE_MEM_ZONE, msg.c_str(), msg.size()));
    }
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &json, "details", messages);
  }

  generateResult(HttpResponse::CREATED, &json);
  TRI_DestroyJson(TRI_CORE_MEM_ZONE, &json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse a single document line
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* RestImportHandler::parseJsonLine (string const& line) {
  return parseJsonLine(line.c_str(), line.c_str() + line.size());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse a single document line
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* RestImportHandler::parseJsonLine (char const* start,
                                              char const* end) {
  char* errmsg = nullptr;
  TRI_json_t* json = TRI_Json2String(TRI_UNKNOWN_MEM_ZONE, start, &errmsg);

  if (errmsg != nullptr) {
    // must free this error message, otherwise we'll have a memleak
    TRI_FreeString(TRI_CORE_MEM_ZONE, errmsg);
  }
  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a JSON object from a line containing a document
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* RestImportHandler::createJsonObject (TRI_json_t const* keys,
                                                 TRI_json_t const* values,
                                                 string& errorMsg,
                                                 size_t lineNumber) {

  if (values->_type != TRI_JSON_LIST) {
    errorMsg = positionise(lineNumber) + "no valid JSON list data";
    return nullptr;
  }

  size_t const n = keys->_value._objects._length;

  if (n !=  values->_value._objects._length) {
    errorMsg = positionise(lineNumber) + "wrong number of JSON values";
    return nullptr;
  }

  TRI_json_t* result = TRI_CreateArray2Json(TRI_UNKNOWN_MEM_ZONE, n);

  if (result == nullptr) {
    LOG_ERROR("out of memory");
    return nullptr;
  }

  for (size_t i = 0;  i < n;  ++i) {

    TRI_json_t const* key   = static_cast<TRI_json_t const*>(TRI_AtVector(&keys->_value._objects, i));
    TRI_json_t const* value = static_cast<TRI_json_t const*>(TRI_AtVector(&values->_value._objects, i));

    if (JsonHelper::isString(key) && value->_type > TRI_JSON_NULL) {
      TRI_InsertArrayJson(TRI_UNKNOWN_MEM_ZONE, result, key->_value._string.data, value);
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate keys
////////////////////////////////////////////////////////////////////////////////

bool RestImportHandler::checkKeys (TRI_json_t const* keys) const {
  if (! TRI_IsListJson(keys)) {
    return false;
  }

  size_t const n = keys->_value._objects._length;

  if (n == 0) {
    return false;
  }

  for (size_t i = 0;  i < n;  ++i) {
    TRI_json_t const* key = static_cast<TRI_json_t* const>(TRI_AtVector(&keys->_value._objects, i));

    if (! JsonHelper::isString(key)) {
      return false;
    }
  }

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
