////////////////////////////////////////////////////////////////////////////////
/// @brief abstract base request handler
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

#include "RestVocbaseBaseHandler.h"

#include "Basics/StringUtils.h"
#include "BasicsC/conversions.h"
#include "BasicsC/string-buffer.h"
#include "BasicsC/strings.h"
#include "Rest/HttpRequest.h"
#include "ShapedJson/shaped-json.h"
#include "Utils/DocumentHelper.h"
#include "VocBase/primary-collection.h"
#include "VocBase/document-collection.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                         REST_VOCBASE_BASE_HANDLER
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief result RES-OK
////////////////////////////////////////////////////////////////////////////////

LoggerData::Extra const RestVocbaseBaseHandler::RES_OK;

////////////////////////////////////////////////////////////////////////////////
/// @brief result RES-ERR
////////////////////////////////////////////////////////////////////////////////

LoggerData::Extra const RestVocbaseBaseHandler::RES_ERR;

////////////////////////////////////////////////////////////////////////////////
/// @brief result RES-ERR
////////////////////////////////////////////////////////////////////////////////

LoggerData::Extra const RestVocbaseBaseHandler::RES_FAIL;

////////////////////////////////////////////////////////////////////////////////
/// @brief document path
////////////////////////////////////////////////////////////////////////////////

string RestVocbaseBaseHandler::DOCUMENT_PATH = "/_api/document";

////////////////////////////////////////////////////////////////////////////////
/// @brief document path
////////////////////////////////////////////////////////////////////////////////

string RestVocbaseBaseHandler::EDGE_PATH = "/_api/edge";

////////////////////////////////////////////////////////////////////////////////
/// @brief collection path
////////////////////////////////////////////////////////////////////////////////

string RestVocbaseBaseHandler::COLLECTION_PATH = "/_api/collection";

////////////////////////////////////////////////////////////////////////////////
/// @brief documents import path
////////////////////////////////////////////////////////////////////////////////

string RestVocbaseBaseHandler::DOCUMENT_IMPORT_PATH = "/_api/import";

////////////////////////////////////////////////////////////////////////////////
/// @brief batch path
////////////////////////////////////////////////////////////////////////////////

string RestVocbaseBaseHandler::BATCH_PATH = "/_api/batch";

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

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

RestVocbaseBaseHandler::RestVocbaseBaseHandler (HttpRequest* request, TRI_vocbase_t* vocbase)
  : RestBaseHandler(request),
    _vocbase(vocbase),
    _resolver(vocbase),
    _timing(),
    _timingResult(RES_FAIL) {

}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

