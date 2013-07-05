////////////////////////////////////////////////////////////////////////////////
/// @brief document request handler
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

#include "RestEdgeHandler.h"

#include "Basics/StringUtils.h"
#include "BasicsC/conversions.h"
#include "BasicsC/tri-strings.h"
#include "Rest/HttpRequest.h"
#include "VocBase/document-collection.h"
#include "VocBase/edge-collection.h"
#include "Utilities/ResourceHolder.h"

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
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{collection,string,required}
/// Creates a new edge in the collection identified by `collection` name.
///
/// @RESTQUERYPARAM{from,string,required}
/// The document handle of the start point must be passed in `from` handle.
///
/// @RESTQUERYPARAM{to,string,required}
/// The document handle of the end point must be passed in `to` handle.
///
/// @RESTBODYPARAM{edge-document,json,required}
/// A JSON representation of the edge document must be passed as the body of
/// the POST request. This JSON object may contain the edge's document key in
/// the `_key` attribute if needed.
///
/// @RESTDESCRIPTION
/// `from` handle and `to` handle are immutable once the edge has been
/// created.
///
/// In all other respects the method works like `POST /document`, see
/// @ref RestDocument for details.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{202}
/// is returned if the edge was created successfully.
///
/// @RESTRETURNCODE{404}
/// is returned if the edge collection was not found.
///
/// @EXAMPLES
///
/// Create an edge and reads it back:
///
/// @EXAMPLE_ARANGOSH_RUN{RestEdgeCreateEdge}
///     var Graph = require("org/arangodb/graph").Graph;
///     var g = new Graph("graph", "vertices", "edges");
///     g.addVertex(1);
///     g.addVertex(2);
///     var url = "/_api/edge/?collection=edges&from=vertices/1&to=vertices/2";
/// 
///     var response = logCurlRequest("POST", url, { "name": "Emil" });
/// 
///     assert(response.code === 202);
///
///     logJsonResponse(response);
///     var body = response.body.replace(/\\/g, '');
///     var edge_id = JSON.parse(body)._id;
///     var response2 = logCurlRequest("GET", "/_api/edge/" + edge_id);
/// 
///     assert(response2.code === 200);
///
///     logJsonResponse(response2);
///     db._drop("edges");
///     db._drop("vertices");
///     db._graphs.remove("graph");
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

bool RestEdgeHandler::createDocument () {
  vector<string> const& suffix = _request->suffix();

  if (suffix.size() != 0) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "superfluous suffix, expecting " + EDGE_PATH + "?collection=<identifier>");
    return false;
  }

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
  const string& collection = _request->value("collection", found);

  if (! found || collection.empty()) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,
                  "'collection' is missing, expecting " + DOCUMENT_PATH + "?collection=<identifier>");
    return false;
  }

  const bool waitForSync = extractWaitForSync();

  // auto-ptr that will free JSON data when scope is left
  ResourceHolder holder;

  TRI_json_t* json = parseJsonBody();
  if (! holder.registerJson(TRI_UNKNOWN_MEM_ZONE, json)) {
    return false;
  }
  
  if (json->_type != TRI_JSON_ARRAY) {
    generateTransactionError(collection, TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
    return false;
  }

  if (! checkCreateCollection(collection, getCollectionType())) {
    return false;
  }

  // find and load collection given by name or identifier
  SingleCollectionWriteTransaction<StandaloneTransaction<RestTransactionContext>, 1> trx(_vocbase, _resolver, collection);

  // .............................................................................
  // inside write transaction
  // .............................................................................

  int res = trx.begin();
  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
    return false;
  }
  
  if (trx.primaryCollection()->base._info._type != TRI_COL_TYPE_EDGE) {
    // check if we are inserting with the EDGE handler into a non-EDGE collection    
    generateError(HttpResponse::BAD, TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
    return false;
  }

  const TRI_voc_cid_t cid = trx.cid();

  // edge
  TRI_document_edge_t edge;
  edge._fromCid = cid;
  edge._toCid = cid;
  edge._fromKey = 0;
  edge._toKey = 0;

  res = parseDocumentId(from, edge._fromCid, edge._fromKey);
  holder.registerString(TRI_CORE_MEM_ZONE, edge._fromKey);

  if (res != TRI_ERROR_NO_ERROR) {
    if (res == TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
      generateError(HttpResponse::NOT_FOUND, res, "'from' does not point to a valid collection");
    }
    else {
      generateError(HttpResponse::BAD, res, "'from' is not a document handle");
    }
    return false;
  }

  res = parseDocumentId(to, edge._toCid, edge._toKey);
  holder.registerString(TRI_CORE_MEM_ZONE, edge._toKey);

  if (res != TRI_ERROR_NO_ERROR) {
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
  TRI_doc_mptr_t document;
  res = trx.createEdge(&document, json, waitForSync, &edge);
  const bool wasSynchronous = trx.synchronous();
  res = trx.finish(res);

  // .............................................................................
  // outside write transaction
  // .............................................................................

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collection, res);
    return false;
  }

  assert(document._key != 0);

  // generate result
  if (wasSynchronous) {
    generateCreated(cid, document._key, document._rid);
  }
  else {
    generateAccepted(cid, document._key, document._rid);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
