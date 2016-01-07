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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "RestImportHandler.h"

#include "Basics/json-utilities.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Rest/HttpRequest.h"
#include "VocBase/document-collection.h"
#include "VocBase/edge-collection.h"
#include "VocBase/vocbase.h"

#include <velocypack/Dumper.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;


////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestImportHandler::RestImportHandler(HttpRequest* request)
    : RestVocbaseBaseHandler(request), _onDuplicateAction(DUPLICATE_ERROR) {}


////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_t RestImportHandler::execute() {
  if (ServerState::instance()->isCoordinator()) {
    generateError(HttpResponse::NOT_IMPLEMENTED, TRI_ERROR_CLUSTER_UNSUPPORTED,
                  "'/_api/import' is not yet supported in a cluster");
    return status_t(HANDLER_DONE);
  }

  // set default value for onDuplicate
  _onDuplicateAction = DUPLICATE_ERROR;

  bool found;

  string const duplicateType = _request->value("onDuplicate", found);

  if (found) {
    if (duplicateType == "update") {
      _onDuplicateAction = DUPLICATE_UPDATE;
    } else if (duplicateType == "replace") {
      _onDuplicateAction = DUPLICATE_REPLACE;
    } else if (duplicateType == "ignore") {
      _onDuplicateAction = DUPLICATE_IGNORE;
    }
  }

  // extract the sub-request type
  HttpRequest::HttpRequestType type = _request->requestType();

  switch (type) {
    case HttpRequest::HTTP_REQUEST_POST: {
      // extract the import type
      string const documentType = _request->value("type", found);

      if (found && (documentType == "documents" || documentType == "array" ||
                    documentType == "list" || documentType == "auto")) {
        createFromJson(documentType);
      } else {
        // CSV
        createFromKeyValueList();
      }
      break;
    }

    default:
      generateNotImplemented("ILLEGAL " + IMPORT_PATH);
      break;
  }

  // this handler is done
  return status_t(HANDLER_DONE);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief determine the collection type from the request
////////////////////////////////////////////////////////////////////////////////

