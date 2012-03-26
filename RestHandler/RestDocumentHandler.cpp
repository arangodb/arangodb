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

#include "RestDocumentHandler.h"

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

RestDocumentHandler::RestDocumentHandler (HttpRequest* request, TRI_vocbase_t* vocbase)
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

bool RestDocumentHandler::isDirect () {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

string const& RestDocumentHandler::queue () {
  static string const client = "CLIENT";

  return client;
}

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
/// @REST{POST /document?collection=@FA{collection-identifier}}
///
/// Creates a new document in the collection identified by the
/// @FA{collection-identifier}.  A JSON representation of the document must be
/// passed as the body of the POST request.
///
/// If the document was created successfully, then a @LIT{HTTP 201} is returned
/// and the "Location" header contains the path to the newly created
/// document. The "ETag" header field contains the revision of the document.
///
/// The body of the response contains a JSON object with the same information.
/// The attribute @LIT{_id} contains the document handle of the newly created
/// document, the attribute @LIT{_rev} contains the document revision.
///
/// If the collection parameter @LIT{waitForSync} is @LIT{false}, then a
/// @LIT{HTTP 202} is returned in order to indicate that the document has been
/// accepted but not yet stored.
///
/// If the @FA{collection-identifier} is unknown, then a @LIT{HTTP 404} is
/// returned and the body of the response contains an error document.
///
/// If the body does not contain a valid JSON representation of an document,
/// then a @LIT{HTTP 400} is returned and the body of the response contains
/// an error document.
///
/// @REST{POST /document?collection=@FA{collection-name}&createCollection=@FA{create}}
///
/// Instead of a @FA{collection-identifier}, a @FA{collection-name} can be
/// used. If @FA{createCollection} is true, then the collection is created if it does not
/// exists.
///
/// @EXAMPLES
///
/// Create a document given a collection identifier @LIT{161039} for the collection
/// named @LIT{demo}. Note that the revision identifier might or might by equal to
/// the last part of the document handle. It generally will be equal, but there is
/// no guaranty.
///
/// @verbinclude rest_create-document
///
/// Create a document in a collection where @LIT{waitForSync} is @LIT{false}.
///
/// @verbinclude rest_create-document-accept
///
/// Create a document in a known, named collection
///
/// @verbinclude rest_create-document-named-collection
///
/// Create a document in a new, named collection
///
/// @verbinclude rest_create-document-new-named-collection
///
/// Unknown collection identifier:
///
/// @verbinclude rest_create-document-unknown-cid
///
/// Illegal document:
///
/// @verbinclude rest_create-document-bad-json
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::createDocument () {
  vector<string> const& suffix = request->suffix();

  if (suffix.size() != 0) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "superfluous suffix, expecting " + DOCUMENT_PATH + "?collection=<identifier>");
    return false;
  }

  // extract the cid
  bool found;
  string collection = request->value("collection", found);

  if (! found || collection.empty()) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_AVOCADO_COLLECTION_PARAMETER_MISSING,
                  "'collection' is missing, expecting " + DOCUMENT_PATH + "?collection=<identifier>");
    return false;
  }

  // shall we create the collection?
  string createStr = request->value("createCollection", found);
  bool create = found ? StringUtils::boolean(createStr) : false;

  // parse document
  TRI_json_t* json = parseJsonBody();

  if (json == 0) {
    return false;
  }

  // find and load collection given by name oder identifier
  int res = useCollection(collection, create);

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  // .............................................................................
  // inside write transaction
  // .............................................................................

  _documentCollection->beginWrite(_documentCollection);

  bool waitForSync = _documentCollection->base._waitForSync;
  TRI_voc_cid_t cid = _documentCollection->base._cid;

  // note: unlocked is performed by createJson()
  TRI_doc_mptr_t const mptr = _documentCollection->createJson(_documentCollection, TRI_DOC_MARKER_DOCUMENT, json, 0, true);

  // .............................................................................
  // outside write transaction
  // .............................................................................

  // release collection and free json
  releaseCollection();
  TRI_FreeJson(json);

  // generate result
  if (mptr._did != 0) {
    if (waitForSync) {
      generateCreated(cid, mptr._did, mptr._rid);
    }
    else {
      generateAccepted(cid, mptr._did, mptr._rid);
    }

    return true;
  }
  else {
    if (TRI_errno() == TRI_ERROR_AVOCADO_READ_ONLY) {
      generateError(HttpResponse::FORBIDDEN, 
                    TRI_ERROR_AVOCADO_READ_ONLY,
                    "collection is read-only");
    }
    else {
      generateError(HttpResponse::SERVER_ERROR,
                    TRI_ERROR_INTERNAL,
                    "cannot create, failed with: " + string(TRI_last_error()));
    }

    return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a single or all documents
///
/// Either readSingleDocument or readAllDocuments.
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::readDocument () {
  switch (request->suffix().size()) {
    case 0:
      return readAllDocuments();

    case 2:
      return readSingleDocument(true);

    default:
      generateError(HttpResponse::BAD, 
                    TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                    "expecting GET /document/<document-handle> or GET /document?collection=<collection-identifier>");
      return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a single document
///
/// @REST{GET /document/@FA{document-handle}}
///
/// Returns the document identified by @FA{document-handle}. The returned
/// document contains two special attributes: @LIT{_id} containing the document
/// handle and @LIT{_rev} containing the revision.
///
/// If the document exists, then a @LIT{HTTP 200} is returned and the JSON
/// representation of the document is the body of the response.
///
/// If the @FA{document-handle} points to a non-existing document, then a
/// @LIT{HTTP 404} is returned and the body contains an error document.
///
/// If the "If-None-Match" header is given, then it must contain exactly one
/// etag. The document is returned, if it has a different revision than the
/// given etag. Otherwise a @LIT{HTTP 304} is returned.
///
/// If the "If-Match" header is given, then it must contain exactly one
/// etag. The document is returned, if it has the same revision ad the
/// given etag. Otherwise a @LIT{HTTP 412} is returned.
///
/// @EXAMPLES
///
/// Use a document handle:
///
/// @verbinclude rest_read-document
///
/// Use a document handle and an etag:
///
/// @verbinclude rest_read-document-if-none-match
///
/// Unknown document handle:
///
/// @verbinclude rest_read-document-unknown-handle
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::readSingleDocument (bool generateBody) {
  vector<string> const& suffix = request->suffix();

  /// check for an etag
  TRI_voc_rid_t ifNoneRid = extractRevision("if-none-match");
  TRI_voc_rid_t ifRid = extractRevision("if-match", "rev");

  // split the document reference
  string collection = suffix[0];
  string did = suffix[1];

  // find and load collection given by name oder identifier
  int res = useCollection(collection);

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  // .............................................................................
  // inside read transaction
  // .............................................................................

  _documentCollection->beginRead(_documentCollection);

  TRI_voc_cid_t cid = _documentCollection->base._cid;
  TRI_doc_mptr_t const document = findDocument(did);

  _documentCollection->endRead(_documentCollection);

  // .............................................................................
  // outside read transaction
  // .............................................................................

  // release collection
  releaseCollection();

  // generate result
  if (document._did == 0) {
    generateDocumentNotFound(cid + TRI_DOCUMENT_HANDLE_SEPARATOR_STR +  did);
    return false;
  }

  TRI_voc_rid_t rid = document._rid;

  if (ifNoneRid == 0) {
    if (ifRid == 0) {
      generateDocument(&document, generateBody);
    }
    else if (ifRid == rid) {
      generateDocument(&document, generateBody);
    }
    else {
      generatePreconditionFailed(cid, document._did, rid);
    }
  }
  else if (ifNoneRid == rid) {
    if (ifRid == 0) {
      generateNotModified(StringUtils::itoa(rid));
    }
    else if (ifRid == rid) {
      generateNotModified(StringUtils::itoa(rid));
    }
    else {
      generatePreconditionFailed(cid, document._did, rid);
    }
  }
  else {
    if (ifRid == 0) {
      generateDocument(&document, generateBody);
    }
    else if (ifRid == rid) {
      generateDocument(&document, generateBody);
    }
    else {
      generatePreconditionFailed(cid, document._did, rid);
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads all documents
///
/// @REST{GET /document?collection=@FA{collection-identifier}}
///
/// Returns a list of all URI for all documents from the collection identified
/// by @FA{collection-identifier}.
///
/// Instead of a @FA{collection-identifier}, a collection name can be given.
///
/// @EXAMPLES
///
/// @verbinclude rest_read-document-all
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::readAllDocuments () {

  // extract the cid
  bool found;
  string collection = request->value("collection", found);

  // find and load collection given by name oder identifier
  int res = useCollection(collection);

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  // .............................................................................
  // inside read transaction
  // .............................................................................

  vector<TRI_voc_did_t> ids;

  _documentCollection->beginRead(_documentCollection);

  TRI_voc_cid_t cid = _documentCollection->base._cid;

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

  // release collection
  releaseCollection();

  // generate result
  TRI_string_buffer_t buffer;

  TRI_InitStringBuffer(&buffer);

  TRI_AppendStringStringBuffer(&buffer, "{ \"documents\" : [\n");

  bool first = true;
  string prefix = "\"" + DOCUMENT_PATH + "/" + StringUtils::itoa(cid) + "/";

  for (vector<TRI_voc_did_t>::iterator i = ids.begin();  i != ids.end();  ++i) {
    TRI_AppendString2StringBuffer(&buffer, prefix.c_str(), prefix.size());
    TRI_AppendUInt64StringBuffer(&buffer, *i);
    TRI_AppendCharStringBuffer(&buffer, '"');

    if (first) {
      prefix = ",\n" + prefix;
      first = false;
    }
  }

  TRI_AppendStringStringBuffer(&buffer, "\n] }\n");

  // and generate a response
  response = new HttpResponse(HttpResponse::OK);
  response->setContentType("application/json; charset=utf-8");

  response->body().appendText(TRI_BeginStringBuffer(&buffer), TRI_LengthStringBuffer(&buffer));

  TRI_AnnihilateStringBuffer(&buffer);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a single document head
///
/// @REST{HEAD /document/@FA{document-handle}}
///
/// Like @FN{GET}, but only returns the header fields and not the body. You
/// can use this call to get the current revision of a document or check if
/// the document was deleted.
///
/// @EXAMPLES
///
/// @verbinclude rest_read-document-head
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::checkDocument () {
  vector<string> const& suffix = request->suffix();

  if (suffix.size() != 2) {
    generateError(HttpResponse::BAD, 
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting URI /document/<document-handle>");
    return false;
  }

  return readSingleDocument(false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document
///
/// @REST{PUT /document/@FA{document-handle}}
///
/// Updates the document identified by @FA{document-handle}. If the document exists
/// and can be updated, then a @LIT{HTTP 201} is returned and the "ETag" header
/// field contains the new revision of the document.
///
/// Note the updated document passed in the body of the request normally also
/// contains the @FA{document-handle} in the attribute @LIT{_id} and the
/// revision in @LIT{_rev}. These attributes, however, are ignored. Only the URI
/// and the "ETag" header are relevant in order to avoid confusion when using
/// proxies.
///
/// The body of the response contains a JSON object with the information about
/// the handle and the revision.  The attribute @LIT{_id} contains the known
/// @FA{document-handle} of the updated document, the attribute @LIT{_rev}
/// contains the new document revision.
///
/// If the document does not exists, then a @LIT{HTTP 404} is returned and the
/// body of the response contains an error document.
///
/// If an etag is supplied in the "If-Match" header field, then the AvocadoDB
/// checks that the revision of the document is equal to the etag. If there is a
/// mismatch, then a @LIT{HTTP 409} conflict is returned and no update is
/// performed.
///
/// @REST{PUT /document/@FA{document-handle}?policy=@FA{policy}}
///
/// As before, if @FA{policy} is @LIT{error}. If @FA{policy} is @LIT{last},
/// then the last write will win.
///
/// @REST{PUT /document/@FA{collection-identifier}/@FA{document-identifier}?rev=@FA{etag}}
///
/// You can also supply the etag using the parameter @LIT{rev} instead of an "ETag"
/// header. You must never supply both the "ETag" header and the @LIT{rev}
/// parameter.
///
/// @EXAMPLES
///
/// Using document handle:
///
/// @verbinclude rest_update-document
///
/// Unknown document handle:
///
/// @verbinclude rest_update-document-unknown-handle
///
/// Produce a revision conflict:
///
/// @verbinclude rest_update-document-if-match-other
///
/// Last write wins:
///
/// @verbinclude rest_update-document-if-match-other-last-write
///
/// Alternative to header field:
///
/// @verbinclude rest_update-document-rev-other
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::updateDocument () {
  vector<string> const& suffix = request->suffix();

  if (suffix.size() != 2) {
    generateError(HttpResponse::BAD, 
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting UPDATE /document/<document-handle>");
    return false;
  }

  // split the document reference
  string collection = suffix[0];
  string didStr = suffix[1];

  // parse document
  TRI_json_t* json = parseJsonBody();

  if (json == 0) {
    return false;
  }

  // extract document identifier
  TRI_voc_did_t did = StringUtils::uint64(didStr);

  // extract the revision
  TRI_voc_rid_t revision = extractRevision("if-match", "rev");

  // extract or chose the update policy
  TRI_doc_update_policy_e policy = extractUpdatePolicy();

  // find and load collection given by name oder identifier
  int res = useCollection(collection);

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  // .............................................................................
  // inside write transaction
  // .............................................................................

  _documentCollection->beginWrite(_documentCollection);

  // unlocking is performed in updateJson()
  TRI_voc_rid_t rid = 0;
  TRI_voc_cid_t cid = _documentCollection->base._cid;

  TRI_doc_mptr_t const mptr = _documentCollection->updateJson(_documentCollection, json, did, revision, &rid, policy, true);
  int en = mptr._did == 0 ? TRI_errno() : 0;

  // .............................................................................
  // outside write transaction
  // .............................................................................

  // release collection
  releaseCollection();

  // generate result
  if (mptr._did != 0) {
    generateUpdated(cid, did, mptr._rid);
    return true;
  }
  else {
    switch (en) {
      case TRI_ERROR_AVOCADO_READ_ONLY:
        generateError(HttpResponse::FORBIDDEN, 
                      TRI_ERROR_AVOCADO_READ_ONLY,
                      "collection is read-only");
        return false;

      case TRI_ERROR_AVOCADO_DOCUMENT_NOT_FOUND:
        generateDocumentNotFound(cid + TRI_DOCUMENT_HANDLE_SEPARATOR_STR + didStr);
        return false;

      case TRI_ERROR_AVOCADO_CONFLICT:
        generatePreconditionFailed(_documentCollection->base._cid, did, rid);
        return false;

      default:
        generateError(HttpResponse::SERVER_ERROR,
                      TRI_ERROR_INTERNAL,
                      "cannot update, failed with " + string(TRI_last_error()));
        return false;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document
///
/// @REST{DELETE /document/@FA{document-handle}}
///
/// Deletes the document identified by @FA{document-handle}. If the document
/// exists and could be deleted, then a @LIT{HTTP 204} is returned.
///
/// The body of the response contains a JSON object with the information about
/// the handle and the revision.  The attribute @LIT{_id} contains the known
/// @FA{document-handle} of the updated document, the attribute @LIT{_rev}
/// contains the known document revision.
///
/// If the document does not exists, then a @LIT{HTTP 404} is returned and the
/// body of the response contains an error document.
///
/// If an etag is supplied in the "If-Match" field, then the AvocadoDB checks
/// that the revision of the document is equal to the etag. If there is a
/// mismatch, then a @LIT{HTTP 412} conflict is returned and no delete is
/// performed.
///
/// @REST{DELETE /document/@FA{document-handle}?policy=@FA{policy}}
///
/// As before, if @FA{policy} is @LIT{error}. If @FA{policy} is @LIT{last}, then
/// the last write will win.
///
/// @REST{DELETE /document/@FA{document-handle}?rev=@FA{etag}}
///
/// You can also supply the etag using the parameter @LIT{rev} instead of an
/// "If-Match" header. You must never supply both the "If-Match" header and the
/// @LIT{rev} parameter.
///
/// @EXAMPLES
///
/// Using document handle:
///
/// @verbinclude rest_delete-document
///
/// Unknown document handle:
///
/// @verbinclude rest_delete-document-unknown-handle
///
/// Revision conflict:
///
/// @verbinclude rest_delete-document-if-match-other
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::deleteDocument () {
  vector<string> const& suffix = request->suffix();

  if (suffix.size() != 2) {
    generateError(HttpResponse::BAD, 
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting DELETE /document/<document-handle>");
    return false;
  }

  // split the document reference
  string collection = suffix[0];
  string didStr = suffix[1];

  // extract document identifier
  TRI_voc_did_t did = StringUtils::uint64(didStr);

  // extract the revision
  TRI_voc_rid_t revision = extractRevision("if-match", "rev");

  // extract or chose the update policy
  TRI_doc_update_policy_e policy = extractUpdatePolicy();

  if (policy == TRI_DOC_UPDATE_ILLEGAL) {
    generateError(HttpResponse::BAD, 
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "policy must be 'error' or 'last'");
    return false;
  }

  // find and load collection given by name oder identifier
  int res = useCollection(collection);

  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }

  // .............................................................................
  // inside write transaction
  // .............................................................................

  _documentCollection->beginWrite(_documentCollection);

  TRI_voc_rid_t rid = 0;
  TRI_voc_cid_t cid = _documentCollection->base._cid;

  // unlocking is performed in destroy()
  res = _documentCollection->destroy(_documentCollection, did, revision, &rid, policy, true);

  // .............................................................................
  // outside write transaction
  // .............................................................................

  // release collection
  releaseCollection();

  // generate result
  if (res == TRI_ERROR_NO_ERROR) {
    generateDeleted(cid, did, rid);
    return true;
  }
  else {
    switch (res) {
      case TRI_ERROR_AVOCADO_READ_ONLY:
        generateError(HttpResponse::FORBIDDEN, 
                      TRI_ERROR_AVOCADO_READ_ONLY,
                      "collection is read-only");
        return false;

      case TRI_ERROR_AVOCADO_DOCUMENT_NOT_FOUND:
        generateDocumentNotFound(cid + TRI_DOCUMENT_HANDLE_SEPARATOR_STR + didStr);
        return false;

      case TRI_ERROR_AVOCADO_CONFLICT:
        generatePreconditionFailed(_documentCollection->base._cid, did, rid);
        return false;

      default:
        generateError(HttpResponse::SERVER_ERROR, 
                      TRI_ERROR_INTERNAL,
                      "cannot delete, failed with " + string(TRI_last_error()));
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
