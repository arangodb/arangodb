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
#include "BasicsC/conversions.h"
#include "BasicsC/json.h"
#include "BasicsC/json-utilities.h"
#include "BasicsC/string-buffer.h"
#include "Rest/HttpRequest.h"
#include "Rest/JsonContainer.h"
#include "VocBase/document-collection.h"
#include "VocBase/vocbase.h"
#include "Utils/Barrier.h"

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
/// @addtogroup ArangoDB
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
  static string const client = "STANDARD";

  return client;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_e RestDocumentHandler::execute () {

  // extract the sub-request type
  HttpRequest::HttpRequestType type = _request->requestType();

  // prepare logging
  static LoggerData::Task const logCreate(DOCUMENT_PATH + " [create]");
  static LoggerData::Task const logRead(DOCUMENT_PATH + " [read]");
  static LoggerData::Task const logUpdate(DOCUMENT_PATH + " [update]");
  static LoggerData::Task const logDelete(DOCUMENT_PATH + " [delete]");
  static LoggerData::Task const logHead(DOCUMENT_PATH + " [head]");
  static LoggerData::Task const logPatch(DOCUMENT_PATH + " [patch]");
  static LoggerData::Task const logIllegal(DOCUMENT_PATH + " [illegal]");

  LoggerData::Task const * task = &logCreate;

  switch (type) {
    case HttpRequest::HTTP_REQUEST_DELETE: task = &logDelete; break;
    case HttpRequest::HTTP_REQUEST_GET: task = &logRead; break;
    case HttpRequest::HTTP_REQUEST_HEAD: task = &logHead; break;
    case HttpRequest::HTTP_REQUEST_ILLEGAL: task = &logIllegal; break;
    case HttpRequest::HTTP_REQUEST_POST: task = &logCreate; break;
    case HttpRequest::HTTP_REQUEST_PUT: task = &logUpdate; break;
    case HttpRequest::HTTP_REQUEST_PATCH: task = &logUpdate; break;
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
    case HttpRequest::HTTP_REQUEST_PUT: res = replaceDocument(); break;
    case HttpRequest::HTTP_REQUEST_PATCH: res = updateDocument(); break;

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
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a document
///
/// @RESTHEADER{POST /_api/document,creates a document}
///
/// @REST{POST /_api/document?collection=@FA{collection-identifier}}
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
/// @REST{POST /_api/document?collection=@FA{collection-name}@LATEXBREAK&createCollection=@FA{create-flag}}
///
/// Instead of a @FA{collection-identifier}, a @FA{collection-name} can be
/// used. If @FA{create-flag} has a value of @LIT{true} or @LIT{yes}, then the 
/// collection is created if it does not yet exist. Other values for @FA{create-flag}
/// will be ignored so the collection must be present for the operation to succeed.
///
/// @EXAMPLES
///
/// Create a document given a collection identifier @LIT{161039} for the collection
/// named @LIT{demo}. Note that the revision identifier might or might by equal to
/// the last part of the document handle. It generally will be equal, but there is
/// no guaranty.
///
/// @EXAMPLE{rest-create-document,create a document}
///
/// Create a document in a collection where @LIT{waitForSync} is @LIT{false}.
///
/// @EXAMPLE{rest-create-document-accept,accept a document}
///
/// Create a document in a known, named collection
///
/// @verbinclude rest-create-document-named-collection
///
/// Create a document in a new, named collection
///
/// @verbinclude rest-create-document-create-collection
///
/// Unknown collection identifier:
///
/// @verbinclude rest-create-document-unknown-cid
///
/// Illegal document:
///
/// @verbinclude rest-create-document-bad-json
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::createDocument () {
  vector<string> const& suffix = _request->suffix();

  if (suffix.size() != 0) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "superfluous suffix, expecting " + DOCUMENT_PATH + "?collection=<identifier>");
    return false;
  }

  // extract the cid
  bool found;
  char const* collection = _request->value("collection", found);

  if (! found || *collection == '\0') {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,
                  "'collection' is missing, expecting " + DOCUMENT_PATH + "?collection=<identifier>");
    return false;
  }

  // shall we create the collection?
  char const* valueStr = _request->value("createCollection", found);
  bool create = found ? StringUtils::boolean(valueStr) : false;
  
  // shall we reuse document and revision id?
  valueStr = _request->value("useId", found);
  bool reuseId = found ? StringUtils::boolean(valueStr) : false;

  // auto-ptr that will free JSON data when scope is left
  JsonContainer container(TRI_UNKNOWN_MEM_ZONE, parseJsonBody());
  TRI_json_t* json = container.ptr();

  if (json == 0) {
    return false;
  }

  // find and load collection given by name or identifier
  CollectionAccessor ca(_vocbase, collection, getCollectionType(), create);

  int res = ca.use();
  if (TRI_ERROR_NO_ERROR != res) {
    generateCollectionError(collection, res);
    return false;
  }
  
  TRI_voc_cid_t cid = ca.cid();
  const bool waitForSync = ca.waitForSync();

  // .............................................................................
  // inside write transaction
  // .............................................................................

  WriteTransaction trx(&ca);

  TRI_doc_mptr_t const mptr = trx.primary()->createJson(trx.primary(), TRI_DOC_MARKER_DOCUMENT, json, 0, reuseId, false);

  trx.end();

  // .............................................................................
  // outside write transaction
  // .............................................................................

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
    int res = TRI_errno();

    switch (res) {
      case TRI_ERROR_ARANGO_READ_ONLY:
        generateError(HttpResponse::FORBIDDEN, res, "collection is read-only");
        return false;

      case TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED:
        generateError(HttpResponse::CONFLICT, res, "cannot create document, unique constraint violated");
        return false;

      case TRI_ERROR_ARANGO_GEO_INDEX_VIOLATED:
        generateError(HttpResponse::BAD, res, "geo constraint violated");
        return false;

      default:
        generateError(HttpResponse::SERVER_ERROR,
                      TRI_ERROR_INTERNAL,
                      "cannot create, failed with: " + string(TRI_last_error()));
        return false;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a single or all documents
///
/// Either readSingleDocument or readAllDocuments.
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::readDocument () {
  size_t len = _request->suffix().size();

  switch (len) {
    case 0:
      return readAllDocuments();

    case 1:
      generateError(HttpResponse::BAD, 
                    TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD,
                    "expecting GET /_api/document/<document-handle>");
      return false;

    case 2:
      return readSingleDocument(true);

    default:
      generateError(HttpResponse::BAD, 
                    TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                    "expecting GET /_api/document/<document-handle> or GET /_api/document?collection=<collection-identifier>");
      return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a single document
///
/// @RESTHEADER{GET /_api/document,reads a document}
///
/// @REST{GET /_api/document/@FA{document-handle}}
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
/// given etag. Otherwise a @LIT{HTTP 412} is returned. As an alternative
/// you can supply the etag in an attribute @LIT{rev} in the URL.
///
/// @EXAMPLES
///
/// Use a document handle:
///
/// @verbinclude rest-read-document
///
/// Use a document handle and an etag:
///
/// @verbinclude rest-read-document-if-none-match
///
/// Unknown document handle:
///
/// @verbinclude rest-read-document-unknown-handle
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::readSingleDocument (bool generateBody) {
  vector<string> const& suffix = _request->suffix();

  /// check for an etag
  TRI_voc_rid_t ifNoneRid = extractRevision("if-none-match", 0);
  TRI_voc_rid_t ifRid = extractRevision("if-match", "rev");

  // split the document reference
  string collection = suffix[0];
  string did = suffix[1];

  // find and load collection given by name or identifier
  CollectionAccessor ca(_vocbase, collection, getCollectionType(), false);
  
  int res = ca.use();
  if (TRI_ERROR_NO_ERROR != res) {
    generateCollectionError(collection, res);
    return false;
  }
  
  TRI_voc_cid_t cid = ca.cid();
  TRI_shaper_t* shaper = ca.shaper();
  TRI_voc_did_t id = StringUtils::uint64(did);

  // .............................................................................
  // inside read transaction
  // .............................................................................

  ReadTransaction trx(&ca);

  TRI_doc_mptr_t const document = trx.primary()->read(trx.primary(), id);

  // register a barrier. will be destroyed automatically
  Barrier barrier(trx.primary());

  trx.end();

  // .............................................................................
  // outside read transaction
  // .............................................................................

  // generate result
  if (document._did == 0) {
    generateDocumentNotFound(cid, did);
    return false;
  }

  TRI_voc_rid_t rid = document._rid;

  if (ifNoneRid == 0) {
    if (ifRid == 0 || ifRid == rid) {
      generateDocument(&document, cid, shaper, generateBody);
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
    if (ifRid == 0 || ifRid == rid) {
      generateDocument(&document, cid, shaper, generateBody);
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
/// @RESTHEADER{GET /_api/document,reads all document}
///
/// @REST{GET /_api/document?collection=@FA{collection-identifier}}
///
/// Returns a list of all URI for all documents from the collection identified
/// by @FA{collection-identifier}.
///
/// Instead of a @FA{collection-identifier}, a collection name can be given.
///
/// @EXAMPLES
///
/// @verbinclude rest-read-document-all
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::readAllDocuments () {
  // extract the cid
  bool found;
  string collection = _request->value("collection", found);

  // find and load collection given by name or identifier
  CollectionAccessor ca(_vocbase, collection, getCollectionType(), false);
  
  int res = ca.use();
  if (TRI_ERROR_NO_ERROR != res) {
    generateCollectionError(collection, res);
    return false;
  }

  vector<TRI_voc_did_t> ids;
  TRI_voc_cid_t cid = ca.cid();

  // .............................................................................
  // inside read transaction
  // .............................................................................

  ReadTransaction trx(&ca);

  const TRI_primary_collection_t* primary = trx.primary();

  if (0 < primary->_primaryIndex._nrUsed) {
    void** ptr = primary->_primaryIndex._table;
    void** end = ptr + primary->_primaryIndex._nrAlloc;

    for (;  ptr < end;  ++ptr) {
      if (*ptr) {
        TRI_doc_mptr_t const* d = (TRI_doc_mptr_t const*) *ptr;

        if (d->_deletion == 0) {
          ids.push_back(d->_did);
        }
      }
    }
  }

  trx.end();

  // .............................................................................
  // outside read transaction
  // .............................................................................

  // generate result
  TRI_string_buffer_t buffer;

  TRI_InitStringBuffer(&buffer, TRI_UNKNOWN_MEM_ZONE);

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
  _response = new HttpResponse(HttpResponse::OK);
  _response->setContentType("application/json; charset=utf-8");

  _response->body().appendText(TRI_BeginStringBuffer(&buffer), TRI_LengthStringBuffer(&buffer));

  TRI_AnnihilateStringBuffer(&buffer);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a single document head
///
/// @RESTHEADER{HEAD /_api/document,reads a document header}
///
/// @REST{HEAD /_api/document/@FA{document-handle}}
///
/// Like @FN{GET}, but only returns the header fields and not the body. You
/// can use this call to get the current revision of a document or check if
/// the document was deleted.
///
/// @EXAMPLES
///
/// @verbinclude rest-read-document-head
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::checkDocument () {
  vector<string> const& suffix = _request->suffix();

  if (suffix.size() != 2) {
    generateError(HttpResponse::BAD, 
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting URI /_api/document/<document-handle>");
    return false;
  }

  return readSingleDocument(false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces a document
///
/// @RESTHEADER{PUT /_api/document,updates a document}
///
/// @REST{PUT /_api/document/@FA{document-handle}}
///
/// Completely updates (i.e. replaces) the document identified by @FA{document-handle}. 
/// If the document exists and can be updated, then a @LIT{HTTP 201} is returned 
/// and the "ETag" header field contains the new revision of the document.
///
/// If the new document passed in the body of the request contains the
/// @FA{document-handle} in the attribute @LIT{_id} and the revision in @LIT{_rev},
/// these attributes will be ignored. Only the URI and the "ETag" header are 
/// relevant in order to avoid confusion when using proxies.
///
/// The body of the response contains a JSON object with the information about
/// the handle and the revision.  The attribute @LIT{_id} contains the known
/// @FA{document-handle} of the updated document, the attribute @LIT{_rev}
/// contains the new document revision.
///
/// If the document does not exist, then a @LIT{HTTP 404} is returned and the
/// body of the response contains an error document.
///
/// If an etag is supplied in the "If-Match" header field, then the ArangoDB
/// checks that the revision of the document is equal to the etag. If there is a
/// mismatch, then a @LIT{HTTP 409} conflict is returned and no update is
/// performed.
///
/// @REST{PUT /_api/document/@FA{document-handle}?policy=@FA{policy}}
///
/// As before, if @FA{policy} is @LIT{error}. If @FA{policy} is @LIT{last},
/// then the last write will win.
///
/// @REST{PUT /_api/document/@FA{collection-identifier}/@FA{document-identifier}?rev=@FA{etag}}
///
/// You can also supply the etag using the parameter @LIT{rev} instead of an "ETag"
/// header. You must never supply both the "ETag" header and the @LIT{rev}
/// parameter.
///
/// @EXAMPLES
///
/// Using document handle:
///
/// @verbinclude rest-update-document
///
/// Unknown document handle:
///
/// @verbinclude rest-update-document-unknown-handle
///
/// Produce a revision conflict:
///
/// @verbinclude rest-update-document-if-match-other
///
/// Last write wins:
///
/// @verbinclude rest-update-document-if-match-other-last-write
///
/// Alternative to header field:
///
/// @verbinclude rest-update-document-rev-other
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::replaceDocument () {
  return modifyDocument(false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document
///
/// @RESTHEADER{PATCH /_api/document,patches a document}
///
/// @REST{PATCH /_api/document/@FA{document-handle}}
///
/// Partially updates the document identified by @FA{document-handle}. 
/// The body of the request must contain a JSON document with the attributes
/// to patch (the patch document). All attributes from the patch document will
/// be added to the existing document if they do not yet exist, and overwritten 
/// in the existing document if they do exist there. 
///
/// Setting an attribute value to @LIT{null} in the patch document will cause a 
/// value of @LIT{null} be saved for the attribute by default. If the intention
/// is to delete existing attributes with the patch command, the URL parameter 
/// @LIT{keepNull} can be used with a value of @LIT{false}.
/// This will modify the behavior of the patch command to remove any attributes 
/// from the existing document that are contained in the patch document with an 
/// attribute value of @LIT{null}.
///
/// The body of the response contains a JSON object with the information about
/// the handle and the revision.  The attribute @LIT{_id} contains the known
/// @FA{document-handle} of the updated document, the attribute @LIT{_rev}
/// contains the new document revision.
///
/// If the document does not exist, then a @LIT{HTTP 404} is returned and the
/// body of the response contains an error document.
///
/// @EXAMPLES
///
/// @verbinclude rest-patch-document
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::updateDocument () {
  return modifyDocument(true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function for replaceDocument and updateDocument
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::modifyDocument (bool isPatch) {
  vector<string> const& suffix = _request->suffix();

  if (suffix.size() != 2) {
    generateError(HttpResponse::BAD, 
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting UPDATE /_api/document/<document-handle>");
    return false;
  }

  // split the document reference
  string collection = suffix[0];
  string didStr = suffix[1];

  // auto-ptr that will free JSON data when scope is left
  JsonContainer container(TRI_UNKNOWN_MEM_ZONE, parseJsonBody());
  TRI_json_t* json = container.ptr();
  
  if (json == 0) {
    return false;
  }

  // extract document identifier
  TRI_voc_did_t did = StringUtils::uint64(didStr);

  // extract the revision
  TRI_voc_rid_t revision = extractRevision("if-match", "rev");

  // extract or chose the update policy
  TRI_doc_update_policy_e policy = extractUpdatePolicy();

  // find and load collection given by name or identifier
  CollectionAccessor ca(_vocbase, collection, getCollectionType(), false);

  int res = ca.use();
  if (TRI_ERROR_NO_ERROR != res) {
    generateCollectionError(collection, res);
    return false;
  }
  
  TRI_shaper_t* shaper = ca.shaper();
  TRI_voc_cid_t cid = ca.cid();
  TRI_voc_rid_t rid = 0;
  
  // init target document
  TRI_doc_mptr_t mptr;

  // .............................................................................
  // inside write transaction
  // .............................................................................

  WriteTransaction trx(&ca);

  if (isPatch) {
    // patching an existing document
    bool nullMeansRemove;
    bool found;

    char const* valueStr = _request->value("keepNull", found);
    if (!found || StringUtils::boolean(valueStr)) {
      // default: null values are saved as Null
      nullMeansRemove = false;
    }
    else {
      // delete null attributes
      nullMeansRemove = true;
    }

    // read the existing document
    TRI_doc_mptr_t document = trx.primary()->read(trx.primary(), did);
    TRI_shaped_json_t shapedJson;
    TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, document._data);
    TRI_json_t* old = TRI_JsonShapedJson(shaper, &shapedJson);
  
    if (old != 0) {
      TRI_json_t* patchedJson = TRI_MergeJson(TRI_UNKNOWN_MEM_ZONE, old, json, nullMeansRemove);
      TRI_FreeJson(shaper->_memoryZone, old);

      if (patchedJson != 0) {
        mptr = trx.primary()->updateJson(trx.primary(), patchedJson, did, revision, &rid, policy, false);

        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, patchedJson);
      }
    }
  }
  else {
    // replacing an existing document
    mptr = trx.primary()->updateJson(trx.primary(), json, did, revision, &rid, policy, false);
  }

  trx.end();

  // .............................................................................
  // outside write transaction
  // .............................................................................

  res = mptr._did == 0 ? TRI_errno() : 0;

  // generate result
  if (mptr._did != 0) {
    generateUpdated(cid, did, mptr._rid);
    return true;
  }

  switch (res) {
    case TRI_ERROR_ARANGO_READ_ONLY:
      generateError(HttpResponse::FORBIDDEN, 
          TRI_ERROR_ARANGO_READ_ONLY,
          "collection is read-only");
      return false;

    case TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND:
      generateDocumentNotFound(cid, didStr);
      return false;

    case TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED:
      generateError(HttpResponse::CONFLICT, res, "cannot create document, unique constraint violated");
      return false;

    case TRI_ERROR_ARANGO_GEO_INDEX_VIOLATED:
      generateError(HttpResponse::BAD, res, "geo constraint violated");
      return false;

    case TRI_ERROR_ARANGO_CONFLICT:
      generatePreconditionFailed(cid, did, rid);
      return false;

    default:
      generateError(HttpResponse::SERVER_ERROR,
          TRI_ERROR_INTERNAL,
          "cannot update, failed with " + string(TRI_last_error()));
      return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document
///
/// @RESTHEADER{DELETE /_api/document,deletes a document}
///
/// @REST{DELETE /_api/document/@FA{document-handle}}
///
/// Deletes the document identified by @FA{document-handle}. If the document
/// exists and could be deleted, then a @LIT{HTTP 204} is returned.
///
/// The body of the response contains a JSON object with the information about
/// the handle and the revision.  The attribute @LIT{_id} contains the known
/// @FA{document-handle} of the updated document, the attribute @LIT{_rev}
/// contains the known document revision.
///
/// If the document does not exist, then a @LIT{HTTP 404} is returned and the
/// body of the response contains an error document.
///
/// If an etag is supplied in the "If-Match" field, then the ArangoDB checks
/// that the revision of the document is equal to the etag. If there is a
/// mismatch, then a @LIT{HTTP 412} conflict is returned and no delete is
/// performed.
///
/// @REST{DELETE /_api/document/@FA{document-handle}?policy=@FA{policy}}
///
/// As before, if @FA{policy} is @LIT{error}. If @FA{policy} is @LIT{last}, then
/// the last write will win.
///
/// @REST{DELETE /_api/document/@FA{document-handle}?rev=@FA{etag}}
///
/// You can also supply the etag using the parameter @LIT{rev} instead of an
/// "If-Match" header. You must never supply both the "If-Match" header and the
/// @LIT{rev} parameter.
///
/// @EXAMPLES
///
/// Using document handle:
///
/// @verbinclude rest-delete-document
///
/// Unknown document handle:
///
/// @verbinclude rest-delete-document-unknown-handle
///
/// Revision conflict:
///
/// @verbinclude rest-delete-document-if-match-other
////////////////////////////////////////////////////////////////////////////////

bool RestDocumentHandler::deleteDocument () {
  vector<string> const& suffix = _request->suffix();

  if (suffix.size() != 2) {
    generateError(HttpResponse::BAD, 
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting DELETE /_api/document/<document-handle>");
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

  // find and load collection given by name or identifier
  CollectionAccessor ca(_vocbase, collection, getCollectionType(), false);

  int res = ca.use();
  if (TRI_ERROR_NO_ERROR != res) {
    generateCollectionError(collection, res);
    return false;
  }
  
  TRI_voc_cid_t cid = ca.cid();
  TRI_voc_rid_t rid = 0;

  // .............................................................................
  // inside write transaction
  // .............................................................................

  WriteTransaction trx(&ca);

  // unlocking is performed in destroy()
  res = trx.primary()->destroy(trx.primary(), did, revision, &rid, policy, false);

  trx.end();

  // .............................................................................
  // outside write transaction
  // .............................................................................

  // generate result
  if (res == TRI_ERROR_NO_ERROR) {
    generateDeleted(cid, did, rid);
    return true;
  }
  else {
    switch (res) {
      case TRI_ERROR_ARANGO_READ_ONLY:
        generateError(HttpResponse::FORBIDDEN, 
                      TRI_ERROR_ARANGO_READ_ONLY,
                      "collection is read-only");
        return false;

      case TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND:
        generateDocumentNotFound(cid, didStr);
        return false;

      case TRI_ERROR_ARANGO_CONFLICT:
        generatePreconditionFailed(cid, did, rid);
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
