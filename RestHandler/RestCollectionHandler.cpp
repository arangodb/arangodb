////////////////////////////////////////////////////////////////////////////////
/// @brief document request handler
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

#include "RestCollectionHandler.h"

#include "Basics/StringUtils.h"
#include "BasicsC/string-buffer.h"
#include "Rest/HttpRequest.h"
#include "VocBase/simple-collection.h"
#include "VocBase/vocbase.h"

using namespace std;
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

RestCollectionHandler::RestCollectionHandler (HttpRequest* request, TRI_vocbase_t* vocbase)
  : RestVocbaseBaseHandler(request, vocbase) {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestCollectionHandler::isDirect () {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

string const& RestCollectionHandler::queue () {
  static string const client = "CLIENT";

  return client;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_e RestCollectionHandler::execute () {

  // extract the sub-request type
  HttpRequest::HttpRequestType type = request->requestType();

  // prepare logging
  static LoggerData::Task const logCreate(DOCUMENT_PATH + " [create]");
  static LoggerData::Task const logRead(DOCUMENT_PATH + " [read]");
  static LoggerData::Task const logUpdate(DOCUMENT_PATH + " [update]");
  static LoggerData::Task const logDelete(DOCUMENT_PATH + " [delete]");
  static LoggerData::Task const logHead(DOCUMENT_PATH + " [head]");
  static LoggerData::Task const logIllegal(DOCUMENT_PATH + " [illegal]");

  LoggerData::Task const * task = &logCreate;

  switch (type) {
    case HttpRequest::HTTP_REQUEST_DELETE: task = &logDelete; break;
    case HttpRequest::HTTP_REQUEST_GET: task = &logRead; break;
    case HttpRequest::HTTP_REQUEST_HEAD: task = &logHead; break;
    case HttpRequest::HTTP_REQUEST_ILLEGAL: task = &logIllegal; break;
    case HttpRequest::HTTP_REQUEST_POST: task = &logCreate; break;
    case HttpRequest::HTTP_REQUEST_PUT: task = &logUpdate; break;
  }

  _timing << *task;
  LOGGER_REQUEST_IN_START_I(_timing);

  // execute one of the CRUD methods
  bool res = false;

  if (request->suffix().size() < 1) {
    generateError(HttpResponse::BAD, "missing collection identifier");
  }
  else {
    switch (type) {
      case HttpRequest::HTTP_REQUEST_DELETE: res = deleteDocument(); break;
      case HttpRequest::HTTP_REQUEST_GET: res = readDocument(); break;
      case HttpRequest::HTTP_REQUEST_HEAD: res = checkDocument(); break;
      case HttpRequest::HTTP_REQUEST_POST: res = createDocument(); break;
      case HttpRequest::HTTP_REQUEST_PUT: res = updateDocument(); break;

      case HttpRequest::HTTP_REQUEST_ILLEGAL:
        res = false;
        generateNotImplemented("ILLEGAL " + DOCUMENT_PATH);
        break;
    }
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
/// @REST{POST /collection/@FA{collection-identifier}}
///
/// Creates a new document in the collection identified by the
/// @FA{collection-identifier}.  A JSON representation of the document must be
/// passed as the body of the POST request.
///
/// If the document was created successfully, then a @LIT{HTTP 201} is returned
/// and the "Location" header contains the path to the newly created
/// document. The "ETag" header field contains the revision of the newly created
/// document.
///
/// If the collection parameter @LIT{waitForSync} is @LIT{false}, then a
/// @LIT{HTTP 202} is returned in order to indicate that the document has been
/// accepted but not yet stored.
///
/// If the @FA{collection-identifier} is unknown, then a @LIT{HTTP 404} is
/// returned.
///
/// If the body does not contain a valid JSON representation of an document,
/// then a @LIT{HTTP 400} is returned.
///
/// Instead of a @FA{collection-identifier}, a collection name can be
/// given.
///
/// @EXAMPLES
///
/// Create a document given a collection identifier:
///
/// @verbinclude rest3
///
/// Unknown collection identifier:
///
/// @verbinclude rest4
///
/// Illegal document:
///
/// @verbinclude rest5
///
/// Create a document given a collection name:
///
/// @verbinclude rest6
////////////////////////////////////////////////////////////////////////////////

bool RestCollectionHandler::createDocument () {
  vector<string> const& suffix = request->suffix();

  if (suffix.size() != 1) {
    generateError(HttpResponse::BAD, "superfluous identifier");
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

  bool waitForSync = _documentCollection->base._waitForSync;

  // note: unlocked is performed by createJson()
  TRI_doc_mptr_t const* mptr = _documentCollection->createJson(_documentCollection, TRI_DOC_MARKER_DOCUMENT, json, 0, true);
  TRI_voc_did_t did = 0;
  TRI_voc_rid_t rid = 0;

  if (mptr != 0) {
    did = mptr->_did;
    rid = mptr->_rid;
  }

  // .............................................................................
  // outside write transaction
  // .............................................................................

  TRI_FreeJson(json);

  if (mptr != 0) {
    if (waitForSync) {
      generateCreated(_documentCollection->base._cid, did, rid);
    }
    else {
      generateAccepted(_documentCollection->base._cid, did, rid);
    }

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
/// @brief reads a single or all documents
///
/// Either readSingleDocument or readAllDocuments.
////////////////////////////////////////////////////////////////////////////////

bool RestCollectionHandler::readDocument () {
  switch (request->suffix().size()) {
    case 1:
      return readAllDocuments();

    case 2:
      return readSingleDocument(true);

    default:
      generateError(HttpResponse::BAD, "superfluous identifier");
      return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a single document
///
/// @REST{GET /collection/@FA{collection-identifier}/@FA{document-identifier}}
///
/// Returns the document identified by @FA{document-identifier} from the
/// collection identified by @FA{collection-identifier}.
///
/// If the document exists, then a @LIT{HTTP 200} is returned and the JSON
/// representation of the document is the body of the response.
///
/// If the collection identifier points to a non-existing collection, then a
/// @LIT{HTTP 404} is returned and the body contains an error document.
///
/// If the document identifier points to a non-existing document, then a
/// @LIT{HTTP 404} is returned and the body contains an error document.
///
/// Instead of a @FA{document-identifier}, a document reference can be given. A
/// @LIT{HTTP 400} is returned, if there is a mismatch between the
/// @FA{collection-identifier} and the @FA{document-reference}.
///
/// Instead of a @FA{collection-identifier}, a collection name can be given.
///
/// @EXAMPLES
///
/// Use a collection and document identfier:
///
/// @verbinclude rest1
///
/// Use a collection name and document reference:
///
/// @verbinclude rest18
///
/// Unknown document identifier:
///
/// @verbinclude rest2
///
/// Unknown collection identifier:
///
/// @verbinclude rest17
////////////////////////////////////////////////////////////////////////////////

bool RestCollectionHandler::readSingleDocument (bool generateBody) {
  vector<string> const& suffix = request->suffix();

  // find and load collection given by name oder identifier
  bool ok = findCollection(suffix[0]) && loadCollection();

  if (! ok) {
    return false;
  }

  // split the document reference
  string did;
  ok = splitDocumentReference(suffix[1], did);

  if (! ok) {
    return false;
  }

  // .............................................................................
  // inside read transaction
  // .............................................................................

  _documentCollection->beginRead(_documentCollection);

  TRI_doc_mptr_t const* document = findDocument(did);

  _documentCollection->endRead(_documentCollection);

  // .............................................................................
  // outside read transaction
  // .............................................................................

  if (document == 0) {
    generateDocumentNotFound(suffix[0], did);
    return false;
  }

  generateDocument(document, generateBody);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads all documents
///
/// @REST{GET /collection/@FA{collection-identifier}}
///
/// Returns the URI for all documents from the collection identified by
/// @FA{collection-identifier}.
///
/// Instead of a @FA{collection-identifier}, a collection name can be given.
///
/// @EXAMPLES
///
/// @verbinclude rest20
////////////////////////////////////////////////////////////////////////////////

bool RestCollectionHandler::readAllDocuments () {
  vector<string> const& suffix = request->suffix();

  // find and load collection given by name oder identifier
  bool ok = findCollection(suffix[0]) && loadCollection();

  if (! ok) {
    return false;
  }

  // .............................................................................
  // inside read transaction
  // .............................................................................

  vector<TRI_voc_did_t> ids;

  _documentCollection->beginRead(_documentCollection);

  try {
    TRI_sim_collection_t* collection = (TRI_sim_collection_t*) _documentCollection;

    if (0 < collection->_primaryIndex._nrUsed) {
      void** ptr = collection->_primaryIndex._table;
      void** end = collection->_primaryIndex._table + collection->_primaryIndex._nrAlloc;

      for (;  ptr < end;  ++ptr) {
        if (*ptr) {
          TRI_doc_mptr_t const* d = (TRI_doc_mptr_t const*) *ptr;

          if (d->_deletion == 0) {
            ids.push_back(d->_did);
          }
        }
      }
    }
  }
  catch (...) {
    // necessary so we will always remove the read lock
  }
  
  _documentCollection->endRead(_documentCollection);

  // .............................................................................
  // outside read transaction
  // .............................................................................

  TRI_string_buffer_t buffer;

  TRI_InitStringBuffer(&buffer);

  TRI_AppendStringStringBuffer(&buffer, "{ \"documents\" : [\n");

  bool first = true;
  string prefix = "\"" + DOCUMENT_PATH + "/" + StringUtils::itoa(_documentCollection->base._cid) + "/";

  for (vector<TRI_voc_did_t>::iterator i = ids.begin();  i != ids.end();  ++i) {
    TRI_AppendString2StringBuffer(&buffer, prefix.c_str(), prefix.size());
    TRI_AppendUInt64StringBuffer(&buffer, *i);

    if (first) {
      prefix = "\",\n" + prefix;
      first = false;
    }
  }

  TRI_AppendStringStringBuffer(&buffer, "\"\n] }\n");

  // and generate a response
  response = new HttpResponse(HttpResponse::OK);
  response->setContentType("application/json");

  response->body().appendText(TRI_BeginStringBuffer(&buffer), TRI_LengthStringBuffer(&buffer));

  TRI_DestroyStringBuffer(&buffer);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a single document head
///
/// @REST{HEAD /collection/@FA{collection-identifier}/@FA{document-identifier}}
///
/// Like @FN{GET}, but does not return the body.
///
/// @EXAMPLES
///
/// @verbinclude rest19
////////////////////////////////////////////////////////////////////////////////

bool RestCollectionHandler::checkDocument () {
  return readSingleDocument(false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document
///
/// @REST{PUT /collection/@FA{collection-identifier}/@FA{document-identifier}}
///
/// Updates the document identified by @FA{document-identifier} in the
/// collection identified by @FA{collection-identifier}. If the document exists
/// and could be updated, then a @LIT{HTTP 201} is returned and the "ETag"
/// header field contains the new revision of the document.
///
/// If the document does not exists, then a @LIT{HTTP 404} is returned.
///
/// If an etag is supplied in the "ETag" field, then the AvocadoDB checks that
/// the revision of the document is equal to the etag. If there is a mismatch,
/// then a @LIT{HTTP 409} conflict is returned and no update is performed.
///
/// Instead of a @FA{document-identifier}, a document reference can be given.
///
/// Instead of a @FA{collection-identifier}, a collection name can be given.
///
/// @REST{PUT /collection/@FA{collection-identifier}/@FA{document-identifier}?policy=@FA{policy}}
///
/// As before, if @FA{policy} is @LIT{error}. If @FA{policy} is @LIT{last},
/// then the last write will win.
///
/// @REST{PUT /collection/@FA{collection-identifier}/@FA{document-identifier}?_rev=@FA{etag}}
///
/// You can also supply the etag using the parameter "_rev" instead of an "ETag"
/// header.
///
/// @EXAMPLES
///
/// Using collection and document identifier:
///
/// @verbinclude rest7
///
/// Unknown document identifier:
///
/// @verbinclude rest8
///
/// Produce a revision conflict:
///
/// @verbinclude rest9
///
/// Last write wins:
///
/// @verbinclude rest10
///
/// Alternative to ETag header field:
///
/// @verbinclude rest11
////////////////////////////////////////////////////////////////////////////////

bool RestCollectionHandler::updateDocument () {
  vector<string> const& suffix = request->suffix();

  // find and load collection given by name oder identifier
  bool ok = findCollection(suffix[0]) && loadCollection();

  if (! ok) {
    return false;
  }

  // split the document reference
  string didStr;
  ok = splitDocumentReference(suffix[1], didStr);

  if (! ok) {
    return false;
  }

  // parse document
  TRI_json_t* json = parseJsonBody();

  if (json == 0) {
    return false;
  }

  // extract document identifier
  TRI_voc_did_t did = StringUtils::uint64(didStr);

  // extract the revision
  TRI_voc_rid_t revision = extractRevision();

  // extract or chose the update policy
  TRI_doc_update_policy_e policy = extractUpdatePolicy();

  // .............................................................................
  // inside write transaction
  // .............................................................................

  _documentCollection->beginWrite(_documentCollection);

  // unlocking is performed in updateJson()
  TRI_doc_mptr_t const* mptr = _documentCollection->updateJson(_documentCollection, json, did, revision, policy, true);
  TRI_voc_rid_t rid = 0;

  if (mptr != 0) {
    rid = mptr->_rid;
  }

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
      generateDocumentNotFound(suffix[0], didStr);
      return false;
    }
    else if (TRI_errno() == TRI_VOC_ERROR_CONFLICT) {
      generateConflict(suffix[0], didStr);
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
/// @REST{DELETE /collection/@FA{collection-identifier}/@FA{document-identifier}}
///
/// Deletes the document identified by @FA{document-identifier} from the
/// collection identified by @FA{collection-identifier}. If the document exists
/// and could be deleted, then a @LIT{HTTP 204} is returned.
///
/// If the document does not exists, then a @LIT{HTTP 404} is returned.
///
/// If an etag is supplied in the "ETag" field, then the AvocadoDB checks that
/// the revision of the document is equal to the etag. If there is a mismatch,
/// then a @LIT{HTTP 409} conflict is returned and no delete is performed.
///
/// Instead of a @FA{document-identifier}, a document reference can be given.
///
/// Instead of a @FA{collection-identifier}, a collection name can be given.
///
/// @REST{DELETE /collection/@FA{collection-identifier}/@FA{document-identifier}?policy=@FA{policy}}
///
/// As before, if @FA{policy} is @LIT{error}. If @FA{policy} is @LIT{last}, then
/// the last write will win.
///
/// @REST{DELETE /collection/@FA{collection-identifier}/@FA{document-identifier}? _rev=@FA{etag}}
///
/// You can also supply the etag using the parameter "_rev" instead of an "ETag"
/// header.
///
/// @EXAMPLES
///
/// Using collection and document identifier:
///
/// @verbinclude rest13
///
/// Unknown document identifier:
///
/// @verbinclude rest14
///
/// Revision conflict:
///
/// @verbinclude rest12
////////////////////////////////////////////////////////////////////////////////

bool RestCollectionHandler::deleteDocument () {
  vector<string> suffix = request->suffix();

  // find and load collection given by name oder identifier
  bool ok = findCollection(suffix[0]) && loadCollection();

  if (! ok) {
    return false;
  }

  // split the document reference
  string didStr;
  ok = splitDocumentReference(suffix[1], didStr);

  if (! ok) {
    return false;
  }

  // extract document identifier
  TRI_voc_did_t did = StringUtils::uint64(didStr);

  // extract the revision
  TRI_voc_rid_t revision = extractRevision();

  // extract or chose the update policy
  TRI_doc_update_policy_e policy = extractUpdatePolicy();

  // .............................................................................
  // inside write transaction
  // .............................................................................

  _documentCollection->beginWrite(_documentCollection);

  // unlocking is performed in destroy()
  ok = _documentCollection->destroy(_documentCollection, did, revision, policy, true);

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
      generateDocumentNotFound(suffix[0], didStr);
      return false;
    }
    else if (TRI_errno() == TRI_VOC_ERROR_CONFLICT) {
      generateConflict(suffix[0], didStr);
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
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