TRI_col_type_e RestImportHandler::getCollectionType() {
  // extract the collection type from the request
  bool found;
  std::string const& collectionType =
      _request->value("createCollectionType", found);

  if (found && !collectionType.empty() && collectionType == "edge") {
    return TRI_COL_TYPE_EDGE;
  }

  return TRI_COL_TYPE_DOCUMENT;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the "overwrite" value
////////////////////////////////////////////////////////////////////////////////

bool RestImportHandler::extractOverwrite() const {
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

bool RestImportHandler::extractComplete() const {
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

std::string RestImportHandler::positionise(size_t i) const {
  return string("at position " + StringUtils::itoa(i) + ": ");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register an error
////////////////////////////////////////////////////////////////////////////////

void RestImportHandler::registerError(RestImportResult& result,
                                      std::string const& errorMsg) {
  ++result._numErrors;

  result._errors.push_back(errorMsg);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief construct an error message
////////////////////////////////////////////////////////////////////////////////

std::string RestImportHandler::buildParseError(size_t i,
                                               char const* lineStart) {
  if (lineStart != nullptr) {
    string part(lineStart);
    if (part.size() > 255) {
      // UTF-8 chars in string will be escaped so we can truncate it at any
      // point
      part = part.substr(0, 255) + "...";
    }

    return positionise(i) +
           "invalid JSON type (expecting object, probably parse error), "
           "offending context: " +
           part;
  }

  return positionise(i) +
         "invalid JSON type (expecting object, probably parse error)";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process a single VelocyPack document
////////////////////////////////////////////////////////////////////////////////

int RestImportHandler::handleSingleDocument(RestImportTransaction& trx,
                                            RestImportResult& result,
                                            char const* lineStart,
                                            VPackSlice const& slice,
                                            bool isEdgeCollection,
                                            bool waitForSync, size_t i) {
  if (!slice.isObject()) {
    string part = VPackDumper::toString(slice);
    if (part.size() > 255) {
      // UTF-8 chars in string will be escaped so we can truncate it at any
      // point
      part = part.substr(0, 255) + "...";
    }

    std::string errorMsg =
        positionise(i) +
        "invalid JSON type (expecting object), offending document: " + part;

    registerError(result, errorMsg);
    return TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
  }

  // document ok, now import it
  TRI_doc_mptr_copy_t document;
  int res = TRI_ERROR_NO_ERROR;

  if (isEdgeCollection) {
    std::string from;
    std::string to;

    try {
      from = triagens::basics::VelocyPackHelper::checkAndGetStringValue(
          slice, TRI_VOC_ATTRIBUTE_FROM);
      to = triagens::basics::VelocyPackHelper::checkAndGetStringValue(
          slice, TRI_VOC_ATTRIBUTE_TO);
    } catch (triagens::basics::Exception const&) {
      string part = VPackDumper::toString(slice);
      if (part.size() > 255) {
        // UTF-8 chars in string will be escaped so we can truncate it at any
        // point
        part = part.substr(0, 255) + "...";
      }

      std::string errorMsg =
          positionise(i) +
          "missing '_from' or '_to' attribute, offending document: " + part;

      registerError(result, errorMsg);
      return TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE;
    }

    TRI_document_edge_t edge;

    edge._fromCid = 0;
    edge._toCid = 0;
    edge._fromKey = nullptr;
    edge._toKey = nullptr;

    // Note that in a DBserver in a cluster the following two calls will
    // parse the first part as a cluster-wide collection name:
    int res1 =
        parseDocumentId(trx.resolver(), from, edge._fromCid, edge._fromKey);
    int res2 = parseDocumentId(trx.resolver(), to, edge._toCid, edge._toKey);

    if (res1 == TRI_ERROR_NO_ERROR && res2 == TRI_ERROR_NO_ERROR) {
      res = trx.createEdge(&document, slice, waitForSync, &edge);
    } else {
      res = (res1 != TRI_ERROR_NO_ERROR ? res1 : res2);
    }

    if (edge._fromKey != nullptr) {
      TRI_Free(TRI_CORE_MEM_ZONE, edge._fromKey);
    }
    if (edge._toKey != nullptr) {
      TRI_Free(TRI_CORE_MEM_ZONE, edge._toKey);
    }
  } else {
    // do not acquire an extra lock
    res = trx.createDocument(&document, slice, waitForSync);
  }

  if (res == TRI_ERROR_NO_ERROR) {
    ++result._numCreated;
  }

  // special behavior in case of unique constraint violation . . .
  if (res == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED &&
      _onDuplicateAction != DUPLICATE_ERROR) {
    VPackSlice const keySlice = slice.get(TRI_VOC_ATTRIBUTE_KEY);

    if (keySlice.isString()) {
      // insert failed. now try an update/replace

      std::string keyString = keySlice.copyString();
      if (_onDuplicateAction == DUPLICATE_UPDATE) {
        // update: first read existing document
        TRI_doc_mptr_copy_t previous;
        int res2 = trx.read(&previous, keyString);

        if (res2 == TRI_ERROR_NO_ERROR) {
          auto shaper =
              trx.documentCollection()->getShaper();  // PROTECTED by trx here

          TRI_shaped_json_t shapedJson;
          TRI_EXTRACT_SHAPED_JSON_MARKER(
              shapedJson, previous.getDataPtr());  // PROTECTED by trx here
          std::unique_ptr<TRI_json_t> old(
              TRI_JsonShapedJson(shaper, &shapedJson));

          // default value
          res = TRI_ERROR_OUT_OF_MEMORY;

          if (old != nullptr) {
            std::unique_ptr<TRI_json_t> json(
                triagens::basics::VelocyPackHelper::velocyPackToJson(slice));
            std::unique_ptr<TRI_json_t> patchedJson(TRI_MergeJson(
                TRI_UNKNOWN_MEM_ZONE, old.get(), json.get(), false, true));

            if (patchedJson != nullptr) {
              res = trx.updateDocument(keyString, &document, patchedJson.get(),
                                       TRI_DOC_UPDATE_LAST_WRITE, waitForSync,
                                       0, nullptr);
            }
          }

          if (res == TRI_ERROR_NO_ERROR) {
            ++result._numUpdated;
          }
        }
      } else if (_onDuplicateAction == DUPLICATE_REPLACE) {
        // replace
        res = trx.updateDocument(keyString, &document, slice,
                                 TRI_DOC_UPDATE_LAST_WRITE, waitForSync, 0,
                                 nullptr);

        if (res == TRI_ERROR_NO_ERROR) {
          ++result._numUpdated;
        }
      } else {
        // simply ignore unique key violations silently
        TRI_ASSERT(_onDuplicateAction == DUPLICATE_IGNORE);
        res = TRI_ERROR_NO_ERROR;
        ++result._numIgnored;
      }
    }
  }

  if (res != TRI_ERROR_NO_ERROR) {
    string part = VPackDumper::toString(slice);
    if (part.size() > 255) {
      // UTF-8 chars in string will be escaped so we can truncate it at any
      // point
      part = part.substr(0, 255) + "...";
    }

    std::string errorMsg =
        positionise(i) + "creating document failed with error '" +
        TRI_errno_string(res) + "', offending document: " + part;

    registerError(result, errorMsg);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_import_json
/// @brief imports documents from JSON
///
/// @RESTHEADER{POST /_api/import#json,imports documents from JSON}
///
/// @RESTALLBODYPARAM{documents,string,required}
/// The body must either be a JSON-encoded array of objects or a string with
/// multiple JSON objects separated by newlines.
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{type,string,required}
/// Determines how the body of the request will be interpreted. `type` can have
/// the following values:
/// - `documents`: when this type is used, each line in the request body is
///   expected to be an individual JSON-encoded document. Multiple JSON objects
///   in the request body need to be separated by newlines.
/// - `list`: when this type is used, the request body must contain a single
///   JSON-encoded array of individual objects to import.
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
/// @RESTQUERYPARAM{createCollectionType,string,optional}
/// If this parameter has a value of `document` or `edge`, it will determine
/// the type of collection that is going to be created when the
/// `createCollection`
/// option is set to `true`. The default value is `document`.
///
/// @RESTQUERYPARAM{overwrite,boolean,optional}
/// If this parameter has a value of `true` or `yes`, then all data in the
/// collection will be removed prior to the import. Note that any existing
/// index definitions will be preseved.
///
/// @RESTQUERYPARAM{waitForSync,boolean,optional}
/// Wait until documents have been synced to disk before returning.
///
/// @RESTQUERYPARAM{onDuplicate,string,optional}
/// Controls what action is carried out in case of a unique key constraint
/// violation. Possible values are:
///
/// - *error*: this will not import the current document because of the unique
///   key constraint violation. This is the default setting.
///
/// - *update*: this will update an existing document in the database with the
///   data specified in the request. Attributes of the existing document that
///   are not present in the request will be preseved.
///
/// - *replace*: this will replace an existing document in the database with the
///   data specified in the request.
///
/// - *ignore*: this will not update an existing document and simply ignore the
///   error caused by a unique key constraint violation.
///
/// Note that that *update*, *replace* and *ignore* will only work when the
/// import document in the request contains the *_key* attribute. *update* and
/// *replace* may also fail because of secondary unique key constraint
/// violations.
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
/// **NOTE** Swagger examples won't work due to the anchor.
///
///
/// Creates documents in the collection identified by `collection-name`.
/// The JSON representations of the documents must be passed as the body of the
/// POST request. The request body can either consist of multiple lines, with
/// each line being a single stand-alone JSON object, or a singe JSON array with
/// sub-objects.
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
/// - `updated`: number of updated/replaced documents (in case `onDuplicate`
///   was set to either `update` or `replace`).
///
/// - `ignored`: number of failed but ignored insert operations (in case
///   `onDuplicate` was set to `ignore`).
///
/// - `details`: if query parameter `details` is set to true, the result will
///   contain a `details` attribute which is an array with more detailed
///   information about which documents could not be inserted.
///
/// Note: this API is currently not supported on cluster coordinators.
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
/// @RESTRETURNCODE{501}
/// The server will respond with *HTTP 501* if this API is called on a cluster
/// coordinator.
///
/// @EXAMPLES
///
/// Importing documents with heterogenous attributes from a JSON array
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
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn
///     + "&type=list", body);
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
/// Importing documents from individual JSON lines
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportJsonLines}
///     db._flushCache();
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///     db._flushCache();
///
///     var body = '{ "_key": "abc", "value1": 25, "value2": "test",' +
///                '"allowed": true }\n' + 
///                '{ "_key": "foo", "name": "baz" }\n\n' + 
///                '{ "name": {' +
///                ' "detailed": "detailed name", "short": "short name" } }\n';
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn
///     + "&type=documents", body);
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
/// Using the auto type detection
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
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn
///     + "&type=auto", body);
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
/// Importing documents into a new collection from a JSON array
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
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn
///     + "&createCollection=true&type=list", body);
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
/// Importing into an edge collection, with attributes `_from`, `_to` and `name`
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
///     var body = '{ "_from": "products/123", "_to": "products/234" }\n' + 
///                '{"_from": "products/332", "_to": "products/abc", ' + 
///                '  "name": "other name" }';
///
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn
///     + "&type=documents", body);
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
/// Importing into an edge collection, omitting `_from` or `_to`
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
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn
///     + "&type=list&details=true", body);
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
/// Violating a unique constraint, but allow partial imports
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportJsonUniqueContinue}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///     db._flushCache();
///
///     var body = '{ "_key": "abc", "value1": 25, "value2": "test" }\n' + 
///                '{ "_key": "abc", "value1": "bar", "value2": "baz" }';
///
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn
///     + "&type=documents&details=true", body);
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
/// Violating a unique constraint, not allowing partial imports
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportJsonUniqueFail}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///     db._flushCache();
///
///     var body = '{ "_key": "abc", "value1": 25, "value2": "test" }\n' +
///                '{ "_key": "abc", "value1": "bar", "value2": "baz" }';
///
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn
///     + "&type=documents&complete=true", body);
///
///     assert(response.code === 409);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Using a non-existing collection
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportJsonInvalidCollection}
///     var cn = "products";
///     db._drop(cn);
///
///     var body = '{ "name": "test" }';
///
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn
///     + "&type=documents", body);
///
///     assert(response.code === 404);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Using a malformed body
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportJsonInvalidBody}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///     db._flushCache();
///
///     var body = '{ }';
///
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn
///     + "&type=list", body);
///
///     assert(response.code === 400);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

bool RestImportHandler::createFromJson(string const& type) {
  RestImportResult result;

  vector<string> const& suffix = _request->suffix();

  if (suffix.size() != 0) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "superfluous suffix, expecting " + IMPORT_PATH +
                      "?collection=<identifier>");
    return false;
  }

  bool const waitForSync = extractWaitForSync();
  bool const complete = extractComplete();
  bool const overwrite = extractOverwrite();

  // extract the collection name
  bool found;
  string const& collection = _request->value("collection", found);

  if (!found || collection.empty()) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,
                  "'collection' is missing, expecting " + IMPORT_PATH +
                      "?collection=<identifier>");
    return false;
  }

  if (!checkCreateCollection(collection, getCollectionType())) {
    return false;
  }

  bool linewise;

  if (type == "documents") {
    // linewise import
    linewise = true;
  } else if (type == "array" || type == "list") {
    // non-linewise import
    linewise = false;
  } else if (type == "auto") {
    linewise = true;

    // auto detect import type by peeking at first character
    char const* ptr = _request->body();
    char const* end = ptr + _request->bodySize();

    while (ptr < end) {
      char const c = *ptr;
      if (c == '\r' || c == '\n' || c == '\t' || c == '\b' || c == '\f' ||
          c == ' ') {
        ptr++;
        continue;
      } else if (c == '[') {
        linewise = false;
      }

      break;
    }
  } else {
    generateError(HttpResponse::BAD, TRI_ERROR_BAD_PARAMETER,
                  "invalid value for 'type'");
    return false;
  }

  // find and load collection given by name or identifier
  RestImportTransaction trx(new StandaloneTransactionContext(), _vocbase,
                            collection);

  // .............................................................................
  // inside write transaction
  // .............................................................................

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
    return false;
  }

  TRI_document_collection_t* document = trx.documentCollection();
  bool const isEdgeCollection = (document->_info.type() == TRI_COL_TYPE_EDGE);

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

    while (ptr < end) {
      // read line until done
      i++;

      TRI_ASSERT(ptr != nullptr);

      // trim whitespace at start of line
      while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\r' ||
                           *ptr == '\b' || *ptr == '\f')) {
        ++ptr;
      }

      if (ptr == end || *ptr == '\0') {
        break;
      }

      // now find end of line
      char const* pos = strchr(ptr, '\n');
      char const* oldPtr = nullptr;

      std::shared_ptr<VPackBuilder> builder;

      bool success = false;

      if (pos == ptr) {
        // line starting with \n, i.e. empty line
        ptr = pos + 1;
        ++result._numEmpty;
        continue;
      } else if (pos != nullptr) {
        // non-empty line
        *(const_cast<char*>(pos)) = '\0';
        TRI_ASSERT(ptr != nullptr);
        oldPtr = ptr;
        builder = parseVelocyPackLine(ptr, pos, success);
        ptr = pos + 1;
      } else {
        // last-line, non-empty
        TRI_ASSERT(pos == nullptr);
        TRI_ASSERT(ptr != nullptr);
        oldPtr = ptr;
        builder = parseVelocyPackLine(ptr, success);
        ptr = end;
      }

      if (!success) {
        std::string errorMsg = buildParseError(i, oldPtr);
        registerError(result, errorMsg);
        if (complete) {
          // only perform a full import: abort
          break;
        }
        // Do not try to store illegal document
        continue;
      }

      res = handleSingleDocument(trx, result, oldPtr, builder->slice(),
                                 isEdgeCollection, waitForSync, i);

      if (res != TRI_ERROR_NO_ERROR) {
        if (complete) {
          // only perform a full import: abort
          break;
        }

        res = TRI_ERROR_NO_ERROR;
      }
    }
  }

  else {
    // the entire request body is one JSON document
    std::shared_ptr<VPackBuilder> parsedDocuments;
    try {
      parsedDocuments = VPackParser::fromJson(
          reinterpret_cast<uint8_t const*>(_request->body()),
          _request->bodySize());
    } catch (VPackException const&) {
      generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "expecting a JSON array in the request");
      return false;
    }

    VPackSlice const documents = parsedDocuments->slice();

    if (!documents.isArray()) {
      generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "expecting a JSON array in the request");
      return false;
    }

    VPackValueLength const n = documents.length();

    for (VPackValueLength i = 0; i < n; ++i) {
      VPackSlice const slice = documents.at(i);

      res = handleSingleDocument(trx, result, nullptr, slice, isEdgeCollection,
                                 waitForSync, i + 1);

      if (res != TRI_ERROR_NO_ERROR) {
        if (complete) {
          // only perform a full import: abort
          break;
        }

        res = TRI_ERROR_NO_ERROR;
      }
    }
  }

  // this may commit, even if previous errors occurred
  res = trx.finish(res);

  // .............................................................................
  // outside write transaction
  // .............................................................................

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
  } else {
    // generate result
    generateDocumentsCreated(result);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_import_document
/// @brief imports documents from JSON-encoded lists
///
/// @RESTHEADER{POST /_api/import#document,imports document values}
///
/// @RESTALLBODYPARAM{documents,string,required}
/// The body must consist of JSON-encoded arrays of attribute values, with one
/// line per document. The first row of the request must be a JSON-encoded
/// array of attribute names. These attribute names are used for the data in the
/// subsequent lines.
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
/// @RESTQUERYPARAM{createCollectionType,string,optional}
/// If this parameter has a value of `document` or `edge`, it will determine
/// the type of collection that is going to be created when the
/// `createCollection`
/// option is set to `true`. The default value is `document`.
///
/// @RESTQUERYPARAM{overwrite,boolean,optional}
/// If this parameter has a value of `true` or `yes`, then all data in the
/// collection will be removed prior to the import. Note that any existing
/// index definitions will be preseved.
///
/// @RESTQUERYPARAM{waitForSync,boolean,optional}
/// Wait until documents have been synced to disk before returning.
///
/// @RESTQUERYPARAM{onDuplicate,string,optional}
/// Controls what action is carried out in case of a unique key constraint
/// violation. Possible values are:
///
/// - *error*: this will not import the current document because of the unique
///   key constraint violation. This is the default setting.
///
/// - *update*: this will update an existing document in the database with the
///   data specified in the request. Attributes of the existing document that
///   are not present in the request will be preseved.
///
/// - *replace*: this will replace an existing document in the database with the
///   data specified in the request.
///
/// - *ignore*: this will not update an existing document and simply ignore the
///   error caused by the unique key constraint violation.
///
/// Note that *update*, *replace* and *ignore* will only work when the
/// import document in the request contains the *_key* attribute. *update* and
/// *replace* may also fail because of secondary unique key constraint
/// violations.
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
/// **NOTE** Swagger examples won't work due to the anchor.
///
///
/// Creates documents in the collection identified by `collection-name`.
/// The first line of the request body must contain a JSON-encoded array of
/// attribute names. All following lines in the request body must contain
/// JSON-encoded arrays of attribute values. Each line is interpreted as a
/// separate document, and the values specified will be mapped to the array
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
/// - `updated`: number of updated/replaced documents (in case `onDuplicate`
///   was set to either `update` or `replace`).
///
/// - `ignored`: number of failed but ignored insert operations (in case
///   `onDuplicate` was set to `ignore`).
///
/// - `details`: if query parameter `details` is set to true, the result will
///   contain a `details` attribute which is an array with more detailed
///   information about which documents could not be inserted.
///
/// Note: this API is currently not supported on cluster coordinators.
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
/// @RESTRETURNCODE{501}
/// The server will respond with *HTTP 501* if this API is called on a cluster
/// coordinator.
///
/// @EXAMPLES
///
/// Importing two documents, with attributes `_key`, `value1` and `value2` each.
/// One
/// line in the import data is empty
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportCsvExample}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     var body = '[ "_key", "value1", "value2" ]\n' + 
///                '[ "abc", 25, "test" ]\n\n' + 
///                '[ "foo", "bar", "baz" ]';
///
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" +
///     cn, body);
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
/// Importing two documents into a new collection
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportCsvCreate}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     var body = '[ "value1", "value2" ]\n' + 
///                '[ 1234, null ]\n' + 
///                '[ "foo", "bar" ]\n' + 
///                '[534.55, true ]';
///
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn
///     + "&createCollection=true", body);
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
/// Importing into an edge collection, with attributes `_from`, `_to` and `name`
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportCsvEdge}
///     var cn = "links";
///     db._drop(cn);
///     db._createEdgeCollection(cn);
///     db._drop("products");
///     db._create("products");
///
///     var body = '[ "_from", "_to", "name" ]\n' + 
///                '[ "products/123","products/234", "some name" ]\n' +
///                '[ "products/332", "products/abc", "other name" ]';
///
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" +
///     cn, body);
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
/// Importing into an edge collection, omitting `_from` or `_to`
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportCsvEdgeInvalid}
///     var cn = "links";
///     db._drop(cn);
///     db._createEdgeCollection(cn);
///
///     var body = '[ "name" ]\n[ "some name" ]\n[ "other name" ]';
///
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn
///     + "&details=true", body);
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
/// Violating a unique constraint, but allow partial imports
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportCsvUniqueContinue}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     var body = '[ "_key", "value1", "value2" ]\n' + 
///                '[ "abc", 25, "test" ]\n' +
///                '["abc", "bar", "baz" ]';
///
///     var response = logCurlRequestRaw('POST', "/_api/import?collection=" + cn
///     + "&details=true", body);
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
/// Violating a unique constraint, not allowing partial imports
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportCsvUniqueFail}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     var body = '[ "_key", "value1", "value2" ]\n' +
///                '[ "abc", 25, "test" ]\n' + 
///                '["abc", "bar", "baz" ]';
///
///     var response = logCurlRequest('POST', "/_api/import?collection=" + cn +
///     "&complete=true", body);
///
///     assert(response.code === 409);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Using a non-existing collection
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportCsvInvalidCollection}
///     var cn = "products";
///     db._drop(cn);
///
///     var body = '[ "_key", "value1", "value2" ]\n' + 
///                '[ "abc", 25, "test" ]\n' + 
///                '["foo", "bar", "baz" ]';
///
///     var response = logCurlRequest('POST', "/_api/import?collection=" + cn,
///     body);
///
///     assert(response.code === 404);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Using a malformed body
///
/// @EXAMPLE_ARANGOSH_RUN{RestImportCsvInvalidBody}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     var body = '{ "_key": "foo", "value1": "bar" }';
///
///     var response = logCurlRequest('POST', "/_api/import?collection=" + cn,
///     body);
///
///     assert(response.code === 400);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

