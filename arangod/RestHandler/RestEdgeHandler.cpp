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

#include "RestEdgeHandler.h"

#include "Basics/StringUtils.h"
#include "BasicsC/conversions.h"
#include "Rest/HttpRequest.h"
#include "Rest/JsonContainer.h"
#include "VocBase/document-collection.h"
#include "strings.h"
#include "VocBase/edge-collection.h"

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

RestEdgeHandler::RestEdgeHandler (HttpRequest* request, TRI_vocbase_t* vocbase)
  : RestDocumentHandler(request, vocbase) {
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
/// @brief creates an edge
///
/// @RESTHEADER{POST /_api/edge,creates an edge}
///
/// @REST{POST /_api/edge?collection=@FA{collection-identifier}&from=@FA{from-handle}&to=@FA{to-handle}}
///
/// Creates a new edge in the collection identified by the
/// @FA{collection-identifier}.  A JSON representation of the document must be
/// passed as the body of the POST request. The object handle of the start point
/// must be passed in @FA{from-handle}. The object handle of the end point must
/// be passed in @FA{to-handle}.
///
/// In all other respects the method works like @LIT{POST /document}, see
/// @ref RestDocument for details.
///
/// If you request such an edge, the returned document will also contain the
/// attributes @LIT{_from} and @LIT{_to}.
///
/// @EXAMPLES
///
/// Create an edge:
///
/// @verbinclude rest-edge-create-edge
///
/// Read an edge:
///
/// @verbinclude rest-edge-read-edge
////////////////////////////////////////////////////////////////////////////////

bool RestEdgeHandler::createDocument () {
  vector<string> const& suffix = _request->suffix();

  if (suffix.size() != 0) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "superfluous suffix, expecting " + DOCUMENT_PATH + "?collection=<identifier>");
    return false;
  }

  bool forceSync = extractWaitForSync();

  // edge
  TRI_document_edge_t edge;

  // extract the from
  bool found;
  char const* from = _request->value("from", found);

  if (! found || *from == '\0') {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "'from' is missing, expecting " + EDGE_PATH + "?collection=<identifier>&from=<from-handle>&to=<to-handle>");
    return false;
  }

  // extract the to
  char const* to = _request->value("to", found);

  if (! found || *to == '\0') {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "'to' is missing, expecting " + EDGE_PATH + "?collection=<identifier>&from=<from-handle>&to=<to-handle>");
    return false;
  }

  // extract the cid
  string collection = _request->value("collection", found);

  if (! found || collection.empty()) {
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
  const bool waitForSync = forceSync || ca.waitForSync();

  // split document handle
  edge._fromCid = cid;
  edge._toCid = cid;
  edge._fromKey = 0;
  edge._toKey = 0;
  edge._isBidirectional = false;
  
  if (json->_type == TRI_JSON_ARRAY) {
    TRI_json_t* k = TRI_LookupArrayJson((TRI_json_t*) json, "_bidirectional");
    if (k != NULL && k->_type == TRI_JSON_BOOLEAN) {
      edge._isBidirectional = k->_value._boolean;
    }    
  }

  res = parseDocumentId(from, edge._fromCid, edge._fromKey);

  if (res != TRI_ERROR_NO_ERROR) {
    if (edge._fromKey) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, edge._fromKey);
    }

    generateError(HttpResponse::BAD,
                  res,
                  "'_from' is not a document handle");
    return false;
  }

  res = parseDocumentId(to, edge._toCid, edge._toKey);

  if (res != TRI_ERROR_NO_ERROR) {
    if (edge._toKey) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, edge._toKey);
    }

    if (edge._fromKey) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, edge._fromKey);
    }

    generateError(HttpResponse::BAD,
                  res,
                  "'_to' is not a document handle");
    return false;
  }

  // .............................................................................
  // inside write transaction
  // .............................................................................
  
  WriteTransaction trx(&ca);

  TRI_doc_mptr_t const mptr = trx.primary()->createJson(trx.primary(), TRI_DOC_MARKER_KEY_EDGE, json, &edge, reuseId, false, forceSync);

  trx.end();

  // .............................................................................
  // outside write transaction
  // .............................................................................

  // generate result
  if (mptr._key != 0) {
    if (waitForSync) {
      generateCreated(cid, mptr._key, mptr._rid);
    }
    else {
      generateAccepted(cid, mptr._key, mptr._rid);
    }

    return true;
  }
  else {
    int res = TRI_errno();
    
    if (edge._toKey) TRI_FreeString(TRI_CORE_MEM_ZONE, edge._toKey);
    if (edge._fromKey) TRI_FreeString(TRI_CORE_MEM_ZONE, edge._fromKey);

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
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