RestVocbaseBaseHandler::~RestVocbaseBaseHandler () {
  LOGGER_REQUEST_IN_END_I(_timing,  _timingResult);
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
/// @brief check if a collection needs to be created on the fly
///
/// this method will check the "createCollection" attribute of the request. if
/// it is set to true, it will verify that the named collection actually exists.
/// if the collection does not yet exist, it will create it on the fly.
/// if the "createCollection" attribute is not set or set to false, nothing will
/// happen, and the collection name will not be checked
////////////////////////////////////////////////////////////////////////////////

bool RestVocbaseBaseHandler::checkCreateCollection (const string& name,
                                                    const TRI_col_type_e type) {
  bool found;
  char const* valueStr = _request->value("createCollection", found);

  if (! found) {
    // "createCollection" parameter not specified
    return true;
  }

  if (! StringUtils::boolean(valueStr)) {
    // "createCollection" parameter specified, but with non-true value
    return true;
  }

  TRI_vocbase_col_t* collection = TRI_FindCollectionByNameOrBearVocBase(_vocbase, name.c_str(), type);
  if (collection == 0) {
    generateTransactionError(name, TRI_errno());
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a HTTP 201 or 202 response
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generate20x (const HttpResponse::HttpResponseCode responseCode,
                                          const string& collectionName,
                                          const TRI_voc_key_t key,
                                          const TRI_voc_rid_t rid) {
  const string handle = DocumentHelper::assembleDocumentId(collectionName, key);
  const string rev = StringUtils::itoa(rid);

  _response = createResponse(responseCode);
  _response->setContentType("application/json; charset=utf-8");

  if (responseCode != HttpResponse::OK) {
    // 200 OK is sent is case of delete or update.
    // in these cases we do not return etag nor location
    _response->setHeader("etag", 4, "\"" + rev + "\"");
    // handle does not need to be RFC 2047-encoded
    _response->setHeader("location", 8, DOCUMENT_PATH + "/" + handle);
  }

  // _id and _key are safe and do not need to be JSON-encoded
  _response->body()
    .appendText("{\"error\":false,\"_id\":\"")
    .appendText(handle)
    .appendText("\",\"_rev\":\"")
    .appendText(rev)
    .appendText("\",\"_key\":\"")
    .appendText(key)
    .appendText("\"}");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates document not found error message
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateDocumentNotFound (const TRI_voc_cid_t cid,
                                                       TRI_voc_key_t key) {
  generateError(HttpResponse::NOT_FOUND,
                TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND,
                "document " + DOCUMENT_PATH + "/" +
                DocumentHelper::assembleDocumentId(_resolver.getCollectionName(cid), key) + " not found");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates not implemented
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateNotImplemented (const string& path) {
  generateError(HttpResponse::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_IMPLEMENTED,
                "'" + path + "' not implemented");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates forbidden
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateForbidden () {
  generateError(HttpResponse::FORBIDDEN,
                TRI_ERROR_FORBIDDEN,
                "operation forbidden");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates precondition failed
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generatePreconditionFailed (const TRI_voc_cid_t cid,
                                                         TRI_voc_key_t key,
                                                         TRI_voc_rid_t rid) {
  _response = createResponse(HttpResponse::PRECONDITION_FAILED);
  _response->setContentType("application/json; charset=utf-8");

  // _id and _key are safe and do not need to be JSON-encoded
  _response->body()
    .appendText("{\"error\":true,\"code\":")
    .appendInteger((int32_t) HttpResponse::PRECONDITION_FAILED)
    .appendText(",\"errorNum\":")
    .appendInteger((int32_t) TRI_ERROR_ARANGO_CONFLICT)
    .appendText(",\"errorMessage\":\"precondition failed\"")
    .appendText(",\"_id\":\"")
    .appendText(DocumentHelper::assembleDocumentId(_resolver.getCollectionName(cid), key))
    .appendText("\",\"_rev\":\"")
    .appendText(StringUtils::itoa(rid))
    .appendText("\",\"_key\":\"")
    .appendText(key)
    .appendText("\"}");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates not modified
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateNotModified (const TRI_voc_rid_t rid) {
  const string rev = StringUtils::itoa(rid);

  _response = createResponse(HttpResponse::NOT_MODIFIED);
  _response->setHeader("etag", 4, "\"" + rev + "\"");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates next entry from a result set
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateDocument (const TRI_voc_cid_t cid,
                                               TRI_doc_mptr_t const* document,
                                               TRI_shaper_t* shaper,
                                               const bool generateBody) {
  if (document == 0) {
    generateError(HttpResponse::SERVER_ERROR,
                  TRI_ERROR_INTERNAL,
                  "document pointer is null, should not happen");
    return;
  }

  const string id = DocumentHelper::assembleDocumentId(_resolver.getCollectionName(cid), document->_key);

  TRI_json_t augmented;
  TRI_Init2ArrayJson(TRI_UNKNOWN_MEM_ZONE, &augmented, 8);

  TRI_json_t* _id = TRI_CreateString2CopyJson(TRI_UNKNOWN_MEM_ZONE, id.c_str(), id.size());

  if (_id) {
    TRI_Insert2ArrayJson(TRI_UNKNOWN_MEM_ZONE, &augmented, "_id", _id);
  }

  // convert rid from uint64_t to string
  const string rid = StringUtils::itoa(document->_rid);
  TRI_json_t* _rev = TRI_CreateString2CopyJson(TRI_UNKNOWN_MEM_ZONE, rid.c_str(), rid.size());

  if (_rev) {
    TRI_Insert2ArrayJson(TRI_UNKNOWN_MEM_ZONE, &augmented, "_rev", _rev);
  }

  TRI_json_t* _key = TRI_CreateString2CopyJson(TRI_UNKNOWN_MEM_ZONE, document->_key, strlen(document->_key));

  if (_key) {
    TRI_Insert2ArrayJson(TRI_UNKNOWN_MEM_ZONE, &augmented, "_key", _key);
  }

  TRI_df_marker_type_t type = ((TRI_df_marker_t*) document->_data)->_type;

  if (type == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_edge_key_marker_t* marker = (TRI_doc_edge_key_marker_t*) document->_data;
    const string from = DocumentHelper::assembleDocumentId(_resolver.getCollectionName(marker->_fromCid), string((char*) marker + marker->_offsetFromKey));
    const string to = DocumentHelper::assembleDocumentId(_resolver.getCollectionName(marker->_toCid), string((char*) marker +  marker->_offsetToKey));

    TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, &augmented, "_from", TRI_CreateString2CopyJson(TRI_UNKNOWN_MEM_ZONE, from.c_str(), from.size()));
    TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, &augmented, "_to", TRI_CreateString2CopyJson(TRI_UNKNOWN_MEM_ZONE, to.c_str(), to.size()));
  }

  // add document identifier to buffer
  TRI_string_buffer_t buffer;

  // convert object to string
  TRI_InitStringBuffer(&buffer, TRI_UNKNOWN_MEM_ZONE);

  TRI_shaped_json_t shapedJson;
  TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, document->_data);
  TRI_StringifyAugmentedShapedJson(shaper, &buffer, &shapedJson, &augmented);

  TRI_DestroyJson(TRI_UNKNOWN_MEM_ZONE, &augmented);

  if (_id) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, _id);
  }

  if (_rev) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, _rev);
  }

  if (_key) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, _key);
  }

  // and generate a response
  _response = createResponse(HttpResponse::OK);
  _response->setContentType("application/json; charset=utf-8");
  _response->setHeader("etag", 4, "\"" + rid + "\"");

  if (generateBody) {
    _response->body().appendText(TRI_BeginStringBuffer(&buffer), TRI_LengthStringBuffer(&buffer));
  }
  else {
    _response->headResponse(TRI_LengthStringBuffer(&buffer));
  }

  TRI_AnnihilateStringBuffer(&buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate an error message for a transaction error
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateTransactionError (const string& collectionName,
                                                       const int res,
                                                       TRI_voc_key_t key,
                                                       TRI_voc_rid_t rid) {
  switch (res) {
    case TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND:
      if (collectionName.empty()) {
        // no collection name specified
        generateError(HttpResponse::BAD, res, "no collection name specified");
      }
      else {
        // collection name specified but collection not found
        generateError(HttpResponse::NOT_FOUND, res, "collection " + COLLECTION_PATH + "/" + collectionName + " not found");
      }
      return;

    case TRI_ERROR_ARANGO_READ_ONLY:
      generateError(HttpResponse::FORBIDDEN, res, "collection is read-only");
      return;

    case TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED:
      generateError(HttpResponse::CONFLICT, res, "cannot create document, unique constraint violated");
      return;

    case TRI_ERROR_ARANGO_GEO_INDEX_VIOLATED:
      generateError(HttpResponse::BAD, res, "geo constraint violated");
      return;

    case TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD:
      generateError(HttpResponse::BAD, res, "invalid document key");
      return;

    case TRI_ERROR_ARANGO_OUT_OF_KEYS:
      generateError(HttpResponse::SERVER_ERROR, res, "out of keys");
      return;

    case TRI_ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED:
      generateError(HttpResponse::BAD, res, "collection does not allow using user-defined keys");
      return;

    case TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND:
      generateDocumentNotFound(_resolver.getCollectionId(collectionName), key);
      return;

    case TRI_ERROR_ARANGO_CONFLICT:
      generatePreconditionFailed(_resolver.getCollectionId(collectionName), key ? key : (TRI_voc_key_t) "unknown", rid);
      return;

    default:
      generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL, "failed with error: " + string(TRI_errno_string(res)));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the revision
////////////////////////////////////////////////////////////////////////////////

TRI_voc_rid_t RestVocbaseBaseHandler::extractRevision (char const* header,
                                                       char const* parameter) {
  bool found;
  char const* etag = _request->header(header, found);

  if (found) {
    char const* s = etag;
    char const* e = etag + strlen(etag);

    while (s < e && (s[0] == ' ' || s[0] == '\t')) {
      ++s;
    }

    while (s < e && (e[-1] == ' ' || e[-1] == '\t')) {
      --e;
    }

    if (s + 1 < e && s[0] == '"' && e[-1] == '"') {
      return TRI_UInt64String2(s + 1, e - s - 2);
    }
    else {
      return 0;
    }
  }

  if (parameter == 0) {
    return 0;
  }
  else {
    etag = _request->value(parameter, found);

    if (found) {
      return TRI_UInt64String(etag);
    }
    else {
      return 0;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the update policy
////////////////////////////////////////////////////////////////////////////////

TRI_doc_update_policy_e RestVocbaseBaseHandler::extractUpdatePolicy () const {
  bool found;
  char const* policy = _request->value("policy", found);

  if (found) {
    if (TRI_CaseEqualString(policy, "error")) {
      return TRI_DOC_UPDATE_ERROR;
    }
    else if (TRI_CaseEqualString(policy, "last")) {
      return TRI_DOC_UPDATE_LAST_WRITE;
    }
    else {
      return TRI_DOC_UPDATE_ILLEGAL;
    }
  }
  else {
    return TRI_DOC_UPDATE_ERROR;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the waitForSync value
////////////////////////////////////////////////////////////////////////////////

bool RestVocbaseBaseHandler::extractWaitForSync () const {
  bool found;
  char const* forceStr = _request->value("waitForSync", found);

  if (found) {
    return StringUtils::boolean(forceStr);
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the body
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* RestVocbaseBaseHandler::parseJsonBody () {
  char* errmsg = 0;
  TRI_json_t* json = _request->toJson(&errmsg);

  if (json == 0) {
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

    return 0;
  }

  if (TRI_HasDuplicateKeyJson(json)) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_CORRUPTED_JSON,
                  "cannot parse json object");
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    return 0;
  }

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                           HANDLER
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestVocbaseBaseHandler::isDirect () {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract a string attribute from a JSON array
///
/// if the attribute is not there or not a string, this returns 0
////////////////////////////////////////////////////////////////////////////////

char const* RestVocbaseBaseHandler::extractJsonStringValue (const TRI_json_t* const json,
                                                            char const* name) {
  if (json == 0 || json->_type != TRI_JSON_ARRAY) {
    return 0;
  }

  TRI_json_t* value = TRI_LookupArrayJson(json, name);
  if (value == 0 || value->_type != TRI_JSON_STRING) {
    return 0;
  }

  return value->_value._string.data;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a document handle
////////////////////////////////////////////////////////////////////////////////

int RestVocbaseBaseHandler::parseDocumentId (string const& handle,
                                             TRI_voc_cid_t& cid,
                                             TRI_voc_key_t& key) {
  vector<string> split;

  split = StringUtils::split(handle, '/');

  if (split.size() != 2) {
    return TRI_set_errno(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }

  cid = _resolver.getCollectionId(split[0]);
  if (cid == 0) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  key = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, split[1].c_str());

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