bool RestImportHandler::createFromKeyValueList() {
  RestImportResult result;

  vector<string> const& suffix = _request->suffix();

  if (suffix.size() != 0) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "superfluous suffix, expecting " + IMPORT_PATH +
                      "?collection=<identifier>");
    return false;
  }

  bool const waitForSync = extractWaitForSync();
  bool const complete = extractComplete();
  bool const overwrite = extractOverwrite();

  // extract the collection name
  bool found;
  string const& collection = _request->value("collection", found);

  if (!found || collection.empty()) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,
                  "'collection' is missing, expecting " + IMPORT_PATH +
                      "?collection=<identifier>");
    return false;
  }

  if (!checkCreateCollection(collection, getCollectionType())) {
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
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "no JSON array found in second line");
    return false;
  }

  char const* lineStart = current;
  char const* lineEnd = next;

  // trim line
  while (lineStart < bodyEnd &&
         (*lineStart == ' ' || *lineStart == '\t' || *lineStart == '\r' ||
          *lineStart == '\n' || *lineStart == '\b' || *lineStart == '\f')) {
    ++lineStart;
  }

  while (lineEnd > lineStart &&
         (*(lineEnd - 1) == ' ' || *(lineEnd - 1) == '\t' ||
          *(lineEnd - 1) == '\r' || *(lineEnd - 1) == '\n' ||
          *(lineEnd - 1) == '\b' || *(lineEnd - 1) == '\f')) {
    --lineEnd;
  }

  *(const_cast<char*>(lineEnd)) = '\0';
  bool success = false;
  std::shared_ptr<VPackBuilder> parsedKeys;
  try {
    parsedKeys = parseVelocyPackLine(lineStart, lineEnd, success);
  } catch (...) {
    // This throws if the body is not parseable
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "no JSON string array found in first line");
    return false;
  }
  VPackSlice const keys = parsedKeys->slice();

  if (!success || !checkKeys(keys)) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "no JSON string array found in first line");
    return false;
  }

  current = next + 1;

  // find and load collection given by name or identifier
  RestImportTransaction trx(new StandaloneTransactionContext(), _vocbase,
                            collection);

  // .............................................................................
  // inside write transaction
  // .............................................................................

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
    return false;
  }

  TRI_document_collection_t* document = trx.documentCollection();
  bool const isEdgeCollection = (document->_info.type() == TRI_COL_TYPE_EDGE);

  trx.lockWrite();

  if (overwrite) {
    // truncate collection first
    trx.truncate(false);
  }

  size_t i = (size_t)lineNumber;

  while (current != nullptr && current < bodyEnd) {
    i++;

    next = static_cast<char const*>(memchr(current, '\n', bodyEnd - current));

    char const* lineStart = current;
    char const* lineEnd = next;

    if (next == nullptr) {
      // reached the end
      lineEnd = bodyEnd;
      current = nullptr;
    } else {
      // got more to read
      current = next + 1;
      *(const_cast<char*>(lineEnd)) = '\0';
    }

    // trim line
    while (lineStart < bodyEnd &&
           (*lineStart == ' ' || *lineStart == '\t' || *lineStart == '\r' ||
            *lineStart == '\n' || *lineStart == '\b' || *lineStart == '\f')) {
      ++lineStart;
    }

    while (lineEnd > lineStart &&
           (*(lineEnd - 1) == ' ' || *(lineEnd - 1) == '\t' ||
            *(lineEnd - 1) == '\r' || *(lineEnd - 1) == '\n' ||
            *(lineEnd - 1) == '\b' || *(lineEnd - 1) == '\f')) {
      --lineEnd;
    }

    if (lineStart == lineEnd) {
      ++result._numEmpty;
      continue;
    }

    bool success;
    std::shared_ptr<VPackBuilder> parsedValues =
        parseVelocyPackLine(lineStart, lineEnd, success);

    // build the json object from the array
    string errorMsg;
    if (!success) {
      errorMsg = buildParseError(i, lineStart);
      registerError(result, errorMsg);
      res = TRI_ERROR_INTERNAL;
    } else {
      VPackSlice const values = parsedValues->slice();
      try {
        std::shared_ptr<VPackBuilder> objectBuilder =
            createVelocyPackObject(keys, values, errorMsg, i);
        res =
            handleSingleDocument(trx, result, lineStart, objectBuilder->slice(),
                                 isEdgeCollection, waitForSync, i);
      } catch (...) {
        // raise any error
        res = TRI_ERROR_INTERNAL;
        registerError(result, errorMsg);
      }
    }

    if (res != TRI_ERROR_NO_ERROR) {
      if (complete) {
        // only perform a full import: abort
        break;
      }

      res = TRI_ERROR_NO_ERROR;
    }
  }

  // we'll always commit, even if previous errors occurred
  res = trx.finish(res);

  // .............................................................................
  // outside write transaction
  // .............................................................................

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
  } else {
    // generate result
    generateDocumentsCreated(result);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create response for number of documents created / failed
////////////////////////////////////////////////////////////////////////////////

void RestImportHandler::generateDocumentsCreated(
    RestImportResult const& result) {
  createResponse(HttpResponse::CREATED);
  _response->setContentType("application/json; charset=utf-8");

  try {
    VPackBuilder json;
    json.add(VPackValue(VPackValueType::Object));
    json.add("error", VPackValue(false));
    json.add("created", VPackValue(result._numCreated));
    json.add("errors", VPackValue(result._numErrors));
    json.add("empty", VPackValue(result._numEmpty));
    json.add("updated", VPackValue(result._numUpdated));
    json.add("ignored", VPackValue(result._numIgnored));

    bool found;
    char const* detailsStr = _request->value("details", found);

    // include failure details?
    if (found && StringUtils::boolean(detailsStr)) {
      json.add("details", VPackValue(VPackValueType::Array));

      for (size_t i = 0, n = result._errors.size(); i < n; ++i) {
        json.add(VPackValue(result._errors[i]));
      }
      json.close();
    }
    json.close();
    VPackSlice s = json.slice();
    generateResult(HttpResponse::CREATED, s);
  } catch (...) {
    // Ignore the error
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse a single document line
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VPackBuilder> RestImportHandler::parseVelocyPackLine(
    string const& line, bool& success) {
  try {
    success = true;
    return VPackParser::fromJson(line);
  } catch (VPackException const&) {
    success = false;
    VPackParser p;
    return p.steal();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse a single document line
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VPackBuilder> RestImportHandler::parseVelocyPackLine(
    char const* start, char const* end, bool& success) {
  try {
    std::string tmp(start, end);
    return parseVelocyPackLine(tmp, success);
  } catch (std::exception const&) {
    // The line is invalid and could not be transformed into a string
    success = false;
    VPackParser p;
    return p.steal();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a VelocyPack object from a key and value list
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VPackBuilder> RestImportHandler::createVelocyPackObject(
    VPackSlice const& keys, VPackSlice const& values, string& errorMsg,
    size_t lineNumber) {
  if (!values.isArray()) {
    errorMsg = positionise(lineNumber) + "no valid JSON array data";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, errorMsg);
  }

  TRI_ASSERT(keys.isArray());
  VPackValueLength const n = keys.length();
  VPackValueLength const m = values.length();

  if (n != m) {
    errorMsg = positionise(lineNumber) + "wrong number of JSON values (got " +
               to_string(m) + ", expected " + to_string(n) + ")";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, errorMsg);
  }

  try {
    auto result = std::make_shared<VPackBuilder>();
    result->openObject();

    for (size_t i = 0; i < n; ++i) {
      VPackSlice const key = keys.at(i);
      VPackSlice const value = values.at(i);

      if (key.isString() && !value.isNone() && !value.isNull()) {
        std::string tmp = key.copyString();
        result->add(tmp, value);
      }
    }
    result->close();

    return result;
  } catch (std::bad_alloc const&) {
    LOG_ERROR("out of memory");
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate keys
////////////////////////////////////////////////////////////////////////////////

bool RestImportHandler::checkKeys(VPackSlice const& keys) const {
  if (!keys.isArray()) {
    return false;
  }

  VPackValueLength const n = keys.length();

  if (n == 0) {
    return false;
  }

  for (VPackSlice const& key : VPackArrayIterator(keys)) {
    if (!key.isString()) {
      return false;
    }
  }

  return true;
}


