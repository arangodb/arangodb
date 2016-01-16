////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "RestEdgesHandler.h"
#include "Basics/ScopeGuard.h"
#include "Cluster/ClusterMethods.h"
#include "VocBase/Traverser.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::rest;
using namespace arangodb::arango;



RestEdgesHandler::RestEdgesHandler(HttpRequest* request)
    : RestVocbaseBaseHandler(request) {}



HttpHandler::status_t RestEdgesHandler::execute() {
  // extract the sub-request type
  HttpRequest::HttpRequestType type = _request->requestType();

  // execute one of the CRUD methods
  switch (type) {
    case HttpRequest::HTTP_REQUEST_GET: {
      std::vector<traverser::TraverserExpression*> empty;
      readEdges(empty);
      break;
    }
    case HttpRequest::HTTP_REQUEST_PUT:
      readFilteredEdges();
      break;
    case HttpRequest::HTTP_REQUEST_POST:
      readEdgesForMultipleVertices();
      break;
    case HttpRequest::HTTP_REQUEST_HEAD:
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


bool RestEdgesHandler::getEdgesForVertex(
    std::string const& id,
    std::vector<traverser::TraverserExpression*> const& expressions,
    TRI_edge_direction_e direction, SingleCollectionReadOnlyTransaction& trx,
    arangodb::basics::Json& result, size_t& scannedIndex, size_t& filtered) {
  arangodb::arango::traverser::VertexId start;
  try {
    start = arangodb::arango::traverser::IdStringToVertexId(trx.resolver(), id);
  } catch (arangodb::basics::Exception& e) {
    handleError(e);
    return false;
  }
  TRI_document_collection_t* docCol =
      trx.trxCollection()->_collection->_collection;

  std::vector<TRI_doc_mptr_copy_t>&& edges = TRI_LookupEdgesDocumentCollection(
      &trx, docCol, direction, start.cid, const_cast<char*>(start.key));

  // generate result
  result.reserve(edges.size());
  scannedIndex += edges.size();

  if (expressions.empty()) {
    for (auto& e : edges) {
      DocumentAccessor da(trx.resolver(), docCol, &e);
      result.add(da.toJson());
    }
  } else {
    for (auto& e : edges) {
      bool add = true;
      // Expressions symbolize an and, so all have to be matched
      for (auto& exp : expressions) {
        if (exp->isEdgeAccess &&
            !exp->matchesCheck(e, docCol, trx.resolver())) {
          ++filtered;
          add = false;
          break;
        }
      }
      if (add) {
        DocumentAccessor da(trx.resolver(), docCol, &e);
        result.add(da.toJson());
      }
    }
  }
  return true;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock API_EDGE_READINOUTBOUND
////////////////////////////////////////////////////////////////////////////////

bool RestEdgesHandler::readEdges(
    std::vector<traverser::TraverserExpression*> const& expressions) {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 1) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expected GET " + EDGES_PATH +
                      "/<collection-identifier>?vertex=<vertex-handle>&"
                      "direction=<direction>");
    return false;
  }

  std::string collectionName = suffix[0];
  CollectionNameResolver resolver(_vocbase);
  TRI_col_type_t colType = resolver.getCollectionTypeCluster(collectionName);
  if (colType == TRI_COL_TYPE_UNKNOWN) {
    generateError(HttpResponse::NOT_FOUND,
                  TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return false;
  } else if (colType != TRI_COL_TYPE_EDGE) {
    generateError(HttpResponse::BAD, TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
    return false;
  }

  bool found;
  char const* dir = _request->value("direction", found);

  if (!found || *dir == '\0') {
    dir = "any";
  }

  std::string dirString(dir);
  TRI_edge_direction_e direction;

  if (dirString == "any") {
    direction = TRI_EDGE_ANY;
  } else if (dirString == "out" || dirString == "outbound") {
    direction = TRI_EDGE_OUT;
  } else if (dirString == "in" || dirString == "inbound") {
    direction = TRI_EDGE_IN;
  } else {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "<direction> must by any, in, or out, not: " + dirString);
    return false;
  }

  char const* startVertex = _request->value("vertex", found);

  if (!found || *startVertex == '\0') {
    generateError(HttpResponse::BAD, TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD,
                  "illegal document handle");
    return false;
  }

  if (ServerState::instance()->isCoordinator()) {
    std::string vertexString(startVertex);
    arangodb::rest::HttpResponse::HttpResponseCode responseCode;
    std::string contentType;
    arangodb::basics::Json resultDocument(arangodb::basics::Json::Object, 3);
    int res = getFilteredEdgesOnCoordinator(
        _vocbase->_name, collectionName, vertexString, direction, expressions,
        responseCode, contentType, resultDocument);
    if (res != TRI_ERROR_NO_ERROR) {
      generateError(responseCode, res);
      return false;
    }

    resultDocument.set("error", arangodb::basics::Json(false));
    resultDocument.set("code", arangodb::basics::Json(200));
    generateResult(resultDocument.json());
    return true;
  }

  // find and load collection given by name or identifier
  SingleCollectionReadOnlyTransaction trx(new StandaloneTransactionContext(),
                                          _vocbase, collectionName);

  // .............................................................................
  // inside read transaction
  // .............................................................................

  int res = trx.begin();
  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collectionName, res);
    return false;
  }

  // If we are a DBserver, we want to use the cluster-wide collection
  // name for error reporting:
  if (ServerState::instance()->isDBServer()) {
    collectionName = trx.resolver()->getCollectionName(trx.cid());
  }

  size_t filtered = 0;
  size_t scannedIndex = 0;

  arangodb::basics::Json documents(arangodb::basics::Json::Array);
  bool ok = getEdgesForVertex(startVertex, expressions, direction, trx,
                              documents, scannedIndex, filtered);
  res = trx.finish(res);
  if (!ok) {
    // Error has been built internally
    return false;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collectionName, res);
    return false;
  }

  arangodb::basics::Json result(arangodb::basics::Json::Object, 4);
  result("edges", documents);
  result("error", arangodb::basics::Json(false));
  result("code", arangodb::basics::Json(200));
  arangodb::basics::Json stats(arangodb::basics::Json::Object, 2);

  stats("scannedIndex",
        arangodb::basics::Json(static_cast<int32_t>(scannedIndex)));
  stats("filtered", arangodb::basics::Json(static_cast<int32_t>(filtered)));
  result("stats", stats);

  // and generate a response
  generateResult(result.json());

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// Internal function to receive all edges for a list of vertices
/// Not publicly documented on purpose.
/// NOTE: It ONLY except _id strings. Nothing else
////////////////////////////////////////////////////////////////////////////////

