////////////////////////////////////////////////////////////////////////////////
/// @brief document request handler
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

#include "RestDocumentHandler.h"

#include <Basics/StringUtils.h>

#include <Rest/HttpRequest.h>

#include <VocBase/document-collection.h>
#include <VocBase/result-set.h>
#include <VocBase/vocbase.h>

using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::avocado;

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

RestDocumentHandler::RestDocumentHandler (HttpRequest* request, TRI_vocbase_t* vocbase)
  : RestVocbaseBaseHandler(request, vocbase) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_e RestDocumentHandler::execute () {

  // extract the sub-request type
  HttpRequest::HttpRequestType type = request->requestType();

  // prepare logging
  static LoggerData::Task const logCreate(DOCUMENT_PATH + " [create]");
  static LoggerData::Task const logRead(DOCUMENT_PATH + " [read]");
  static LoggerData::Task const logUpdate(DOCUMENT_PATH + " [update]");
  static LoggerData::Task const logDelete(DOCUMENT_PATH + " [delete]");
  static LoggerData::Task const logIllegal(DOCUMENT_PATH + " [illegal]");

  LoggerData::Task const * task = &logCreate;

  switch (type) {
    case HttpRequest::HTTP_REQUEST_DELETE: task = &logDelete; break;
    case HttpRequest::HTTP_REQUEST_GET: task = &logRead; break;
    case HttpRequest::HTTP_REQUEST_POST: task = &logCreate; break;
    case HttpRequest::HTTP_REQUEST_PUT: task = &logUpdate; break;
    case HttpRequest::HTTP_REQUEST_ILLEGAL: task = &logIllegal; break;
  }

  _timing << *task;
  LOGGER_REQUEST_IN_START_I(_timing);

  // execute one of the CRUD methods
  bool res = false;

  switch (type) {
    case HttpRequest::HTTP_REQUEST_POST: res = createDocument(); break;
    case HttpRequest::HTTP_REQUEST_GET: res = readDocument(); break;
    case HttpRequest::HTTP_REQUEST_PUT: res = updateDocument(); break;
    case HttpRequest::HTTP_REQUEST_DELETE: res = deleteDocument(); break;

    case HttpRequest::HTTP_REQUEST_ILLEGAL:
      res = false;
      generateNotImplemented("ILLEGAL " + DOCUMENT_PATH);
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
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a document
///
/// <b><tt>POST /_document/<em>collection-identifier</em></tt></b>
///
/// Creates a new document in the collection identified by the @a
/// collection-identifier.  A JSON representation of the document must be passed
/// in the body of the POST request. If the document was created, then a HTTP
/// 201 is returned and the "Location" header contains the path to the newly
/// created document. The "ETag" contains the revision of the newly created
/// document.
///
/// @verbinclude rest3
///
/// If the @a collection-identifier is unknown, then a HTTP 404 is returned.
///
/// @verbinclude rest4
///
/// If the body does not contain a valid JSON representation of an document,
/// then a HTTP 400 is returned.
///
/// @verbinclude rest5
///
/// <b><tt>POST /_document/<em>collection-name</em></tt></b>
///
/// Creates a new document in the collection named @a collection-name.
///
/// @verbinclude rest6
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::createDocument () {
  vector<string> const& suffix = request->suffix();

  if (suffix.size() != 1) {
    generateError(HttpResponse::BAD, "missing collection identifier");
    return false;
  }

  // find and load collection given by name oder identifier
  bool ok = findCollection(suffix[0]) && loadCollection();

  if (! ok) {
    return false;
  }

  // parse document
  TRI_json_t* json = parseJsonBody();

  if (json == 0) {
    return false;
  }

  // .............................................................................
  // inside write transaction
  // .............................................................................

  _documentCollection->beginWrite(_documentCollection);

  TRI_doc_mptr_t const* mptr = _documentCollection->createJson(_documentCollection, json);
  TRI_voc_did_t did = 0;
  TRI_voc_rid_t rid = 0;

  if (mptr != 0) {
    did = mptr->_did;
    rid = mptr->_rid;
  }

  _documentCollection->endWrite(_documentCollection);

  // .............................................................................
  // outside write transaction
  // .............................................................................

  if (mptr != 0) {
    generateCreated(_documentCollection->base._cid, did, rid);
    return true;
  }
  else {
    if (TRI_errno() == TRI_VOC_ERROR_READ_ONLY) {
      generateError(HttpResponse::FORBIDDEN, "collection is read-only");
      return false;
    }
    else {
      generateError(HttpResponse::SERVER_ERROR, "cannot create, failed with: " + string(TRI_last_error()));
      return false;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a document
///
/// <b><tt>GET /_document/<em>document-reference</em></tt></b>
///
/// Returns the document referenced by @a document-reference. If the document
/// exists, then a HTTP 200 is returned and the JSON representation of the
/// document is the body of the response.
///
/// @verbinclude rest1
///
/// If the document-reference points to a non-existing document, then a
/// HTTP 404 is returned and the body contains an error document.
///
/// @verbinclude rest2
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::readDocument () {
  vector<string> suffix = request->suffix();

  // split the document reference
  if (suffix.size() == 1) {
    suffix = splitDocumentReference(suffix[0]);
  }

  if (suffix.size() != 2) {
    generateError(HttpResponse::BAD, "missing collection or document identifier");
    return false;
  }

  // find and load collection given by name oder identifier
  bool ok = findCollection(suffix[0]) && loadCollection();

  if (! ok) {
    return false;
  }

  // .............................................................................
  // inside read transaction
  // .............................................................................

  _documentCollection->beginRead(_documentCollection);

  findDocument(suffix[1]);

  _documentCollection->endRead(_documentCollection);

  // .............................................................................
  // outside read transaction
  // .............................................................................

  if (_resultSet == 0) {
    generateError(HttpResponse::SERVER_ERROR, "cannot execute find");
    return false;
  }

  if (! _resultSet->hasNext(_resultSet)) {
    generateDocumentNotFound(suffix[0], suffix[1]);
    return false;
  }

  generateResultSetNext();
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document
///
/// <b><tt>PUT /_document/<em>document-reference</em></tt></b>
///
/// Updates the document referenced by @a document-reference. If the document
/// exists and could be updated, then a HTTP 201 is returned and the "ETag"
/// header contains the new revision of the document.
///
/// @verbinclude rest7
///
/// If the document does not exists, then a HTTP 404 is returned.
///
/// @verbinclude rest8
///
/// If an etag is supplied in the "ETag" field, then the AvocadoDB checks that
/// the revision of the document is equal to the etag. If there is a mismatch,
/// then a HTTP 409 conflict is returned and no update is performed.
///
/// @verbinclude rest9
///
/// <b><tt>PUT /_document/<em>document-reference</em>?policy=<em>policy</em></tt></b>
///
/// As before, if @a policy is @c error. If @a pocily is @c last, then the last
/// write will win.
///
/// @verbinclude rest10
///
/// <b><tt>PUT /_document/<em>document-reference</em>?_rev=<em>etag</em></tt></b>
///
/// You can also supply the etag using the parameter "_rev" instead of an "ETag"
/// header.
///
/// @verbinclude rest11
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::updateDocument () {
  vector<string> suffix = request->suffix();

  // split the document reference
  if (suffix.size() == 1) {
    suffix = splitDocumentReference(suffix[0]);
  }

  if (suffix.size() != 2) {
    generateError(HttpResponse::BAD, "missing collection or document identifier");
    return false;
  }

  // find and load collection given by name oder identifier
  bool ok = findCollection(suffix[0]) && loadCollection();

  if (! ok) {
    return false;
  }

  // parse document
  TRI_json_t* json = parseJsonBody();

  if (json == 0) {
    return false;
  }

  // extract document identifier
  TRI_voc_did_t did = StringUtils::uint64(suffix[1]);

  // extract the revision
  TRI_voc_rid_t revision = extractRevision();

  // extract or chose the update policy
  TRI_doc_update_policy_e policy = extractUpdatePolicy();

  // .............................................................................
  // inside write transaction
  // .............................................................................

  _documentCollection->beginWrite(_documentCollection);

  TRI_doc_mptr_t const* mptr = _documentCollection->updateJson(_documentCollection, json, did, revision, policy);
  TRI_voc_rid_t rid = 0;

  if (mptr != 0) {
    rid = mptr->_rid;
  }

  _documentCollection->endWrite(_documentCollection);

  // .............................................................................
  // outside write transaction
  // .............................................................................

  if (mptr != 0) {
    generateOk(_documentCollection->base._cid, did, rid);
    return true;
  }
  else {
    if (TRI_errno() == TRI_VOC_ERROR_READ_ONLY) {
      generateError(HttpResponse::FORBIDDEN, "collection is read-only");
      return false;
    }
    else if (TRI_errno() == TRI_VOC_ERROR_DOCUMENT_NOT_FOUND) {
      generateDocumentNotFound(suffix[0], suffix[1]);
      return false;
    }
    else if (TRI_errno() == TRI_VOC_ERROR_CONFLICT) {
      generateConflict(suffix[0], suffix[1]);
      return false;
    }
    else {
      generateError(HttpResponse::SERVER_ERROR, "cannot update, failed with " + string(TRI_last_error()));
      return false;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document
///
/// <b><tt>DELETE /_document/<em>document-reference</em></tt></b>
///
/// Deletes the document referenced by @a document-reference. If the document
/// exists and could be deleted, then a HTTP 204 is returned.
///
/// @verbinclude rest13
///
/// If the document does not exists, then a HTTP 404 is returned.
///
/// @verbinclude rest14
///
/// If an etag is supplied in the "ETag" field, then the AvocadoDB checks that
/// the revision of the document is equal to the etag. If there is a mismatch,
/// then a HTTP 409 conflict is returned and no delete is performed.
///
/// @verbinclude rest12
///
/// <b><tt>DELETE /_document/<em>document-reference</em>?policy=<em>policy</em></tt></b>
///
/// As before, if @a policy is @c error. If @a pocily is @c last, then the last
/// write will win.
///
/// <b><tt>DELETE /_document/<em>document-reference</em>?_rev=<em>etag</em></tt></b>
///
/// You can also supply the etag using the parameter "_rev" instead of an "ETag"
/// header.
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::deleteDocument () {
  vector<string> suffix = request->suffix();

  // split the document reference
  if (suffix.size() == 1) {
    suffix = splitDocumentReference(suffix[0]);
  }

  if (suffix.size() != 2) {
    generateError(HttpResponse::BAD, "missing collection or document identifier");
    return false;
  }

  // find and load collection given by name oder identifier
  bool ok = findCollection(suffix[0]) && loadCollection();

  if (! ok) {
    return false;
  }

  // extract document identifier
  TRI_voc_did_t did = StringUtils::uint64(suffix[1]);

  // extract the revision
  TRI_voc_rid_t revision = extractRevision();

  // extract or chose the update policy
  TRI_doc_update_policy_e policy = extractUpdatePolicy();

  // .............................................................................
  // inside write transaction
  // .............................................................................

  _documentCollection->beginWrite(_documentCollection);

  ok = _documentCollection->destroy(_documentCollection, did, revision, policy);

  _documentCollection->endWrite(_documentCollection);

  // .............................................................................
  // outside write transaction
  // .............................................................................

  if (ok) {
    generateOk();
    return true;
  }
  else {
    if (TRI_errno() == TRI_VOC_ERROR_READ_ONLY) {
      generateError(HttpResponse::FORBIDDEN, "collection is read-only");
      return false;
    }
    else if (TRI_errno() == TRI_VOC_ERROR_DOCUMENT_NOT_FOUND) {
      generateDocumentNotFound(suffix[0], suffix[1]);
      return false;
    }
    else if (TRI_errno() == TRI_VOC_ERROR_CONFLICT) {
      generateConflict(suffix[0], suffix[1]);
      return false;
    }
    else {
      generateError(HttpResponse::SERVER_ERROR, "cannot delete, failed with " + string(TRI_last_error()));
      return false;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
