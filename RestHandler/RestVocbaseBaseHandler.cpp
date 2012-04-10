////////////////////////////////////////////////////////////////////////////////
/// @brief abstract base request handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2010-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestVocbaseBaseHandler.h"

#include "Basics/StringUtils.h"
#include "BasicsC/conversions.h"
#include "BasicsC/string-buffer.h"
#include "BasicsC/strings.h"
#include "Rest/HttpRequest.h"
#include "ShapedJson/shaped-json.h"
#include "VocBase/document-collection.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::avocado;

// -----------------------------------------------------------------------------
// --SECTION--                                         REST_VOCBASE_BASE_HANDLER
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoDB
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

string RestVocbaseBaseHandler::DOCUMENT_PATH = "/document";

////////////////////////////////////////////////////////////////////////////////
/// @brief document path
////////////////////////////////////////////////////////////////////////////////

string RestVocbaseBaseHandler::EDGE_PATH = "/edge";

////////////////////////////////////////////////////////////////////////////////
/// @brief collection path
////////////////////////////////////////////////////////////////////////////////

string RestVocbaseBaseHandler::COLLECTION_PATH = "/collection";

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestVocbaseBaseHandler::RestVocbaseBaseHandler (HttpRequest* request, TRI_vocbase_t* vocbase)
  : RestBaseHandler(request),
    _vocbase(vocbase),
    _collection(0),
    _documentCollection(0),
    _barrier(0),
    _timing(),
    _timingResult(RES_FAIL) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

