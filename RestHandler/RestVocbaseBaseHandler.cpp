////////////////////////////////////////////////////////////////////////////////
/// @brief abstract base request handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2010-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestVocbaseBaseHandler.h"

#include <Basics/string-buffer.h>
#include <Basics/StringUtils.h>

#include <Rest/HttpRequest.h>

#include <VocBase/document-collection.h>
#include <VocBase/result-set.h>

#include "ShapedJson/shaped-json.h"

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
/// @brief action path
////////////////////////////////////////////////////////////////////////////////

string RestVocbaseBaseHandler::ACTION_PATH = "/_action";

////////////////////////////////////////////////////////////////////////////////
/// @brief document path
////////////////////////////////////////////////////////////////////////////////

string RestVocbaseBaseHandler::DOCUMENT_PATH = "/_document";

////////////////////////////////////////////////////////////////////////////////
/// @brief collection path
////////////////////////////////////////////////////////////////////////////////

string RestVocbaseBaseHandler::COLLECTION_PATH = "/_collection";

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
    _resultSet(0),
    _timing(),
    _timingResult(RES_FAIL) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

RestVocbaseBaseHandler::~RestVocbaseBaseHandler () {
  if (_resultSet != 0) {
    _resultSet->free(_resultSet);
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
/// @brief generates ok message without content
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateOk (TRI_voc_cid_t, TRI_voc_did_t, TRI_voc_rid_t rid) {
  response = new HttpResponse(HttpResponse::NO_CONTENT);

  response->setHeader("ETag", "\"" + StringUtils::itoa(rid) + "\"");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates created message without content but a location header
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateCreated (TRI_voc_cid_t cid, TRI_voc_did_t did, TRI_voc_rid_t rid) {
  response = new HttpResponse(HttpResponse::CREATED);

  response->setHeader("ETag", "\"" + StringUtils::itoa(rid) + "\"");
  response->setHeader("location", DOCUMENT_PATH + "/" + StringUtils::itoa(cid) + ":" + StringUtils::itoa(did));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates collection not found error message
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateCollectionNotFound (string const& cid) {
  generateError(HttpResponse::NOT_FOUND, "collection " + COLLECTION_PATH + "/" + cid + " not found");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates document not found error message
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateDocumentNotFound (string const& cid, string const& did) {
  generateError(HttpResponse::NOT_FOUND, "document " + DOCUMENT_PATH + "/" + cid + ":" + did + " not found");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates conflict message
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateConflict (string const& cid, string const& did) {
  generateError(HttpResponse::CONFLICT, "document " + DOCUMENT_PATH + "/" + cid + ":" + did + " is been altered");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates not implemented
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateNotImplemented (string const& path) {
  generateError(HttpResponse::NOT_IMPLEMENTED, path + " not implemented");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates next entry from a result set
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::generateResultSetNext () {
  if (_resultSet == 0 || ! _resultSet->hasNext(_resultSet)) {
    generateError(HttpResponse::SERVER_ERROR, "result set is empty");
    return;
  }

  TRI_doc_collection_t* collection = _resultSet->_containerElement->_container->_collection;

  // executes a result set query
  TRI_voc_did_t did;
  TRI_voc_did_t rid;
  TRI_json_t const* a;
  TRI_json_t augmented;
  TRI_shaped_json_t* result = _resultSet->next(_resultSet, &did, &rid, &a);

  // add document identifier to buffer
  string id = StringUtils::itoa(_collection->_cid) + ":" + StringUtils::itoa(did);

  if (a == NULL || a->_type != TRI_JSON_ARRAY) {
    augmented._type = TRI_JSON_ARRAY;
    TRI_InitVector(&augmented._value._objects, sizeof(TRI_json_t));
  }
  else {
    TRI_CopyToJson(&augmented, a);
  }

  TRI_Insert2ArrayJson(&augmented, "_id", TRI_CreateStringCopyJson(id.c_str()));

  // convert object to string
  TRI_string_buffer_t buffer;
  TRI_InitStringBuffer(&buffer);

  TRI_StringifyAugmentedShapedJson(collection->_shaper, &buffer, result, &augmented);

  TRI_DestroyJson(&augmented);

  // and generate a response
  response = new HttpResponse(HttpResponse::OK);
  response->setContentType("application/json");
  response->setHeader("ETag", "\"" + StringUtils::itoa(rid) + "\"");

  response->body().appendText(TRI_BeginStringBuffer(&buffer), TRI_LengthStringBuffer(&buffer));

  TRI_DestroyStringBuffer(&buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief splits a document reference into to parts
////////////////////////////////////////////////////////////////////////////////

vector<string> RestVocbaseBaseHandler::splitDocumentReference (string const& name) {
  return StringUtils::split(name, ":");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the revision
////////////////////////////////////////////////////////////////////////////////

TRI_voc_rid_t RestVocbaseBaseHandler::extractRevision () {
  bool found;
  string etag = request->header("etag", found);

  if (found && ! etag.empty() && etag[0] == '"' && etag[etag.length()-1] == '"') {
    return StringUtils::uint64(etag.c_str() + 1, etag.length() - 2);
  }
  else if (found) {
    return 0;
  }

  etag = request->value("_rev", found);

  if (found) {
    return StringUtils::uint64(etag.c_str() + 1, etag.length() - 2);
  }
  else {
    return 0;
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
/// @brief sets the collection variable using a a name or an identifier
////////////////////////////////////////////////////////////////////////////////

bool RestVocbaseBaseHandler::findCollection (string const& name) {
  _collection = 0;

  if (name.empty()) {
    return false;
  }

  if (isdigit(name[0])) {
    TRI_voc_cid_t id = StringUtils::uint64(name);

    _collection = TRI_LookupCollectionByIdVocBase(_vocbase, id);
  }
  else {
    _collection = TRI_LookupCollectionByNameVocBase(_vocbase, name.c_str());
  }

  if (_collection == 0) {
    generateCollectionNotFound(name);
    return false;
  }
  else {
    return true;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates or loads a collection
////////////////////////////////////////////////////////////////////////////////

bool RestVocbaseBaseHandler::loadCollection () {
  _documentCollection = 0;

  // collection name must be known
  if (_collection == 0) {
    generateError(HttpResponse::SERVER_ERROR, "cannot create or load collection");
    return false;
  }

  // check for corrupted collections
  if (_collection->_corrupted) {
    generateError(HttpResponse::SERVER_ERROR, "collection is corrupted, please run collection check");
    return false;
  }

  // check for loaded collections
  if (_collection->_loaded) {
    if (_collection->_collection == 0) {
      generateError(HttpResponse::SERVER_ERROR, "cannot load collection, check log");
      return false;
    }

    _documentCollection = const_cast<TRI_doc_collection_t*>(_collection->_collection);
    return true;
  }

  // create or load collection
  if (_collection->_newBorn) {
    LOGGER_INFO << "creating new collection: '" << _collection->_name << "'";

    bool ok = TRI_ManifestCollectionVocBase(_collection->_vocbase, _collection);

    if (! ok) {
      generateError(HttpResponse::SERVER_ERROR, "cannot create collection");
      return false;
    }

    LOGGER_DEBUG << "collection created";
  }
  else {
    LOGGER_INFO << "loading collection: '" << _collection->_name << "'";

    _collection = TRI_LoadCollectionVocBase(_collection->_vocbase, _collection->_name);

    LOGGER_DEBUG << "collection loaded";
  }

  if (_collection == 0 || _collection->_collection == 0) {
    generateError(HttpResponse::SERVER_ERROR, "cannot load collection, check log");
    return false;
  }

  if (_collection->_corrupted) {
    generateError(HttpResponse::SERVER_ERROR, "collection is corrupted, please run collection check");
    return false;
  }

  _documentCollection = const_cast<TRI_doc_collection_t*>(_collection->_collection);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the body
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* RestVocbaseBaseHandler::parseJsonBody () {
  char* error = 0;
  TRI_json_t* json = TRI_Json2String(request->body().c_str(), &error);

  if (json == 0) {
    if (error == 0) {
      generateError(HttpResponse::BAD, "cannot parse json object");
    }
    else {
      generateError(HttpResponse::BAD, error);
    }

    return false;
  }

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the restult-set, needs a loaded collection
////////////////////////////////////////////////////////////////////////////////

void RestVocbaseBaseHandler::findDocument (string const& doc) {
  if (_resultSet != 0) {
    _resultSet->free(_resultSet);
    _resultSet = 0;
  }

  if (_documentCollection == 0) {
    return;
  }

  uint32_t id = StringUtils::uint32(doc);

  // .............................................................................
  // inside read transaction
  // .............................................................................

  _documentCollection->beginRead(_documentCollection);

  TRI_doc_mptr_t const* document = _documentCollection->read(_documentCollection, id);
  TRI_rs_container_element_t* ce = TRI_AddResultSetRSContainer(&_documentCollection->_resultSets);

  _resultSet = TRI_CreateRSSingle(_documentCollection, ce, document, 1);

  _documentCollection->endRead(_documentCollection);

  // .............................................................................
  // outside read transaction
  // .............................................................................
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
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
