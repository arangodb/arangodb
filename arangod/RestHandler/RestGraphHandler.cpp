////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "RestGraphHandler.h"

#include "Aql/Query.h"
#include "Basics/StringUtils.h"
#include "Graph/Graph.h"
#include "Graph/GraphManager.h"
#include "Graph/GraphOperations.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"

#include <velocypack/Collection.h>
#include <utility>

using namespace arangodb;
using namespace arangodb::graph;

RestGraphHandler::RestGraphHandler(application_features::ApplicationServer& server,
                                   GeneralRequest* request, GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response), _gmngr(_vocbase) {}

RestStatus RestGraphHandler::execute() {
  Result res = executeGharial();
  if (res.fail()) {
    TRI_ASSERT(!_response->isResponseEmpty());
    return RestStatus::FAIL;
  }
  // The url is required to properly generate the result!
  return RestStatus::DONE;
}

Result RestGraphHandler::returnError(ErrorCode errorNumber) {
  auto res = Result(errorNumber);
  generateError(res);
  return res;
}

Result RestGraphHandler::returnError(ErrorCode errorNumber, std::string_view message) {
  auto res = Result(errorNumber, message);
  generateError(res);
  return res;
}

Result RestGraphHandler::executeGharial() {
  auto suffix = request()->suffixes().begin();
  auto end = request()->suffixes().end();

  auto getNextSuffix = [&suffix]() {
    return basics::StringUtils::urlDecodePath(*suffix++);
  };

  auto noMoreSuffixes = [&suffix, &end]() { return suffix == end; };

  if (noMoreSuffixes()) {
    // /_api/gharial
    return graphsAction();
  }

  std::string const& graphName = getNextSuffix();

  std::unique_ptr<Graph> graph = getGraph(graphName);

  // Guaranteed
  TRI_ASSERT(graph != nullptr);

  if (noMoreSuffixes()) {
    // /_api/gharial/{graph-name}
    return graphAction(*(graph.get()));
  }

  std::string const& collType = getNextSuffix();

  const char* vertex = "vertex";
  const char* edge = "edge";
  if (collType != vertex && collType != edge) {
    return returnError(TRI_ERROR_HTTP_NOT_FOUND);
  }

  if (noMoreSuffixes()) {
    if (collType == vertex) {
      // /_api/gharial/{graph-name}/vertex
      return vertexSetsAction(*(graph.get()));
    } else if (collType == edge) {
      // /_api/gharial/{graph-name}/edge
      return edgeSetsAction(*(graph.get()));
    }
  }

  std::string const& setName = getNextSuffix();

  // TODO Add tests for this, especially with existing collections & vertices
  // where the collection is only missing in the graph.
  // TODO The existing tests seem to be inconsistent about this:
  // e.g., deleting a non-existent vertex collection is expected to throw
  // TRI_ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST but reading a vertex of a
  // non-existent collection is expected to throw
  // ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.
  // This is commented out until the tests are changed.
  // TODO The existing API seems to ignore the type of the collection for
  // most operations. So fetching an edge via
  // /_api/gharial/{graph}/vertex/{coll}/{key} works just fine. Should this be
  // changed? One way or the other, make sure there are tests for the desired
  // behavior!
  /*
    if (collType == vertex) {
      if (graph->vertexCollections().find(setName) ==
    graph->vertexCollections().end()) {
        generateError(TRI_ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST);
        return RestStatus::DONE;
      }
    } else if (collType == edge) {
      if (graph->edgeCollections().find(setName) ==
    graph->edgeCollections().end()) {
        generateError(TRI_ERROR_GRAPH_EDGE_COL_DOES_NOT_EXIST);
        return RestStatus::DONE;
      }
    }
  */

  if (noMoreSuffixes()) {
    if (collType == vertex) {
      // /_api/gharial/{graph-name}/vertex/{collection-name}
      return vertexSetAction(*(graph.get()), setName);
    } else if (collType == edge) {
      // /_api/gharial/{graph-name}/edge/{definition-name}
      return edgeSetAction(*(graph.get()), setName);
    }
  }

  std::string const& elementKey = getNextSuffix();

  if (noMoreSuffixes()) {
    if (collType == vertex) {
      // /_api/gharial/{graph-name}/vertex/{collection-name}/{vertex-key}
      return vertexAction(*(graph.get()), setName, elementKey);
    } else if (collType == edge) {
      // /_api/gharial/{graph-name}/edge/{definition-name}/{edge-key}
      return edgeAction(*(graph.get()), setName, elementKey);
    }
  }

  return returnError(TRI_ERROR_HTTP_NOT_FOUND);
}

