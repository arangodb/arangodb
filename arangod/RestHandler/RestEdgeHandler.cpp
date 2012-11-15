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
#include "BasicsC/strings.h"
#include "VocBase/document-collection.h"
#include "VocBase/edge-collection.h"
#include "Rest/HttpRequest.h"
#include "Rest/JsonContainer.h"

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
                  "superfluous suffix, expecting " + EDGE_PATH + "?collection=<identifier>");
    return false;
  }

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

  // auto-ptr that will free JSON data when scope is left
  JsonContainer container(TRI_UNKNOWN_MEM_ZONE, parseJsonBody());
  TRI_json_t* json = container.ptr();

  if (json == 0) {
    return false;
  }
  
  // find and load collection given by name or identifier
  SelfContainedWriteTransaction<RestTransactionContext> trx(_vocbase, collection, getCollectionType(), create); 
  
  // .............................................................................
  // inside write transaction
  // .............................................................................
 
  int res = trx.begin(); 
  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
    return true;
  }

  TRI_voc_cid_t cid = trx.cid();

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
    if (res == TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
      generateError(HttpResponse::NOT_FOUND, res, "'from' does not point to a valid collection");
    }
    else {
      generateError(HttpResponse::BAD, res, "'from' is not a document handle");
    }
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

    if (res == TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
      generateError(HttpResponse::NOT_FOUND, res, "'to' does not point to a valid collection");
    }
    else {
      generateError(HttpResponse::BAD, res, "'to' is not a document handle");
    }
    return false;
  }
  
  // .............................................................................
  // inside write transaction
  // .............................................................................
  
  // will hold the result
  TRI_doc_mptr_t* document = 0;
  res = trx.createEdge(&document, json, extractWaitForSync(), &edge);
  res = trx.finish(res);

  // .............................................................................
  // outside write transaction
  // .............................................................................
  
  if (res != TRI_ERROR_NO_ERROR) {
    if (edge._toKey) {
     TRI_FreeString(TRI_CORE_MEM_ZONE, edge._toKey);
    }
    if (edge._fromKey) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, edge._fromKey);
    }

    generateTransactionError(collection, res);
    return false;
  }

  assert(document);
  assert(document->_key);

  // generate result
  if (trx.synchronous()) {
    generateCreated(trx.cid(), document->_key, document->_rid);
  }
  else {
    generateAccepted(trx.cid(), document->_key, document->_rid);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a document handle
////////////////////////////////////////////////////////////////////////////////

int RestEdgeHandler::parseDocumentId (string const& handle,
                                      TRI_voc_cid_t& cid,
                                      TRI_voc_key_t& key) {
  vector<string> split;
  int res;

  split = StringUtils::split(handle, '/');

  if (split.size() != 2) {
    return TRI_set_errno(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }

  cid = TRI_UInt64String(split[0].c_str());
  res = TRI_errno();

  if (res != TRI_ERROR_NO_ERROR) {
    // issue #277: non-numeric collection id, now try looking up by name
    TRI_vocbase_col_t* collection = TRI_LookupCollectionByNameVocBase(_vocbase, split[0].c_str());
    if (collection == 0) {
      // collection not found
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    }
    // collection found by name
    cid = collection->_cid;
    // clear previous error
    TRI_set_errno(TRI_ERROR_NO_ERROR);
  }
  else {
    // validate whether collection exists
    TRI_vocbase_col_t* collection = TRI_LookupCollectionByIdVocBase(_vocbase, cid);
    if (collection == 0) {
      // collection not found
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    }
  }

  key = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, split[1].c_str());

  return TRI_errno();
}


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
