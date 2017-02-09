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
#include "Utils/CollectionNameResolver.h"
#include "Utils/OperationCursor.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"

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
    default:
      generateNotImplemented("ILLEGAL " + EDGES_PATH);
      break;
  }

  // this handler is done
  return RestStatus::DONE;
}

void RestEdgesHandler::readCursor(aql::AstNode* condition,
    aql::Variable const* var,
    std::string const& collectionName,
    SingleCollectionTransaction& trx,
    VPackBuilder& result,
    std::function<void(DocumentIdentifierToken const&)> cb) {

  Transaction::IndexHandle indexId;
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

  while (cursor->getMore(cb, 1000)) {
  }
}


bool RestEdgesHandler::getEdgesForVertex(
    std::string const& id, std::string const& collectionName,
    TRI_edge_direction_e direction, SingleCollectionTransaction& trx,
    VPackBuilder& result, size_t& scannedIndex, size_t& filtered) {
  TRI_ASSERT(result.isOpenArray());
  trx.orderDitch(trx.cid());  // will throw when it fails

  ManagedDocumentResult mmdr;
  auto collection = trx.documentCollection();

  // Create a conditionBuilder that manages the AstNodes for querying
  aql::EdgeConditionBuilderContainer condBuilder;
  condBuilder.setVertexId(id);

  aql::Variable const* var = condBuilder.getVariable();

  switch (direction) {
    case TRI_EDGE_IN: 
      {
        auto cb = [&] (DocumentIdentifierToken const& token) {
          if (collection->readDocument(&trx, mmdr, token)) {
            result.add(VPackSlice(mmdr.vpack()));
          }
          scannedIndex++;
        };
        readCursor(condBuilder.getInboundCondition(), var, collectionName, trx,
                   result, cb);
        break;
      }
    case TRI_EDGE_OUT: 
      {
        auto cb = [&] (DocumentIdentifierToken const& token) {
          if (collection->readDocument(&trx, mmdr, token)) {
            result.add(VPackSlice(mmdr.vpack()));
          }
          scannedIndex++;
        };
        readCursor(condBuilder.getOutboundCondition(), var, collectionName, trx,
                   result, cb);
        break;
      }
    case TRI_EDGE_ANY: 
      // We have to call both directions AND we have to unify reverse direction
      {
        std::unordered_set<DocumentIdentifierToken> found;
        auto inboundCB = [&] (DocumentIdentifierToken const& token) {
          if (collection->readDocument(&trx, mmdr, token)) {
            result.add(VPackSlice(mmdr.vpack()));
            // Mark edges we find
            found.emplace(token);
          }
          scannedIndex++;
        };
        auto outboundCB = [&] (DocumentIdentifierToken const& token) {
          if (found.find(token) == found.end()) {
            // Only add those tokens we have not found yet
            if (collection->readDocument(&trx, mmdr, token)) {
              result.add(VPackSlice(mmdr.vpack()));
            }
            scannedIndex++;
          }
        };
        readCursor(condBuilder.getInboundCondition(), var, collectionName, trx,
                   result, inboundCB);
        readCursor(condBuilder.getOutboundCondition(), var, collectionName, trx,
                   result, outboundCB);
        break;
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
      AccessMode::Type::READ);

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
