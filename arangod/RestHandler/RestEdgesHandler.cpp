////////////////////////////////////////////////////////////////////////////////
/// @brief edges request handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2010-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestEdgesHandler.h"
#include "VocBase/Traverser.h"

using namespace triagens::rest;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestEdgesHandler::RestEdgesHandler (HttpRequest* request)
  : RestVocbaseBaseHandler(request) {
}


// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_t RestEdgesHandler::execute () {
  // extract the sub-request type
  HttpRequest::HttpRequestType type = _request->requestType();

  // execute one of the CRUD methods
  switch (type) {
    case HttpRequest::HTTP_REQUEST_GET:    readEdges(); break;
    // case HttpRequest::HTTP_REQUEST_PUT:    readEdges(); break;


    case HttpRequest::HTTP_REQUEST_PUT:
    case HttpRequest::HTTP_REQUEST_PATCH:
    case HttpRequest::HTTP_REQUEST_HEAD:
    case HttpRequest::HTTP_REQUEST_POST:
    case HttpRequest::HTTP_REQUEST_DELETE:
    case HttpRequest::HTTP_REQUEST_ILLEGAL:
    default: {
      generateNotImplemented("ILLEGAL " + EDGES_PATH);
      break;
    }
  }

  // this handler is done
  return status_t(HANDLER_DONE);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock API_EDGE_READINOUTBOUND
/// @brief get edges
///
/// @RESTHEADER{GET /_api/edges/{collection-id}, Read in- or outbound edges}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{collection-id,string,required}
/// The id of the collection.
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{vertex,string,required}
/// The id of the start vertex.
///
/// @RESTQUERYPARAM{direction,string,optional}
/// Selects *in* or *out* direction for edges. If not set, any edges are
/// returned.
///
/// @RESTDESCRIPTION
/// Returns an array of edges starting or ending in the vertex identified by
/// *vertex-handle*.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the edge collection was found and edges were retrieved.
///
/// @RESTRETURNCODE{400}
/// is returned if the request contains invalid parameters.
///
/// @RESTRETURNCODE{404}
/// is returned if the edge collection was not found.
///
/// @EXAMPLES
///
/// Any direction
///
/// @EXAMPLE_ARANGOSH_RUN{RestEdgesReadEdgesAny}
///     var Graph = require("org/arangodb/graph-blueprint").Graph;
///     var g = new Graph("graph", "vertices", "edges");
///     var v1 = g.addVertex(1);
///     var v2 = g.addVertex(2);
///     var v3 = g.addVertex(3);
///     var v4 = g.addVertex(4);
///     g.addEdge(v1, v3, 5, "v1 -> v3");
///     g.addEdge(v2, v1, 6, "v2 -> v1");
///     g.addEdge(v4, v1, 7, "v4 -> v1");
///
///     var url = "/_api/edges/edges?vertex=vertices/1";
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop("edges");
///     db._drop("vertices");
///     db._graphs.remove("graph");
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// In edges
///
/// @EXAMPLE_ARANGOSH_RUN{RestEdgesReadEdgesIn}
///     var Graph = require("org/arangodb/graph-blueprint").Graph;
///     var g = new Graph("graph", "vertices", "edges");
///     var v1 = g.addVertex(1);
///     var v2 = g.addVertex(2);
///     var v3 = g.addVertex(3);
///     var v4 = g.addVertex(4);
///     g.addEdge(v1, v3, 5, "v1 -> v3");
///     g.addEdge(v2, v1, 6, "v2 -> v1");
///     g.addEdge(v4, v1, 7, "v4 -> v1");
///
///     var url = "/_api/edges/edges?vertex=vertices/1&direction=in";
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop("edges");
///     db._drop("vertices");
///     db._graphs.remove("graph");
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Out edges
///
/// @EXAMPLE_ARANGOSH_RUN{RestEdgesReadEdgesOut}
///     var Graph = require("org/arangodb/graph-blueprint").Graph;
///     var g = new Graph("graph", "vertices", "edges");
///     var v1 = g.addVertex(1);
///     var v2 = g.addVertex(2);
///     var v3 = g.addVertex(3);
///     var v4 = g.addVertex(4);
///     g.addEdge(v1, v3, 5, "v1 -> v3");
///     g.addEdge(v2, v1, 6, "v2 -> v1");
///     g.addEdge(v4, v1, 7, "v4 -> v1");
///
///     var url = "/_api/edges/edges?vertex=vertices/1&direction=out";
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop("edges");
///     db._drop("vertices");
///     db._graphs.remove("graph");
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

bool RestEdgesHandler::readEdges () {
  std::vector<std::string> const& suffix = _request->suffix();

  if (! (suffix.size() == 1)) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expected GET " + EDGES_PATH +
                  "/<collection-identifier>?vertex=<vertex-handle>&direction=<direction>");
    return false;
  }

  std::string collectionName = suffix[0];
  // TODO check if collection exists

  bool found;
  char const* dir = _request->value("direction", found);

  if (! found || *dir == '\0') {
    dir = "any";
  }

  std::string dirString(dir);
  TRI_edge_direction_e direction;

  if (dirString == "any") {
    direction = TRI_EDGE_ANY;
  }
  else if (dirString == "out" || dirString == "outbound") {
    direction = TRI_EDGE_OUT;
  }
  else if (dirString == "in" || dirString == "inbound") {
    direction = TRI_EDGE_IN;
  }
  else {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "<direction> must by any, in, or out, not: " + dirString);
    return false;
  }

  char const* startVertex = _request->value("vertex", found);

  if (! found || *startVertex == '\0') {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD,
                  "illegal document handle");
    return false;
  }

  if (ServerState::instance()->isCoordinator()) {
    // TODO
    // return getDocumentCoordinator(collection, key, generateBody);
    return false;
  }

  // find and load collection given by name or identifier
  SingleCollectionReadOnlyTransaction trx(new StandaloneTransactionContext(), _vocbase, collectionName);

  // .............................................................................
  // inside read transaction
  // .............................................................................

  int res = trx.begin();
  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collectionName, res);
    return false;
  }

  TRI_voc_cid_t const cid = trx.cid();

  // If we are a DBserver, we want to use the cluster-wide collection
  // name for error reporting:
  if (ServerState::instance()->isDBServer()) {
    collectionName = trx.resolver()->getCollectionName(cid);
  }

  triagens::arango::traverser::VertexId start;
  try {
    start = triagens::arango::traverser::IdStringToVertexId (
      trx.resolver(),
      startVertex
    );
  }
  catch (triagens::basics::Exception& e) {
    handleError(e);
    return false;
  }

  TRI_transaction_collection_t* collection = trx.trxCollection();

  std::vector<TRI_doc_mptr_copy_t>&& edges = TRI_LookupEdgesDocumentCollection(
    collection->_collection->_collection,
    direction,
    start.cid,
    const_cast<char*>(start.key)
  );

  res = trx.finish(res);
  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collectionName, res);
    return false;
  }


  // generate result
  triagens::basics::Json documents(triagens::basics::Json::Array);
  documents.reserve(edges.size());

  for (auto& e : edges) {
    DocumentAccessor da(trx.resolver(), collection->_collection->_collection, &e);
    documents.add(da.toJson());
  }

  return true;
}

