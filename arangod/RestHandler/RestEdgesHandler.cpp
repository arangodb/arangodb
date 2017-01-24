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
#include "MMFiles/MMFilesEdgeIndex.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/OperationCursor.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "VocBase/Traverser.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::rest;

RestEdgesHandler::RestEdgesHandler(GeneralRequest* request,
                                   GeneralResponse* response)
    : RestVocbaseBaseHandler(request, response) {}

RestStatus RestEdgesHandler::execute() {
  // extract the sub-request type
  auto const type = _request->requestType();

  // execute one of the CRUD methods
  switch (type) {
    case rest::RequestType::GET: {
      readEdges();
      break;
    }
    case rest::RequestType::PUT:
      // Now unsupported. Just temporary to check
      TRI_ASSERT(false);
      readEdges();
      break;
    case rest::RequestType::POST:
      readEdgesForMultipleVertices();
      break;
    default:
      generateNotImplemented("ILLEGAL " + EDGES_PATH);
      break;
  }

  // this handler is done
  return RestStatus::DONE;
}

bool RestEdgesHandler::getEdgesForVertexList(
    VPackSlice const ids,
    TRI_edge_direction_e direction, SingleCollectionTransaction& trx,
    VPackBuilder& result, size_t& scannedIndex, size_t& filtered) {
  TRI_ASSERT(result.isOpenArray());
  TRI_ASSERT(ids.isArray());
  trx.orderDitch(trx.cid());  // will throw when it fails

  std::string const collectionName =
      trx.resolver()->getCollectionName(trx.cid());
  Transaction::IndexHandle indexId = trx.edgeIndexHandle(collectionName);

  VPackBuilder searchValueBuilder;
  MMFilesEdgeIndex::buildSearchValueFromArray(direction, ids, searchValueBuilder);
  VPackSlice search = searchValueBuilder.slice();

  std::unique_ptr<OperationCursor> cursor =
      trx.indexScan(collectionName, arangodb::Transaction::CursorType::INDEX,
                    indexId, search, nullptr, 0, UINT64_MAX, 1000, false);
  if (cursor->failed()) {
    THROW_ARANGO_EXCEPTION(cursor->code);
  }

  std::vector<DocumentIdentifierToken> batch;
  ManagedDocumentResult mmdr;
  auto collection = trx.documentCollection();
  while (cursor->hasMore()) {
    cursor->getMoreMptr(batch);
    scannedIndex += batch.size();

    for (auto const& it : batch) {
      if (collection->readDocument(&trx, mmdr, it)) {
        result.add(VPackSlice(mmdr.vpack()));
      }
    }
  }

  return true;
}

bool RestEdgesHandler::getEdgesForVertex(
    std::string const& id, std::string const& collectionName,
    TRI_edge_direction_e direction, SingleCollectionTransaction& trx,
    VPackBuilder& result, size_t& scannedIndex, size_t& filtered) {
  TRI_ASSERT(result.isOpenArray());
  trx.orderDitch(trx.cid());  // will throw when it fails

  Transaction::IndexHandle indexId = trx.edgeIndexHandle(collectionName);

  VPackBuilder searchValueBuilder;
  MMFilesEdgeIndex::buildSearchValue(direction, id, searchValueBuilder);
  VPackSlice search = searchValueBuilder.slice();

  std::unique_ptr<OperationCursor> cursor =
      trx.indexScan(collectionName, arangodb::Transaction::CursorType::INDEX,
                    indexId, search, nullptr, 0, UINT64_MAX, 1000, false);
  if (cursor->failed()) {
    THROW_ARANGO_EXCEPTION(cursor->code);
  }

  std::vector<DocumentIdentifierToken> batch;
  ManagedDocumentResult mmdr;
  auto collection = trx.documentCollection();
  while (cursor->hasMore()) {
    cursor->getMoreMptr(batch);
    scannedIndex += batch.size();

    for (auto const& it : batch) {
      if (collection->readDocument(&trx, mmdr, it)) {
        result.add(VPackSlice(mmdr.vpack()));
      }
    }
  }

  return true;
}