bool RestEdgesHandler::readEdgesForMultipleVertices() {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 1) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expected POST " + EDGES_PATH +
                      "/<collection-identifier>?direction=<direction>");
    return false;
  }

  bool parseSuccess = true;
  VPackOptions options;
  std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(&options, parseSuccess);

  if (!parseSuccess) {
    // A body is required
    return false;
  }
  VPackSlice body = parsedBody->slice();

  if (!body.isArray()) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "Expected an array of vertex _id's in body parameter");
    return false;
  }

  std::string collectionName = suffix[0];
  CollectionNameResolver resolver(_vocbase);
  TRI_col_type_t colType = resolver.getCollectionTypeCluster(collectionName);

  if (colType == TRI_COL_TYPE_UNKNOWN) {
    generateError(HttpResponse::NOT_FOUND,
                  TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return false;
  } else if (colType != TRI_COL_TYPE_EDGE) {
    generateError(HttpResponse::BAD, TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
    return false;
  }

  bool found;
  char const* dir = _request->value("direction", found);

  if (!found || *dir == '\0') {
    dir = "any";
  }

  std::string dirString(dir);
  TRI_edge_direction_e direction;

  if (dirString == "any") {
    direction = TRI_EDGE_ANY;
  } else if (dirString == "out" || dirString == "outbound") {
    direction = TRI_EDGE_OUT;
  } else if (dirString == "in" || dirString == "inbound") {
    direction = TRI_EDGE_IN;
  } else {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "<direction> must by any, in, or out, not: " + dirString);
    return false;
  }

  if (ServerState::instance()->isCoordinator()) {
    // This API is only for internal use on DB servers and is not (yet) allowed
    // to
    // be executed on the coordinator
    return false;
  }

  // find and load collection given by name or identifier
  SingleCollectionReadOnlyTransaction trx(new StandaloneTransactionContext(),
                                          _vocbase, collectionName);

  // .............................................................................
  // inside read transaction
  // .............................................................................

  int res = trx.begin();
  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collectionName, res);
    return false;
  }

  // If we are a DBserver, we want to use the cluster-wide collection
  // name for error reporting:
  if (ServerState::instance()->isDBServer()) {
    collectionName = trx.resolver()->getCollectionName(trx.cid());
  }

  size_t filtered = 0;
  size_t scannedIndex = 0;
  std::vector<traverser::TraverserExpression*> const expressions;

  arangodb::basics::Json documents(arangodb::basics::Json::Array);
  for (auto const& vertexSlice : VPackArrayIterator(body)) {
    if (vertexSlice.isString()) {
      std::string vertex = vertexSlice.copyString();
      bool ok = getEdgesForVertex(vertex, expressions, direction, trx,
                                  documents, scannedIndex, filtered);
      if (!ok) {
        // Ignore the error
      }
    }
  }

  res = trx.finish(res);
  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collectionName, res);
    return false;
  }

  arangodb::basics::Json result(arangodb::basics::Json::Object, 4);
  result("edges", documents);
  result("error", arangodb::basics::Json(false));
  result("code", arangodb::basics::Json(200));
  arangodb::basics::Json stats(arangodb::basics::Json::Object, 2);

  stats("scannedIndex",
        arangodb::basics::Json(static_cast<int32_t>(scannedIndex)));
  stats("filtered", arangodb::basics::Json(static_cast<int32_t>(filtered)));
  result("stats", stats);

  // and generate a response
  generateResult(result.json());

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// Internal function for optimized edge retrieval.
/// Allows to send an TraverserExpression for filtering in the body
/// Not publicly documented on purpose.
////////////////////////////////////////////////////////////////////////////////

bool RestEdgesHandler::readFilteredEdges() {
  std::vector<traverser::TraverserExpression*> expressions;
  bool parseSuccess = true;
  VPackOptions options;
  std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(&options, parseSuccess);
  if (!parseSuccess) {
    // We continue unfiltered
    // Filter could be done by caller
    delete _response;
    _response = nullptr;
    return readEdges(expressions);
  }
  VPackSlice body = parsedBody->slice();
  arangodb::basics::ScopeGuard guard{[]() -> void {},
                                     [&expressions]() -> void {
                                       for (auto& e : expressions) {
                                         delete e;
                                       }
                                     }};

  if (!body.isArray()) {
    generateError(
        HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "Expected an array of traverser expressions as body parameter");
    return false;
  }

  expressions.reserve(body.length());

  for (auto const& exp : VPackArrayIterator(body)) {
    if (exp.isObject()) {
      auto expression = std::make_unique<traverser::TraverserExpression>(exp);
      expressions.emplace_back(expression.get());
      expression.release();
    }
  }
  return readEdges(expressions);
}