Result RestGraphHandler::graphAction(Graph& graph) {
  switch (request()->requestType()) {
    case RequestType::GET:
      return graphActionReadGraphConfig(graph);
    case RequestType::DELETE_REQ:
      return graphActionRemoveGraph(graph);
    default:;
  }
  return returnError(TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
}

Result RestGraphHandler::graphsAction() {
  switch (request()->requestType()) {
    case RequestType::GET:
      return graphActionReadGraphs();
    case RequestType::POST:
      return graphActionCreateGraph();
    default:;
  }
  return returnError(TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
}

Result RestGraphHandler::vertexSetsAction(Graph& graph) {
  switch (request()->requestType()) {
    case RequestType::GET:
      return graphActionReadConfig(graph, TRI_COL_TYPE_DOCUMENT, GraphProperty::VERTICES);
    case RequestType::POST:
      return modifyVertexDefinition(graph, VertexDefinitionAction::CREATE, "");
    default:;
  }
  return returnError(TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
}

Result RestGraphHandler::edgeSetsAction(Graph& graph) {
  switch (request()->requestType()) {
    case RequestType::GET:
      return graphActionReadConfig(graph, TRI_COL_TYPE_EDGE, GraphProperty::EDGES);
    case RequestType::POST:
      return createEdgeDefinition(graph);
    default:;
  }
  return returnError(TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
}

Result RestGraphHandler::edgeSetAction(Graph& graph, const std::string& edgeDefinitionName) {
  switch (request()->requestType()) {
    case RequestType::POST:
      return edgeActionCreate(graph, edgeDefinitionName);
    case RequestType::PUT:
      return editEdgeDefinition(graph, edgeDefinitionName);
    case RequestType::DELETE_REQ:
      return removeEdgeDefinition(graph, edgeDefinitionName);
    default:;
  }
  return returnError(TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
}

Result RestGraphHandler::vertexSetAction(Graph& graph, const std::string& vertexCollectionName) {
  switch (request()->requestType()) {
    case RequestType::POST:
      return vertexActionCreate(graph, vertexCollectionName);
    case RequestType::DELETE_REQ:
      return modifyVertexDefinition(graph, VertexDefinitionAction::REMOVE, vertexCollectionName);
    default:;
  }
  return returnError(TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
}

Result RestGraphHandler::vertexAction(Graph& graph, const std::string& vertexCollectionName,
                                      const std::string& vertexKey) {
  switch (request()->requestType()) {
    case RequestType::GET: {
      vertexActionRead(graph, vertexCollectionName, vertexKey);
      return {TRI_ERROR_NO_ERROR};
    }
    case RequestType::PATCH:
      return vertexActionUpdate(graph, vertexCollectionName, vertexKey);
    case RequestType::PUT:
      return vertexActionReplace(graph, vertexCollectionName, vertexKey);
    case RequestType::DELETE_REQ:
      return vertexActionRemove(graph, vertexCollectionName, vertexKey);
    default:;
  }
  return returnError(TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
}

Result RestGraphHandler::edgeAction(Graph& graph, const std::string& edgeDefinitionName,
                                    const std::string& edgeKey) {
  switch (request()->requestType()) {
    case RequestType::GET:
      edgeActionRead(graph, edgeDefinitionName, edgeKey);
      return {TRI_ERROR_NO_ERROR};
    case RequestType::DELETE_REQ:
      return edgeActionRemove(graph, edgeDefinitionName, edgeKey);
    case RequestType::PATCH:
      return edgeActionUpdate(graph, edgeDefinitionName, edgeKey);
    case RequestType::PUT:
      return edgeActionReplace(graph, edgeDefinitionName, edgeKey);
    default:;
  }
  return returnError(TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
}

void RestGraphHandler::vertexActionRead(Graph& graph, std::string const& collectionName,
                                        std::string const& key) {
  // check for an etag
  bool isValidRevision;
  RevisionId ifNoneRid = extractRevision("if-none-match", isValidRevision);
  if (!isValidRevision) {
    ifNoneRid = RevisionId::max();  // an impossible rev, so precondition failed will happen
  }

  auto maybeRev = handleRevision();

  auto ctx = createTransactionContext(AccessMode::Type::READ);
  GraphOperations gops{graph, _vocbase, ctx};
  OperationResult result = gops.getVertex(collectionName, key, maybeRev);

  if (!result.ok()) {
    generateTransactionError(collectionName, result, key, maybeRev.value_or(RevisionId::none()));
    return;
  }

  if (ifNoneRid.isSet()) {
    RevisionId const rid = RevisionId::fromSlice(result.slice());
    if (ifNoneRid == rid) {
      generateNotModified(rid);
      return;
    }
  }

  // use default options
  generateVertexRead(result.slice(), *ctx->getVPackOptions());
}

/// @brief generate response object: { error, code, vertex }
void RestGraphHandler::generateVertexRead(VPackSlice vertex, VPackOptions const& options) {
  vertex = vertex.resolveExternal();
  resetResponse(rest::ResponseCode::OK);
  addEtagHeader(vertex.get(StaticStrings::RevString));
  generateResultWithField("vertex", vertex, options);
}

/// @brief generate response object: { error, code, edge }
void RestGraphHandler::generateEdgeRead(VPackSlice edge, VPackOptions const& options) {
  edge = edge.resolveExternal();
  resetResponse(rest::ResponseCode::OK);
  addEtagHeader(edge.get(StaticStrings::RevString));
  generateResultWithField("edge", edge, options);
}

/// @brief generate response object: { error, code, removed, old? }
/// "old" is omitted if old is a NoneSlice.
void RestGraphHandler::generateRemoved(bool removed, bool wasSynchronous,
                                       VPackSlice old, VPackOptions const& options) {
  ResponseCode code;
  if (wasSynchronous) {
    code = rest::ResponseCode::OK;
  } else {
    code = rest::ResponseCode::ACCEPTED;
  }
  resetResponse(code);
  VPackBuilder obj;
  obj.add(VPackValue(VPackValueType::Object, true));
  obj.add("removed", VPackValue(removed));
  if (!old.isNone()) {
    obj.add("old", old);
  }
  obj.close();
  generateResultMergedWithObject(obj.slice(), options);
}

/// @brief generate response object: { error, code, removed, old? }
/// "old" is omitted if old is a NoneSlice.
void RestGraphHandler::generateGraphRemoved(bool removed, bool wasSynchronous,
                                            VPackOptions const& options) {
  ResponseCode code = rest::ResponseCode::ACCEPTED;
#if 0
  // TODO fix this in a major release upgrade
  if (wasSynchronous) {
    code = rest::ResponseCode::CREATED;
  }
#endif
  resetResponse(code);
  VPackBuilder obj;
  obj.add(VPackValue(VPackValueType::Object, true));
  obj.add("removed", VPackValue(removed));
  obj.close();
  generateResultMergedWithObject(obj.slice(), options);
}

void RestGraphHandler::generateGraphConfig(VPackSlice slice, VPackOptions const& options) {
  resetResponse(rest::ResponseCode::OK);
  generateResultMergedWithObject(slice, options);
}

void RestGraphHandler::generateCreatedGraphConfig(bool wasSynchronous, VPackSlice slice,
                                                  VPackOptions const& options) {
  ResponseCode code;
  if (wasSynchronous) {
    code = rest::ResponseCode::CREATED;
  } else {
    code = rest::ResponseCode::ACCEPTED;
  }
  resetResponse(code);
  addEtagHeader(slice.get("graph").get(StaticStrings::RevString));
  generateResultMergedWithObject(slice, options);
}

void RestGraphHandler::generateCreatedEdgeDefinition(bool wasSynchronous, VPackSlice slice,
                                                     VPackOptions const& options) {
  ResponseCode code = rest::ResponseCode::ACCEPTED;
#if 0
  // TODO: fix this in a major upgrade release
  if (wasSynchronous) {
    code = rest::ResponseCode::CREATED;
  }
#endif
  resetResponse(code);
  addEtagHeader(slice.get("graph").get(StaticStrings::RevString));
  generateResultMergedWithObject(slice, options);
}

/// @brief generate response object: { error, code, vertex, old?, new? }
void RestGraphHandler::generateVertexModified(bool wasSynchronous, VPackSlice resultSlice,
                                              const velocypack::Options& options) {
  generateModified(TRI_COL_TYPE_DOCUMENT, wasSynchronous, resultSlice, options);
}

/// @brief generate response object: { error, code, vertex }
void RestGraphHandler::generateVertexCreated(bool wasSynchronous, VPackSlice resultSlice,
                                             const velocypack::Options& options) {
  generateCreated(TRI_COL_TYPE_DOCUMENT, wasSynchronous, resultSlice, options);
}

/// @brief generate response object: { error, code, edge, old?, new? }
void RestGraphHandler::generateEdgeModified(bool wasSynchronous, VPackSlice resultSlice,
                                            const velocypack::Options& options) {
  generateModified(TRI_COL_TYPE_EDGE, wasSynchronous, resultSlice, options);
}

/// @brief generate response object: { error, code, edge }
void RestGraphHandler::generateEdgeCreated(bool wasSynchronous, VPackSlice resultSlice,
                                           const velocypack::Options& options) {
  generateCreated(TRI_COL_TYPE_EDGE, wasSynchronous, resultSlice, options);
}

/// @brief generate response object: { error, code, vertex/edge, old?, new? }
// TODO Maybe a class enum in Graph.h to discern Vertex/Edge is better than
// abusing document/edge collection types?
void RestGraphHandler::generateModified(TRI_col_type_e colType,
                                        bool wasSynchronous, VPackSlice resultSlice,
                                        const velocypack::Options& options) {
  TRI_ASSERT(colType == TRI_COL_TYPE_DOCUMENT || colType == TRI_COL_TYPE_EDGE);
  if (wasSynchronous) {
    resetResponse(rest::ResponseCode::OK);
  } else {
    resetResponse(rest::ResponseCode::ACCEPTED);
  }
  addEtagHeader(resultSlice.get(StaticStrings::RevString));

  const char* objectTypeName = "_";
  if (colType == TRI_COL_TYPE_DOCUMENT) {
    objectTypeName = "vertex";
  } else if (colType == TRI_COL_TYPE_EDGE) {
    objectTypeName = "edge";
  }

  VPackBuilder objectBuilder =
      velocypack::Collection::remove(resultSlice,
                              std::unordered_set<std::string>{"old", "new"});
  // Note: This doesn't really contain the object, only _id, _key, _rev, _oldRev
  VPackSlice objectSlice = objectBuilder.slice();
  VPackSlice oldSlice = resultSlice.get("old");
  VPackSlice newSlice = resultSlice.get("new");

  VPackBuilder obj;
  obj.add(VPackValue(VPackValueType::Object, true));
  obj.add(objectTypeName, objectSlice);
  if (!oldSlice.isNone()) {
    obj.add("old", oldSlice);
  }
  if (!newSlice.isNone()) {
    obj.add("new", newSlice);
  }
  obj.close();
  generateResultMergedWithObject(obj.slice(), options);
}

/// @brief generate response object: { error, code, vertex/edge }
// TODO Maybe a class enum in Graph.h to discern Vertex/Edge is better than
// abusing document/edge collection types?
void RestGraphHandler::generateCreated(TRI_col_type_e colType,
                                       bool wasSynchronous, VPackSlice resultSlice,
                                       const velocypack::Options& options) {
  TRI_ASSERT(colType == TRI_COL_TYPE_DOCUMENT || colType == TRI_COL_TYPE_EDGE);
  if (wasSynchronous) {
    resetResponse(rest::ResponseCode::CREATED);
  } else {
    resetResponse(rest::ResponseCode::ACCEPTED);
  }
  addEtagHeader(resultSlice.get(StaticStrings::RevString));

  const char* objectTypeName = "_";
  if (colType == TRI_COL_TYPE_DOCUMENT) {
    objectTypeName = "vertex";
  } else if (colType == TRI_COL_TYPE_EDGE) {
    objectTypeName = "edge";
  }

  VPackBuilder objectBuilder =
      velocypack::Collection::remove(resultSlice,
                              std::unordered_set<std::string>{StaticStrings::Old, StaticStrings::New});
  // Note: This doesn't really contain the object, only _id, _key, _rev, _oldRev
  VPackSlice objectSlice = objectBuilder.slice();
  VPackSlice newSlice = resultSlice.get(StaticStrings::New);

  VPackBuilder obj;
  obj.add(VPackValue(VPackValueType::Object, true));
  obj.add(objectTypeName, objectSlice);
  if (!newSlice.isNone()) {
    obj.add("new", newSlice);
  }
  obj.close();
  generateResultMergedWithObject(obj.slice(), options);
}

/// @brief generate response object: { error, code, key: value }
void RestGraphHandler::generateResultWithField(std::string const& key, VPackSlice value,
                                               VPackOptions const& options) {
  VPackBuilder obj;
  obj.add(VPackValue(VPackValueType::Object, true));
  obj.add(key, value);
  obj.close();
  generateResultMergedWithObject(obj.slice(), options);
}

/// @brief generate response object: MERGE({ error, code }, obj)
void RestGraphHandler::generateResultMergedWithObject(VPackSlice obj,
                                                      VPackOptions const& options) {
  _response->setContentType(_request->contentTypeResponse());

  try {
    VPackBuilder result;
    result.add(VPackValue(VPackValueType::Object, true));
    result.add(StaticStrings::Error, VPackValue(false));
    result.add(StaticStrings::Code,
               VPackValue(static_cast<int>(_response->responseCode())));
    result.close();
    VPackBuilder merged = velocypack::Collection::merge(result.slice(), obj, false, false);

    writeResult(merged.slice(), options);
  } catch (...) {
    // Building the error response failed
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  "cannot generate output");
  }
}

// TODO this is nearly exactly the same as vertexActionRead. reuse somehow?
void RestGraphHandler::edgeActionRead(Graph& graph, const std::string& definitionName,
                                      const std::string& key) {
  // check for an etag
  bool isValidRevision;
  RevisionId ifNoneRid = extractRevision("if-none-match", isValidRevision);
  if (!isValidRevision) {
    ifNoneRid = RevisionId::max();  // an impossible rev, so precondition failed will happen
  }

  auto maybeRev = handleRevision();

  auto ctx = createTransactionContext(AccessMode::Type::READ);
  GraphOperations gops{graph, _vocbase, ctx};
  OperationResult result = gops.getEdge(definitionName, key, maybeRev);

  if (result.fail()) {
    generateTransactionError(/*collection*/"", result, key, maybeRev.value_or(RevisionId::none()));
    return;
  }

  if (ifNoneRid.isSet()) {
    RevisionId const rid = RevisionId::fromSlice(result.slice());
    if (ifNoneRid == rid) {
      generateNotModified(rid);
      return;
    }
  }

  generateEdgeRead(result.slice(), *ctx->getVPackOptions());
}

std::unique_ptr<Graph> RestGraphHandler::getGraph(const std::string& graphName) {
  auto graphResult = _gmngr.lookupGraphByName(graphName);
  if (graphResult.fail()) {
    THROW_ARANGO_EXCEPTION(std::move(graphResult).result());
  }
  TRI_ASSERT(graphResult.get() != nullptr);
  return std::move(graphResult.get());
}

// TODO this is very similar to (edge|vertex)ActionRead. find a way to reduce
// the duplicate code.
// TODO The tests check that, if "returnOld: true" is passed,  the result
// contains the old value in the field "old". This is not documented in
// HTTP/Gharial!
Result RestGraphHandler::edgeActionRemove(Graph& graph, const std::string& definitionName,
                                          const std::string& key) {
  bool waitForSync = _request->parsedValue(StaticStrings::WaitForSyncString, false);

  bool returnOld = _request->parsedValue(StaticStrings::ReturnOldString, false);

  auto maybeRev = handleRevision();

  auto ctx = createTransactionContext(AccessMode::Type::WRITE);
  GraphOperations gops{graph, _vocbase, ctx};

  OperationResult result =
      gops.removeEdge(definitionName, key, maybeRev, waitForSync, returnOld);

  if (result.fail()) {
    generateTransactionError(/*collection*/"", result, key, maybeRev.value_or(RevisionId::none()));
    return result.result;
  }

  generateRemoved(true, result.options.waitForSync, result.slice().get(StaticStrings::Old),
                  *ctx->getVPackOptions());

  return Result();
}

/// @brief If rev is a string, set the Etag header to its value.
/// rev is expected to be either None or a string.
void RestGraphHandler::addEtagHeader(velocypack::Slice rev) {
  TRI_ASSERT(rev.isString() || rev.isNone());
  if (rev.isString()) {
    _response->setHeaderNC(StaticStrings::Etag, rev.copyString());
  }
}

Result RestGraphHandler::vertexActionUpdate(graph::Graph& graph,
                                            const std::string& collectionName,
                                            const std::string& key) {
  return vertexModify(graph, collectionName, key, true);
}

Result RestGraphHandler::vertexActionReplace(graph::Graph& graph,
                                             const std::string& collectionName,
                                             const std::string& key) {
  return vertexModify(graph, collectionName, key, false);
}

Result RestGraphHandler::vertexActionCreate(graph::Graph& graph,
                                            const std::string& collectionName) {
  return vertexCreate(graph, collectionName);
}

Result RestGraphHandler::edgeActionUpdate(graph::Graph& graph, const std::string& collectionName,
                                          const std::string& key) {
  return edgeModify(graph, collectionName, key, true);
}

Result RestGraphHandler::edgeActionReplace(graph::Graph& graph, const std::string& collectionName,
                                           const std::string& key) {
  return edgeModify(graph, collectionName, key, false);
}

Result RestGraphHandler::edgeModify(graph::Graph& graph, const std::string& collectionName,
                                    const std::string& key, bool isPatch) {
  return documentModify(graph, collectionName, key, isPatch, TRI_COL_TYPE_EDGE);
}

Result RestGraphHandler::edgeCreate(graph::Graph& graph, const std::string& collectionName) {
  return documentCreate(graph, collectionName, TRI_COL_TYPE_EDGE);
}

Result RestGraphHandler::edgeActionCreate(graph::Graph& graph, const std::string& collectionName) {
  return edgeCreate(graph, collectionName);
}

Result RestGraphHandler::vertexModify(graph::Graph& graph, const std::string& collectionName,
                                      const std::string& key, bool isPatch) {
  return documentModify(graph, collectionName, key, isPatch, TRI_COL_TYPE_DOCUMENT);
}

Result RestGraphHandler::vertexCreate(graph::Graph& graph, const std::string& collectionName) {
  return documentCreate(graph, collectionName, TRI_COL_TYPE_DOCUMENT);
}

// /_api/gharial/{graph-name}/edge/{definition-name}
Result RestGraphHandler::editEdgeDefinition(graph::Graph& graph,
                                            const std::string& edgeDefinitionName) {
  return modifyEdgeDefinition(graph, EdgeDefinitionAction::EDIT, edgeDefinitionName);
}

Result RestGraphHandler::createEdgeDefinition(graph::Graph& graph) {
  return modifyEdgeDefinition(graph, EdgeDefinitionAction::CREATE);
}

// /_api/gharial/{graph-name}/edge
Result RestGraphHandler::modifyEdgeDefinition(graph::Graph& graph, EdgeDefinitionAction action,
                                              std::string edgeDefinitionName) {
  // edgeDefinitionName == "" <=> action == CREATE
  TRI_ASSERT((action == EdgeDefinitionAction::CREATE) == edgeDefinitionName.empty());
  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    return {TRI_ERROR_BAD_PARAMETER, "unable to parse body"};
  }
  bool waitForSync = _request->parsedValue(StaticStrings::WaitForSyncString, false);
  bool dropCollections = _request->parsedValue(StaticStrings::GraphDropCollections, false);

  // simon: why is this part of el-cheapo ??
  auto ctx = createTransactionContext(AccessMode::Type::WRITE);
  GraphOperations gops{graph, _vocbase, ctx};
  OperationOptions options(_context);
  OperationResult result(Result(), options);

  if (action == EdgeDefinitionAction::CREATE) {
    result = gops.addEdgeDefinition(body, waitForSync);
  } else if (action == EdgeDefinitionAction::EDIT) {
    result = gops.editEdgeDefinition(body, waitForSync, edgeDefinitionName);
  } else if (action == EdgeDefinitionAction::REMOVE) {
    // TODO Does this get waitForSync? Not according to the documentation.
    // if not, remove the parameter from eraseEdgeDefinition. What about
    // add/edit?
    result = gops.eraseEdgeDefinition(waitForSync, edgeDefinitionName, dropCollections);
  } else {
    TRI_ASSERT(false);
  }

  if (result.fail()) {
    generateTransactionError(/*collection*/"", result);
    return result.result;
  }

  auto newGraph = getGraph(graph.name());
  TRI_ASSERT(newGraph != nullptr);
  VPackBuilder builder;
  builder.openObject();
  newGraph->graphForClient(builder);
  builder.close();

  generateCreatedEdgeDefinition(waitForSync, builder.slice(), *ctx->getVPackOptions());

  return Result();
}

Result RestGraphHandler::modifyVertexDefinition(graph::Graph& graph,
                                                VertexDefinitionAction action,
                                                std::string vertexDefinitionName) {
  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    return returnError(TRI_ERROR_BAD_PARAMETER, "unable to parse body");
  }

  // TODO maybe merge this function with modifyEdgeDefinition?
  bool waitForSync = _request->parsedValue(StaticStrings::WaitForSyncString, false);
  bool dropCollection = _request->parsedValue(StaticStrings::GraphDropCollection, false);
  bool createCollection =
      _request->parsedValue(StaticStrings::GraphCreateCollection, true);

  auto ctx = createTransactionContext(AccessMode::Type::WRITE);
  GraphOperations gops{graph, _vocbase, ctx};
  OperationOptions options(_context);
  OperationResult result(Result(), options);

  if (action == VertexDefinitionAction::CREATE) {
    result = gops.addOrphanCollection(body, waitForSync, createCollection);
  } else if (action == VertexDefinitionAction::REMOVE) {
    result = gops.eraseOrphanCollection(waitForSync, vertexDefinitionName, dropCollection);
  } else {
    TRI_ASSERT(false);
  }

  if (result.fail()) {
    generateTransactionError(/*collection*/"", result);
    return result.result;
  }

  auto newGraph = getGraph(graph.name());
  TRI_ASSERT(newGraph != nullptr);
  VPackBuilder builder;
  builder.openObject();
  newGraph->graphForClient(builder);
  builder.close();

  generateCreatedEdgeDefinition(waitForSync, builder.slice(), *ctx->getVPackOptions());

  return Result();
}
Result RestGraphHandler::removeEdgeDefinition(graph::Graph& graph,
                                              const std::string& edgeDefinitionName) {
  return modifyEdgeDefinition(graph, EdgeDefinitionAction::REMOVE, edgeDefinitionName);
}

// TODO The tests check that, if "returnOld: true" is passed,  the result
// contains the old value in the field "old"; and if "returnNew: true" is
// passed, the field "new" contains the new value (along with "vertex"!).
// This is not documented in HTTP/Gharial!
// TODO the document API also supports mergeObjects, silent and ignoreRevs;
// should gharial, too?
Result RestGraphHandler::documentModify(graph::Graph& graph, const std::string& collectionName,
                                        const std::string& key, bool isPatch,
                                        TRI_col_type_e colType) {
  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    return returnError(TRI_ERROR_BAD_PARAMETER, "unable to parse body");
  }

  bool waitForSync = _request->parsedValue(StaticStrings::WaitForSyncString, false);
  bool returnNew = _request->parsedValue(StaticStrings::ReturnNewString, false);
  bool returnOld = _request->parsedValue(StaticStrings::ReturnOldString, false);
  // Note: the default here differs from the one in the RestDoumentHandler
  bool keepNull = _request->parsedValue(StaticStrings::KeepNullString, true);

  // extract the revision, if single document variant and header given:
  std::unique_ptr<VPackBuilder> builder;
  auto maybeRev = handleRevision();

  auto ctx = createTransactionContext(AccessMode::Type::WRITE);
  GraphOperations gops{graph, _vocbase, ctx};

  OperationOptions options(_context);
  OperationResult result(Result(), options);
  // TODO get rid of this branching, rather use several functions and reuse the
  // common code another way.
  if (isPatch && colType == TRI_COL_TYPE_DOCUMENT) {
    result = gops.updateVertex(collectionName, key, body, maybeRev, waitForSync,
                               returnOld, returnNew, keepNull);
  } else if (!isPatch && colType == TRI_COL_TYPE_DOCUMENT) {
    result = gops.replaceVertex(collectionName, key, body, maybeRev,
                                waitForSync, returnOld, returnNew, keepNull);
  } else if (isPatch && colType == TRI_COL_TYPE_EDGE) {
    result = gops.updateEdge(collectionName, key, body, maybeRev, waitForSync,
                             returnOld, returnNew, keepNull);
  } else if (!isPatch && colType == TRI_COL_TYPE_EDGE) {
    result = gops.replaceEdge(collectionName, key, body, maybeRev, waitForSync,
                              returnOld, returnNew, keepNull);
  } else {
    TRI_ASSERT(false);
  }

  if (result.fail()) {
    // simon: do not pass in collection name, otherwise HTTP return code
    //        changes to 404 in for unknown _to/_from collection -> breaks API
    generateTransactionError(/*cname*/"", result, key, maybeRev.value_or(RevisionId::none()));
    return result.result;
  }

  switch (colType) {
    case TRI_COL_TYPE_DOCUMENT:
      generateVertexModified(result.options.waitForSync, result.slice(),
                             *ctx->getVPackOptions());
      break;
    case TRI_COL_TYPE_EDGE:
      generateEdgeModified(result.options.waitForSync, result.slice(),
                           *ctx->getVPackOptions());
      break;
    default:
      TRI_ASSERT(false);
  }

  return TRI_ERROR_NO_ERROR;
}

Result RestGraphHandler::documentCreate(graph::Graph& graph, std::string const& collectionName,
                                        TRI_col_type_e colType) {
  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    return returnError(TRI_ERROR_BAD_PARAMETER, "unable to parse body");
  }

  if (!body.isObject()) {
    return returnError(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  bool waitForSync = _request->parsedValue(StaticStrings::WaitForSyncString, false);
  bool returnNew = _request->parsedValue(StaticStrings::ReturnNewString, false);

  auto ctx = createTransactionContext(AccessMode::Type::WRITE);
  GraphOperations gops{graph, _vocbase, ctx};

  OperationOptions options(_context);
  OperationResult result(Result(), options);
  if (colType == TRI_COL_TYPE_DOCUMENT) {
    result = gops.createVertex(collectionName, body, waitForSync, returnNew);
  } else if (colType == TRI_COL_TYPE_EDGE) {
    result = gops.createEdge(collectionName, body, waitForSync, returnNew);
  } else {
    TRI_ASSERT(false);
  }

  if (result.fail()) {
    // need to call more detailed constructor here
    generateTransactionError(collectionName, result);
  } else {
    switch (colType) {
      case TRI_COL_TYPE_DOCUMENT:
        generateVertexCreated(result.options.waitForSync, result.slice(),
                              *ctx->getVPackOptions());
        break;
      case TRI_COL_TYPE_EDGE:
        generateEdgeCreated(result.options.waitForSync, result.slice(),
                            *ctx->getVPackOptions());
        break;
      default:
        TRI_ASSERT(false);
    }
  }

  return result.result;
}

Result RestGraphHandler::vertexActionRemove(graph::Graph& graph,
                                            const std::string& collectionName,
                                            const std::string& key) {
  bool waitForSync = _request->parsedValue(StaticStrings::WaitForSyncString, false);

  bool returnOld = _request->parsedValue(StaticStrings::ReturnOldString, false);

  auto maybeRev = handleRevision();

  auto ctx = createTransactionContext(AccessMode::Type::WRITE);
  GraphOperations gops{graph, _vocbase, ctx};

  OperationResult result =
      gops.removeVertex(collectionName, key, maybeRev, waitForSync, returnOld);

  if (result.fail()) {
    generateTransactionError(collectionName, result, key, maybeRev.value_or(RevisionId::none()));
    return result.result;
  }

  generateRemoved(true, result.options.waitForSync, result.slice().get(StaticStrings::Old),
                  *ctx->getVPackOptions());

  return Result();
}

Result RestGraphHandler::graphActionReadGraphConfig(graph::Graph const& graph) {
  auto ctx = std::make_shared<transaction::StandaloneContext>(_vocbase);
  VPackBuilder builder;
  builder.openObject();
  graph.graphForClient(builder);
  builder.close();
  generateGraphConfig(builder.slice(), *ctx->getVPackOptions());

  return Result();
}

Result RestGraphHandler::graphActionRemoveGraph(graph::Graph const& graph) {
  bool waitForSync = _request->parsedValue(StaticStrings::WaitForSyncString, false);
  bool dropCollections = _request->parsedValue(StaticStrings::GraphDropCollections, false);

  OperationResult result = _gmngr.removeGraph(graph, waitForSync, dropCollections);

  if (result.fail()) {
    generateTransactionError(/*collection*/"", result);
    return result.result;
  }

  auto ctx = std::make_shared<transaction::StandaloneContext>(_vocbase);
  generateGraphRemoved(true, result.options.waitForSync, *ctx->getVPackOptions());

  return Result();
}

Result RestGraphHandler::graphActionCreateGraph() {
  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    return returnError(TRI_ERROR_BAD_PARAMETER, "unable to parse body");
  }
  bool waitForSync = _request->parsedValue(StaticStrings::WaitForSyncString, false);

  {
    OperationResult result = _gmngr.createGraph(body, waitForSync);

    if (result.fail()) {
      generateTransactionError(/*collection*/"", result);
      return result.result;
    }
  }

  std::string graphName = body.get(StaticStrings::DataSourceName).copyString();

  auto ctx = std::make_shared<transaction::StandaloneContext>(_vocbase);
  std::unique_ptr<Graph const> graph = getGraph(graphName);
  TRI_ASSERT(graph != nullptr);

  VPackBuilder builder;
  builder.openObject();
  graph->graphForClient(builder);
  builder.close();

  generateCreatedGraphConfig(waitForSync, builder.slice(), *ctx->getVPackOptions());

  return Result();
}

Result RestGraphHandler::graphActionReadGraphs() {
  auto ctx = std::make_shared<transaction::StandaloneContext>(_vocbase);

  VPackBuilder builder;
  _gmngr.readGraphs(builder);

  generateGraphConfig(builder.slice(), *ctx->getVPackOptions());

  return Result();
}

Result RestGraphHandler::graphActionReadConfig(graph::Graph const& graph, TRI_col_type_e colType,
                                               GraphProperty property) {
  VPackBuilder builder;

  if (colType == TRI_COL_TYPE_DOCUMENT && property == GraphProperty::VERTICES) {
    graph.verticesToVpack(builder);
  } else if (colType == TRI_COL_TYPE_EDGE && property == GraphProperty::EDGES) {
    graph.edgesToVpack(builder);
  } else {
    TRI_ASSERT(false);
  }

  auto ctx = std::make_shared<transaction::StandaloneContext>(_vocbase);

  generateGraphConfig(builder.slice(), *ctx->getVPackOptions());

  return Result();
}

RequestLane RestGraphHandler::lane() const { return RequestLane::CLIENT_SLOW; }

std::optional<RevisionId> RestGraphHandler::handleRevision() const {
  bool isValidRevision;
  RevisionId revision = extractRevision("if-match", isValidRevision);
  if (!isValidRevision) {
    revision = RevisionId::max();  // an impossible revision, so precondition failed
  }
  if (revision.empty() || revision == RevisionId::max()) {
    bool found = false;
    std::string const& revString = _request->value("rev", found);
    if (found) {
      revision = RevisionId::fromString(revString.data(), revString.size(), false);
    }
  }
  return revision.isSet() ? std::optional{revision} : std::nullopt;
}