// read in- or outbound edges
bool RestEdgesHandler::readEdges() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expected GET " + EDGES_PATH +
                      "/<collection-identifier>?vertex=<vertex-handle>&"
                      "direction=<direction>");
    return false;
  }

  std::string collectionName = suffixes[0];
  CollectionNameResolver resolver(_vocbase);
  TRI_col_type_e colType = resolver.getCollectionTypeCluster(collectionName);
  if (colType == TRI_COL_TYPE_UNKNOWN) {
    generateError(rest::ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return false;
  }

  if (colType != TRI_COL_TYPE_EDGE) {
    generateError(rest::ResponseCode::BAD,
                  TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
    return false;
  }

  bool found;
  std::string dir = _request->value("direction", found);

  if (!found || dir.empty()) {
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
    generateError(rest::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "<direction> must by any, in, or out, not: " + dirString);
    return false;
  }

  std::string const& startVertex = _request->value("vertex", found);

  if (!found || startVertex.empty()) {
    generateError(rest::ResponseCode::BAD,
                  TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD,
                  "illegal document handle");
    return false;
  }

  if (ServerState::instance()->isCoordinator()) {
    std::string vertexString(startVertex);
    rest::ResponseCode responseCode;
    VPackBuilder resultDocument;
    resultDocument.openObject();

    int res = getFilteredEdgesOnCoordinator(
        _vocbase->name(), collectionName, vertexString, direction,
        responseCode, resultDocument);
    if (res != TRI_ERROR_NO_ERROR) {
      generateError(responseCode, res);
      return false;
    }

    resultDocument.add("error", VPackValue(false));
    resultDocument.add("code", VPackValue(200));
    resultDocument.close();

    generateResult(rest::ResponseCode::OK, resultDocument.slice());

    return true;
  }

  // find and load collection given by name or identifier
  SingleCollectionTransaction trx(
      StandaloneTransactionContext::Create(_vocbase), collectionName,
      TRI_TRANSACTION_READ);

  // .............................................................................
  // inside read transaction
  // .............................................................................

  int res = trx.begin();
  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collectionName, res, "");
    return false;
  }

  size_t filtered = 0;
  size_t scannedIndex = 0;

  VPackBuilder resultBuilder;
  resultBuilder.openObject();
  // build edges
  resultBuilder.add(VPackValue("edges"));  // only key
  resultBuilder.openArray();
  // NOTE: collecitonName is the shard-name in DBServer case
  bool ok = getEdgesForVertex(startVertex, collectionName, direction, trx,
                              resultBuilder, scannedIndex, filtered);
  resultBuilder.close();

  res = trx.finish(res);
  if (!ok) {
    // Error has been built internally
    return false;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    if (ServerState::instance()->isDBServer()) {
      // If we are a DBserver, we want to use the cluster-wide collection
      // name for error reporting:
      collectionName = trx.resolver()->getCollectionName(trx.cid());
    }
    generateTransactionError(collectionName, res, "");
    return false;
  }

  resultBuilder.add("error", VPackValue(false));
  resultBuilder.add("code", VPackValue(200));
  resultBuilder.add("stats", VPackValue(VPackValueType::Object));
  resultBuilder.add("scannedIndex", VPackValue(scannedIndex));
  resultBuilder.add("filtered", VPackValue(filtered));
  resultBuilder.close();  // inner object
  resultBuilder.close();

  // and generate a response
  generateResult(rest::ResponseCode::OK, resultBuilder.slice(),
                 trx.transactionContext());

  return true;
}