RestVocbaseBaseHandler::~RestVocbaseBaseHandler () {
  if (_barrier != 0) {
    TRI_FreeBarrier(_barrier);
  }

  LOGGER_REQUEST_IN_END_I(_timing) << _timingResult;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief generates ok message without content
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateOk () {
  response = new HttpResponse(HttpResponse::NO_CONTENT);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates created message
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateCreated (TRI_voc_cid_t cid, TRI_voc_did_t did, TRI_voc_rid_t rid) {
  string cidStr = StringUtils::itoa(cid);
  string didStr = StringUtils::itoa(did);
  string ridStr = StringUtils::itoa(rid);
  string handle = cidStr + "/" + didStr;

  response = new HttpResponse(HttpResponse::CREATED);

  response->setContentType("application/json; charset=utf-8");
  response->setHeader("ETag", "\"" + ridStr + "\"");
  response->setHeader("location", DOCUMENT_PATH + "/" + handle);

  response->body()
    .appendText("{\"error\":false,\"_id\":\"")
    .appendText(handle.c_str())
    .appendText("\",\"_rev\":")
    .appendInteger(rid)
    .appendText("}");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates accepted message
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateAccepted (TRI_voc_cid_t cid, TRI_voc_did_t did, TRI_voc_rid_t rid) {
  string cidStr = StringUtils::itoa(cid);
  string didStr = StringUtils::itoa(did);
  string ridStr = StringUtils::itoa(rid);
  string handle = cidStr + "/" + didStr;

  response = new HttpResponse(HttpResponse::ACCEPTED);

  response->setContentType("application/json; charset=utf-8");
  response->setHeader("ETag", "\"" + ridStr + "\"");
  response->setHeader("location", DOCUMENT_PATH + "/" + handle);

  response->body()
    .appendText("{\"error\":false,\"_id\":\"")
    .appendText(handle.c_str())
    .appendText("\",\"_rev\":")
    .appendInteger(rid)
    .appendText("}");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates deleted message
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateDeleted (TRI_voc_cid_t cid, TRI_voc_did_t did, TRI_voc_rid_t rid) {
  string cidStr = StringUtils::itoa(cid);
  string didStr = StringUtils::itoa(did);
  string ridStr = StringUtils::itoa(rid);
  string handle = cidStr + "/" + didStr;

  response = new HttpResponse(HttpResponse::OK);

  response->setContentType("application/json; charset=utf-8");

  response->body()
    .appendText("{\"error\":false,\"_id\":\"")
    .appendText(handle.c_str())
    .appendText("\",\"_rev\":")
    .appendInteger(rid)
    .appendText("}");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates updated message
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateUpdated (TRI_voc_cid_t cid, TRI_voc_did_t did, TRI_voc_rid_t rid) {
  string cidStr = StringUtils::itoa(cid);
  string didStr = StringUtils::itoa(did);
  string ridStr = StringUtils::itoa(rid);
  string handle = cidStr + "/" + didStr;

  response = new HttpResponse(HttpResponse::OK);

  response->setContentType("application/json; charset=utf-8");

  response->body()
    .appendText("{\"error\":false,\"_id\":\"")
    .appendText(handle.c_str())
    .appendText("\",\"_rev\":")
    .appendInteger(rid)
    .appendText("}");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates collection not found error message
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateCollectionNotFound (string const& cid) {
  generateError(HttpResponse::NOT_FOUND, 
                TRI_ERROR_AVOCADO_COLLECTION_NOT_FOUND,
                "collection " + COLLECTION_PATH + "/" + cid + " not found");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates document not found error message
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateDocumentNotFound (TRI_voc_cid_t cid, string const& did) {
  string location = DOCUMENT_PATH + "/" + StringUtils::itoa(cid) + TRI_DOCUMENT_HANDLE_SEPARATOR_STR + did;

  generateError(HttpResponse::NOT_FOUND,
                TRI_ERROR_AVOCADO_DOCUMENT_NOT_FOUND,
                "document " + location + " not found");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates conflict message
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateConflict (string const& cid, string const& did) {
  generateError(HttpResponse::CONFLICT, 
                TRI_ERROR_AVOCADO_CONFLICT,
                "document " + DOCUMENT_PATH + "/" + cid + "/" + did + " has been altered");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates not implemented
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateNotImplemented (string const& path) {
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

void RestVocbaseBaseHandler::generatePreconditionFailed (TRI_voc_cid_t cid, TRI_voc_did_t did, TRI_voc_rid_t rid) {
  response = new HttpResponse(HttpResponse::PRECONDITION_FAILED);
  response->setContentType("application/json; charset=utf-8");

  response->body()
    .appendText("{\"error\":true,\"_id\":\"")
    .appendInteger(cid).appendChar(TRI_DOCUMENT_HANDLE_SEPARATOR_CHR).appendInteger(did)
    .appendText("\",\"_rev\":")
    .appendInteger(rid)
    .appendText("}");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates not modified
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateNotModified (string const& etag) {
  response = new HttpResponse(HttpResponse::NOT_MODIFIED);

  response->setHeader("ETag", "\"" + etag + "\"");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates next entry from a result set
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateDocument (TRI_doc_mptr_t const* document,
                                               bool generateDocument) {
  if (document == 0 || _documentCollection == 0) {
    generateError(HttpResponse::SERVER_ERROR, 
                  TRI_ERROR_INTERNAL,
                  "document or collection pinter is null, should not happen");
    return;
  }

  // add document identifier to buffer
  TRI_string_buffer_t buffer;

  string id = StringUtils::itoa(_documentCollection->base._cid) + TRI_DOCUMENT_HANDLE_SEPARATOR_STR + StringUtils::itoa(document->_did);

  TRI_json_t augmented;
  TRI_InitArrayJson(&augmented);

  TRI_json_t* _id = TRI_CreateStringCopyJson(id.c_str());

  if (_id) {
    TRI_Insert2ArrayJson(&augmented, "_id", _id);
  }

  TRI_json_t* _rev = TRI_CreateNumberJson(document->_rid);

  if (_rev) {
    TRI_Insert2ArrayJson(&augmented, "_rev", _rev);
  }

  TRI_df_marker_type_t type = ((TRI_df_marker_t*) document->_data)->_type;

  if (type == TRI_DOC_MARKER_EDGE) {
    TRI_doc_edge_marker_t* marker = (TRI_doc_edge_marker_t*) document->_data;
    string from = StringUtils::itoa(marker->_fromCid) + TRI_DOCUMENT_HANDLE_SEPARATOR_STR + StringUtils::itoa(marker->_fromDid);
    string to = StringUtils::itoa(marker->_toCid) + TRI_DOCUMENT_HANDLE_SEPARATOR_STR + StringUtils::itoa(marker->_toDid);

    TRI_Insert3ArrayJson(&augmented, "_from", TRI_CreateStringCopyJson(from.c_str()));
    TRI_Insert3ArrayJson(&augmented, "_to", TRI_CreateStringCopyJson(to.c_str()));
  }

  // convert object to string
  TRI_InitStringBuffer(&buffer);

  TRI_StringifyAugmentedShapedJson(_documentCollection->_shaper, &buffer, &document->_document, &augmented);

  TRI_DestroyJson(&augmented);

  if (_id) {
    TRI_Free(_id);
  }

  if (_rev) {
    TRI_Free(_rev);
  }

  // and generate a response
  response = new HttpResponse(HttpResponse::OK);
  response->setContentType("application/json; charset=utf-8");
  response->setHeader("ETag", "\"" + StringUtils::itoa(document->_rid) + "\"");

  if (generateDocument) {
    response->body().appendText(TRI_BeginStringBuffer(&buffer), TRI_LengthStringBuffer(&buffer));
  }
  else {
    response->headResponse(TRI_LengthStringBuffer(&buffer));
  }

  TRI_AnnihilateStringBuffer(&buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the revision
////////////////////////////////////////////////////////////////////////////////

TRI_voc_rid_t RestVocbaseBaseHandler::extractRevision (string const& header, string const& parameter) {
  bool found;
  string etag = StringUtils::trim(request->header(header, found));

  if (found && ! etag.empty() && etag[0] == '"' && etag[etag.length()-1] == '"') {
    return StringUtils::uint64(etag.c_str() + 1, etag.length() - 2);
  }
  else if (found) {
    return 0;
  }

  if (parameter.empty()) {
    return 0;
  }
  else {
    etag = request->value(parameter, found);

    if (found) {
      return StringUtils::uint64(etag.c_str());
    }
    else {
      return 0;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the update policy
////////////////////////////////////////////////////////////////////////////////

TRI_doc_update_policy_e RestVocbaseBaseHandler::extractUpdatePolicy () {
  bool found;
  string policy = request->value("policy", found);

  if (found) {
    policy = StringUtils::tolower(policy);

    if (policy == "error") {
      return TRI_DOC_UPDATE_ERROR;
    }
    else if (policy == "last") {
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
/// @brief uses a collection, loading or manifesting and locking it
////////////////////////////////////////////////////////////////////////////////

int RestVocbaseBaseHandler::useCollection (string const& name, bool create) {
  _collection = 0;
  _documentCollection = 0;

  // sanity check
  if (name.empty()) {
    generateError(HttpResponse::BAD, 
                  TRI_ERROR_HTTP_CORRUPTED_JSON,
                  "collection identifier is empty");
    return TRI_set_errno(TRI_ERROR_HTTP_CORRUPTED_JSON);
  }

  // try to find the collection
  if (isdigit(name[0])) {
    TRI_voc_cid_t id = StringUtils::uint64(name);

    _collection = TRI_LookupCollectionByIdVocBase(_vocbase, id);
  }
  else {
    _collection = TRI_FindCollectionByNameVocBase(_vocbase, name.c_str(), create);
  }

  if (_collection == 0) {
    generateCollectionNotFound(name);
    return TRI_set_errno(TRI_ERROR_AVOCADO_COLLECTION_NOT_FOUND);
  }

  // and use the collection
  int res = TRI_UseCollectionVocBase(_vocbase, const_cast<TRI_vocbase_col_s*>(_collection));

  if (res == TRI_ERROR_NO_ERROR) {
    _documentCollection = _collection->_collection;
    assert(_documentCollection != 0);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a collection
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::releaseCollection () {
  if (_collection == 0) {
    return;
  }

  TRI_ReleaseCollectionVocBase(_vocbase, _collection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the body
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* RestVocbaseBaseHandler::parseJsonBody () {
  char* errmsg = 0;
  TRI_json_t* json = TRI_Json2String(request->body().c_str(), &errmsg);

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

      TRI_FreeString(errmsg);
    }

    return 0;
  }

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the restult-set, needs a loaded collection
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t const RestVocbaseBaseHandler::findDocument (string const& doc) {
  TRI_doc_mptr_t document;

  if (_documentCollection == 0) {
    document._did = 0;
    return document;
  }

  TRI_voc_did_t id = StringUtils::uint64(doc);

  // .............................................................................
  // inside read transaction
  // .............................................................................

  _documentCollection->beginRead(_documentCollection);

  document = _documentCollection->read(_documentCollection, id);

  // keep the oldest barrier
  if (_barrier != 0) {
    _barrier = TRI_CreateBarrierElement(&_documentCollection->_barrierList);
  }

  _documentCollection->endRead(_documentCollection);

  // .............................................................................
  // outside read transaction
  // .............................................................................

  return document;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a document handle
////////////////////////////////////////////////////////////////////////////////

int RestVocbaseBaseHandler::parseDocumentId (string const& handle,
                                             TRI_voc_cid_t& cid,
                                             TRI_voc_did_t& did) {
  vector<string> split;
  int res;

  split = StringUtils::split(handle, '/');

  if (split.size() != 2) {
    return TRI_set_errno(TRI_ERROR_HTTP_BAD_PARAMETER);
  }

  cid = TRI_UInt64String(split[0].c_str());
  res = TRI_errno();

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  did = TRI_UInt64String(split[1].c_str());

  return TRI_errno();
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
/// @addtogroup AvocadoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestVocbaseBaseHandler::isDirect () {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
