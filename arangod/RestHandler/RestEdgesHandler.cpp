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
#include "Aql/AstNode.h"
#include "Aql/Graphs.h"
#include "Aql/Variable.h"
#include "Basics/ScopeGuard.h"
#include "Cluster/ClusterMethods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/OperationCursor.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/ManagedDocumentResult.h"

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
    case rest::RequestType::GET:
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

void RestEdgesHandler::readCursor(
    aql::AstNode* condition, aql::Variable const* var,
    std::string const& collectionName, SingleCollectionTransaction& trx,
    std::function<void(DocumentIdentifierToken const&)> cb) {
  transaction::Methods::IndexHandle indexId;
  bool foundIdx = trx.getBestIndexHandleForFilterCondition(
      collectionName, condition, var, 1000, indexId);
  if (!foundIdx) {
    // Right now we enforce an edge index that can exactly! work on this condition.
    // So it is impossible to not find an index.
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_ARANGO_NO_INDEX,
        "Unable to find an edge-index to identify matching edges.");
  }

  ManagedDocumentResult mmdr;
  std::unique_ptr<OperationCursor> cursor(trx.indexScanForCondition(
      indexId, condition, var, &mmdr, UINT64_MAX, 1000, false));

  if (cursor->failed()) {
    THROW_ARANGO_EXCEPTION(cursor->code);
  }

  cursor->all(cb);
}


bool RestEdgesHandler::getEdgesForVertex(
    std::string const& id, std::string const& collectionName,
    TRI_edge_direction_e direction, SingleCollectionTransaction& trx,
    std::function<void(DocumentIdentifierToken const&)> cb) {
  trx.pinData(trx.cid());  // will throw when it fails

  // Create a conditionBuilder that manages the AstNodes for querying
  aql::EdgeConditionBuilderContainer condBuilder;
  condBuilder.setVertexId(id);

  aql::Variable const* var = condBuilder.getVariable();

  switch (direction) {
    case TRI_EDGE_IN:
      readCursor(condBuilder.getInboundCondition(), var, collectionName, trx,
                 cb);
      break;
    case TRI_EDGE_OUT:
      readCursor(condBuilder.getOutboundCondition(), var, collectionName, trx,
                 cb);
      break;
    case TRI_EDGE_ANY:
      // We have to call both directions
      readCursor(condBuilder.getInboundCondition(), var, collectionName, trx,
                 cb);
      readCursor(condBuilder.getOutboundCondition(), var, collectionName, trx,
                 cb);
      break;
  }
  return true;
}

bool RestEdgesHandler::parseDirection(TRI_edge_direction_e& direction) {
  bool found;
  std::string dir = _request->value("direction", found);

  if (!found || dir.empty()) {
    dir = "any";
  }

  std::string dirString(dir);

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
  return true;
}

bool RestEdgesHandler::validateCollection(std::string const& name) {
  CollectionNameResolver resolver(_vocbase);
  TRI_col_type_e colType = resolver.getCollectionTypeCluster(name);
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
  if (!validateCollection(collectionName)) {
    return false;
  }

  TRI_edge_direction_e direction;
  if (!parseDirection(direction)) {
    return false;
  }

  bool found;
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
      transaction::StandaloneContext::Create(_vocbase), collectionName,
      AccessMode::Type::READ);

  // .............................................................................
  // inside read transaction
  // .............................................................................

  Result res = trx.begin();
  if (!res.ok()) {
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

  auto collection = trx.documentCollection();
  ManagedDocumentResult mmdr;
  std::unordered_set<DocumentIdentifierToken> foundTokens;
  auto cb = [&] (DocumentIdentifierToken const& token) {
    if (foundTokens.find(token) == foundTokens.end()) {
      if (collection->readDocument(&trx, token, mmdr)) {
        resultBuilder.add(VPackSlice(mmdr.vpack()));
      }
      scannedIndex++;
      // Mark edges we find
      foundTokens.emplace(token);
    }
  };

  // NOTE: collectionName is the shard-name in DBServer case
  bool ok = getEdgesForVertex(startVertex, collectionName, direction, trx, cb);
  resultBuilder.close();

  res = trx.finish(res);
  if (!ok) {
    // Error has been built internally
    return false;
  }

  if (!res.ok()) {
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
      parseVelocyPackBody(parseSuccess);

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
  if (!validateCollection(collectionName)) {
    return false;
  }

  TRI_edge_direction_e direction;
  if (!parseDirection(direction)) {
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
      transaction::StandaloneContext::Create(_vocbase), collectionName,
      AccessMode::Type::READ);

  // .............................................................................
  // inside read transaction
  // .............................................................................

  Result res = trx.begin();
  if (!res.ok()) {
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

  auto collection = trx.documentCollection();
  ManagedDocumentResult mmdr;
  std::unordered_set<DocumentIdentifierToken> foundTokens;
  auto cb = [&] (DocumentIdentifierToken const& token) {
    if (foundTokens.find(token) == foundTokens.end()) {
      if (collection->readDocument(&trx, token, mmdr)) {
        resultBuilder.add(VPackSlice(mmdr.vpack()));
      }
      scannedIndex++;
      // Mark edges we find
      foundTokens.emplace(token);
    }
  };

  for (auto const& it : VPackArrayIterator(body)) {
    if (it.isString()) {
      std::string startVertex = it.copyString();

      // We ignore if this fails
      getEdgesForVertex(startVertex, collectionName, direction, trx, cb);
    }
  }
  resultBuilder.close();

  res = trx.finish(res);
  if (!res.ok()) {
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