// Internal function to receive all edges for a list of vertices
// Not publicly documented on purpose.
// NOTE: It ONLY except _id strings. Nothing else
bool RestEdgesHandler::readEdgesForMultipleVertices() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expected POST " + EDGES_PATH +
                      "/<collection-identifier>?direction=<direction>");
    return false;
  }

  bool parseSuccess = true;
  std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(&VPackOptions::Defaults, parseSuccess);

  if (!parseSuccess) {
    generateError(rest::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expected POST " + EDGES_PATH +
                      "/<collection-identifier>?direction=<direction>");
    // A body is required
    return false;
  }
  VPackSlice body = parsedBody->slice();

  if (!body.isArray()) {
    generateError(rest::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "Expected an array of vertex _id's in body parameter");
    return false;
  }

  std::string collectionName = suffixes[0];
  CollectionNameResolver resolver(_vocbase);
  TRI_col_type_e colType = resolver.getCollectionTypeCluster(collectionName);

  if (colType == TRI_COL_TYPE_UNKNOWN) {
    generateError(rest::ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return false;
  } else if (colType != TRI_COL_TYPE_EDGE) {
    generateError(rest::ResponseCode::BAD,
                  TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
    return false;
  }

  bool found;
  std::string dirString = _request->value("direction", found);

  if (!found || dirString.empty()) {
    dirString = "any";
  }

  TRI_edge_direction_e direction;

  if (dirString == "any") {
    direction = TRI_EDGE_ANY;
  } else if (dirString == "out" || dirString == "outbound") {
    direction = TRI_EDGE_OUT;
  } else if (dirString == "in" || dirString == "inbound") {
    direction = TRI_EDGE_IN;
  } else {
    generateError(rest::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "<direction> must by any, in, or out, not: " + dirString);
    return false;
  }

  if (ServerState::instance()->isCoordinator()) {
    rest::ResponseCode responseCode;
    VPackBuilder resultDocument;
    resultDocument.openObject();

    for (auto const& it : VPackArrayIterator(body)) {
      if (it.isString()) {
        std::string vertexString(it.copyString());

        int res = getFilteredEdgesOnCoordinator(
            _vocbase->name(), collectionName, vertexString, direction,
            responseCode, resultDocument);
        if (res != TRI_ERROR_NO_ERROR) {
          generateError(responseCode, res);
          return false;
        }
      }
    }
    resultDocument.add("error", VPackValue(false));
    resultDocument.add("code", VPackValue(200));
    resultDocument.close();

    generateResult(rest::ResponseCode::OK, resultDocument.slice());
    return true;
  }

  // find and load collection given by name or identifier
  SingleCollectionTransaction trx(
      StandaloneTransactionContext::Create(_vocbase), collectionName,
      TRI_TRANSACTION_READ);

  // .............................................................................
  // inside read transaction
  // .............................................................................

  int res = trx.begin();
  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collectionName, res, "");
    return false;
  }

  // If we are a DBserver, we want to use the cluster-wide collection
  // name for error reporting:
  if (ServerState::instance()->isDBServer()) {
    collectionName = trx.resolver()->getCollectionName(trx.cid());
  }

  size_t filtered = 0;
  size_t scannedIndex = 0;

  VPackBuilder resultBuilder;
  resultBuilder.openObject();
  // build edges
  resultBuilder.add(VPackValue("edges"));  // only key
  resultBuilder.openArray();

  bool ok = getEdgesForVertexList(body, direction, trx, resultBuilder,
                                  scannedIndex, filtered);

  if (!ok) {
    // Ignore the error
  }

  resultBuilder.close();

  res = trx.finish(res);
  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collectionName, res, "");
    return false;
  }

  resultBuilder.add("error", VPackValue(false));
  resultBuilder.add("code", VPackValue(200));
  resultBuilder.add("stats", VPackValue(VPackValueType::Object));
  resultBuilder.add("scannedIndex", VPackValue(scannedIndex));
  resultBuilder.add("filtered", VPackValue(filtered));
  resultBuilder.close();  // inner object
  resultBuilder.close();

  // and generate a response
  generateResult(rest::ResponseCode::OK, resultBuilder.slice(),
                 trx.transactionContext());

  return true;
}
