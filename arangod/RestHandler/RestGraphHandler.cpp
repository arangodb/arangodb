////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include <boost/optional.hpp>
#include <utility>

#include "Aql/Query.h"
#include "Basics/VelocyPackHelper.h"
#include "Graph/Graph.h"
#include "Graph/GraphCache.h"
#include "Graph/GraphManager.h"
#include "Graph/GraphOperations.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/V8Context.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/Graphs.h"

// TODO this is here for easy debugging during development. most log messages
// using this should be removed or at least have their log level reduced before
// this is merged.
/*#define S1(x) #x
#define S2(x) S1(x)
#define LOGPREFIX(func)                                                   \
  "[" << (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__) \
      << ":" S2(__LINE__) << "@" << func << "] "
*/

using namespace arangodb;
using namespace arangodb::graph;
using VelocyPackHelper = arangodb::basics::VelocyPackHelper;

RestGraphHandler::RestGraphHandler(GeneralRequest* request,
                                   GeneralResponse* response,
                                   GraphCache* graphCache_)
    : RestVocbaseBaseHandler(request, response), _graphCache(*graphCache_) {}

RestStatus RestGraphHandler::execute() {

  boost::optional<RestStatus> maybeResult;
  try {
    maybeResult = executeGharial();
  } catch (arangodb::basics::Exception& exception) {
    // reset some error messages to match the tests.
    // TODO it's possibly sane to change the tests to check for error codes
    // only instead
    switch (exception.code()) {
      case TRI_ERROR_GRAPH_NOT_FOUND:
        THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_NOT_FOUND);
      case TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND:
        THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
      default:
        throw exception;
    }
  };

  if (maybeResult) {
    return maybeResult.get();
  }

  RestStatus restStatus = RestStatus::FAIL;

  return restStatus;
}

// Note: boost::none indicates "not (yet) implemented".
// Error-handling for nonexistent routes is, for now, taken from the fallback.
// TODO as soon as this implements everything, just return a RestStatus.
// TODO get rid of RestStatus; it has no useful state.
boost::optional<RestStatus> RestGraphHandler::executeGharial() {
  auto suffix = request()->suffixes().begin();
  auto end = request()->suffixes().end();

  auto getNextSuffix = [&suffix]() { return *suffix++; };

  auto noMoreSuffixes = [&suffix, &end]() { return suffix == end; };

  if (noMoreSuffixes()) {
    // /_api/gharial
    return graphsAction();
  }

  std::string const& graphName = getNextSuffix();

  V8Context* v8Context = V8DealerFeature::DEALER->enterContext(
      &_vocbase, true /*allow use database*/);

  if (!v8Context) {
    generateError(Result(TRI_ERROR_INTERNAL, "could not acquire v8 context"));
    return RestStatus::DONE;
  }

  TRI_DEFER(V8DealerFeature::DEALER->exitContext(v8Context));

  // auto ctx = transaction::StandaloneContext::Create(_vocbase);
  auto ctx = transaction::V8Context::Create(_vocbase, true);
  // ctx->makeGlobal(); // TODO ???

  TRI_ASSERT(ctx);

  std::shared_ptr<Graph const> graph = getGraph(ctx, graphName);

  if (noMoreSuffixes()) {
    // /_api/gharial/{graph-name}
    return graphAction(graph);
  }

  std::string const& collType = getNextSuffix();

  const char* vertex = "vertex";
  const char* edge = "edge";
  if (collType != vertex && collType != edge) {
    return boost::none;
  }

  if (noMoreSuffixes()) {
    if (collType == vertex) {
      // /_api/gharial/{graph-name}/vertex
      return vertexSetsAction(graph);
    } else if (collType == edge) {
      // /_api/gharial/{graph-name}/edge
      return edgeSetsAction(graph);
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
  // behaviour!
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
      return vertexSetAction(graph, setName);
    } else if (collType == edge) {
      // /_api/gharial/{graph-name}/edge/{definition-name}
      return edgeSetAction(graph, setName);
    }
  }

  std::string const& elementKey = getNextSuffix();

  if (noMoreSuffixes()) {
    if (collType == vertex) {
      // /_api/gharial/{graph-name}/vertex/{collection-name}/{vertex-key}
      return vertexAction(graph, setName, elementKey);
    } else if (collType == edge) {
      // /_api/gharial/{graph-name}/edge/{definition-name}/{edge-key}
      return edgeAction(graph, setName, elementKey);
    }
  }

  // TODO This should be a 404
  return boost::none;
}

boost::optional<RestStatus> RestGraphHandler::graphAction(
    const std::shared_ptr<const Graph> graph) {
  switch (request()->requestType()) {
    case RequestType::GET:
      graphActionReadGraphConfig(graph);
      return RestStatus::DONE;
    case RequestType::DELETE_REQ:
      graphActionRemoveGraph(graph);
      return RestStatus::DONE;
    default:;
  }
  return boost::none;
}

boost::optional<RestStatus> RestGraphHandler::graphsAction() {
  switch (request()->requestType()) {
    case RequestType::GET:
      graphActionReadGraphs();
      return RestStatus::DONE;
    case RequestType::POST:
      graphActionCreateGraph();
      return RestStatus::DONE;
    default:;
  }
  return boost::none;
}

boost::optional<RestStatus> RestGraphHandler::vertexSetsAction(
    const std::shared_ptr<const Graph> graph) {

  switch (request()->requestType()) {
    case RequestType::GET:
      graphActionReadConfig(graph, TRI_COL_TYPE_DOCUMENT, GraphProperty::VERTICES);
      return RestStatus::DONE;
    case RequestType::POST:
      modifyVertexDefinition(graph, VertexDefinitionAction::CREATE, "");
      return RestStatus::DONE;
    default:;
  }
  return boost::none;
}

boost::optional<RestStatus> RestGraphHandler::edgeSetsAction(
    const std::shared_ptr<const Graph> graph) {

  switch (request()->requestType()) {
    case RequestType::GET:
      graphActionReadConfig(graph, TRI_COL_TYPE_EDGE, GraphProperty::EDGES);
      return RestStatus::DONE;
    case RequestType::POST:
      createEdgeDefinition(graph);
      return RestStatus::DONE;
    default:;
  }
  return boost::none;
}

boost::optional<RestStatus> RestGraphHandler::edgeSetAction(
    const std::shared_ptr<const Graph> graph,
    const std::string& edgeDefinitionName) {

  switch (request()->requestType()) {
    case RequestType::POST:
      edgeActionCreate(graph, edgeDefinitionName);
      return RestStatus::DONE;
    case RequestType::PUT:
      editEdgeDefinition(graph, edgeDefinitionName);
      return RestStatus::DONE;
    case RequestType::DELETE_REQ:
      removeEdgeDefinition(graph, edgeDefinitionName);
      return RestStatus::DONE;
    default:;
  }
  return boost::none;
}

boost::optional<RestStatus> RestGraphHandler::vertexSetAction(
    const std::shared_ptr<const Graph> graph,
    const std::string& vertexCollectionName) {

  switch (request()->requestType()) {
    case RequestType::POST:
      vertexActionCreate(graph, vertexCollectionName);
      return RestStatus::DONE;
    case RequestType::DELETE_REQ:
      modifyVertexDefinition(graph, VertexDefinitionAction::REMOVE, vertexCollectionName);
      return RestStatus::DONE;
    default:;
  }
  return boost::none;
}

boost::optional<RestStatus> RestGraphHandler::vertexAction(
    const std::shared_ptr<const Graph> graph,
    const std::string& vertexCollectionName, const std::string& vertexKey) {

  switch (request()->requestType()) {
    case RequestType::GET: {
      // TODO Maybe vertexActionRead can return void as it already handles
      // errors
      Result res = vertexActionRead(graph, vertexCollectionName, vertexKey);
      return RestStatus::DONE;
    }
    case RequestType::PATCH:
      vertexActionUpdate(graph, vertexCollectionName, vertexKey);
      return RestStatus::DONE;
    case RequestType::PUT:
      vertexActionReplace(graph, vertexCollectionName, vertexKey);
      return RestStatus::DONE;
    case RequestType::DELETE_REQ:
      vertexActionRemove(graph, vertexCollectionName, vertexKey);
      return RestStatus::DONE;
    default:;
  }
  return boost::none;
}

boost::optional<RestStatus> RestGraphHandler::edgeAction(
    const std::shared_ptr<const Graph> graph,
    const std::string& edgeDefinitionName, const std::string& edgeKey) {

  switch (request()->requestType()) {
    case RequestType::GET:
      // TODO Maybe edgeActionRead can return void as it already handles
      // errors
      edgeActionRead(graph, edgeDefinitionName, edgeKey);
      return RestStatus::DONE;
    case RequestType::DELETE_REQ:
      edgeActionRemove(graph, edgeDefinitionName, edgeKey);
      return RestStatus::DONE;
    case RequestType::PATCH:
      edgeActionUpdate(graph, edgeDefinitionName, edgeKey);
      return RestStatus::DONE;
    case RequestType::PUT:
      edgeActionReplace(graph, edgeDefinitionName, edgeKey);
      return RestStatus::DONE;
    default:;
  }
  return boost::none;
}

Result RestGraphHandler::vertexActionRead(
    const std::shared_ptr<const Graph> graph, const std::string& collectionName,
    const std::string& key) {

  bool isValidRevision;
  TRI_voc_rid_t revision = extractRevision("if-match", isValidRevision);
  if (!isValidRevision) {
    revision =
        UINT64_MAX;  // an impossible rev, so precondition failed will happen
  }
  auto maybeRev = boost::make_optional(revision != 0, revision);

  auto ctx = std::make_shared<transaction::StandaloneContext>(_vocbase);
  GraphOperations gops{*graph, ctx};
  auto resultT = gops.getVertex(collectionName, key, maybeRev);

  if (!resultT.ok()) {
    generateTransactionError(collectionName, resultT, "");
    return resultT.copy_result();
  }

  OperationResult& result = resultT.get().first;

  Result res = resultT.get().second;

  if (!result.ok()) {
    if (result.is(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
      generateDocumentNotFound(collectionName, key);
    } else if (maybeRev && result.is(TRI_ERROR_ARANGO_CONFLICT)) {
      generatePreconditionFailed(result.slice());
    } else {
      generateTransactionError(collectionName, res, key);
    }
    return result.result;
  }

  if (!res.ok()) {
    generateTransactionError(collectionName, res, key);
    return res;
  }

  // use default options
  generateVertexRead(result.slice(), *ctx->getVPackOptionsForDump());
  return Result();
}

/// @brief generate response object: { error, code, vertex }
void RestGraphHandler::generateVertexRead(VPackSlice vertex,
                                          VPackOptions const& options) {
  vertex = vertex.resolveExternal();
  resetResponse(rest::ResponseCode::OK);
  addEtagHeader(vertex.get(StaticStrings::RevString));
  generateResultWithField("vertex", vertex, options);
}

/// @brief generate response object: { error, code, edge }
void RestGraphHandler::generateEdgeRead(VPackSlice edge,
                                        VPackOptions const& options) {
  edge = edge.resolveExternal();
  resetResponse(rest::ResponseCode::OK);
  addEtagHeader(edge.get(StaticStrings::RevString));
  generateResultWithField("edge", edge, options);
}

/// @brief generate response object: { error, code, removed, old? }
/// "old" is omitted if old is a NoneSlice.
void RestGraphHandler::generateRemoved(bool removed, bool wasSynchronous,
                                       VPackSlice old,
                                       VPackOptions const& options) {
  ResponseCode code;
  if (wasSynchronous) {
    code = rest::ResponseCode::OK;
  } else {
    code = rest::ResponseCode::ACCEPTED;
  }
  resetResponse(code);
  VPackBuilder obj;
  obj.add(VPackValue(VPackValueType::Object, true));
  obj.add("removed", VelocyPackHelper::BooleanValue(removed));
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
  ResponseCode code;
  if (wasSynchronous) {
    code = rest::ResponseCode::ACCEPTED;
  } else {
    code = rest::ResponseCode::ACCEPTED;
    // code = rest::ResponseCode::CREATED; // TODO this is not as it is described in the documentation
  }
  resetResponse(code);
  VPackBuilder obj;
  obj.add(VPackValue(VPackValueType::Object, true));
  obj.add("removed", VelocyPackHelper::BooleanValue(removed));
  obj.close();
  generateResultMergedWithObject(obj.slice(), options);
}

void RestGraphHandler::generateGraphConfig(VPackSlice slice,
                                           VPackOptions const& options) {
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

// TODO DOCU not equals TESTS !!! (responseCode)
void RestGraphHandler::generateCreatedEdgeDefinition(bool wasSynchronous, VPackSlice slice,
                                           VPackOptions const& options) {
  ResponseCode code;
  if (wasSynchronous) {
    code = rest::ResponseCode::ACCEPTED;
  } else {
    code = rest::ResponseCode::ACCEPTED;
  }
  resetResponse(code);
  addEtagHeader(slice.get("graph").get(StaticStrings::RevString));
  generateResultMergedWithObject(slice, options);
}

/// @brief generate response object: { error, code, vertex, old?, new? }
void RestGraphHandler::generateVertexModified(
    bool wasSynchronous, VPackSlice resultSlice,
    const velocypack::Options& options) {
  generateModified(TRI_COL_TYPE_DOCUMENT, wasSynchronous, resultSlice, options);
}

/// @brief generate response object: { error, code, vertex }
void RestGraphHandler::generateVertexCreated(
    bool wasSynchronous, VPackSlice resultSlice,
    const velocypack::Options& options) {
  generateCreated(TRI_COL_TYPE_DOCUMENT, wasSynchronous, resultSlice, options);
}

/// @brief generate response object: { error, code, edge, old?, new? }
void RestGraphHandler::generateEdgeModified(
    bool wasSynchronous, VPackSlice resultSlice,
    const velocypack::Options& options) {
  generateModified(TRI_COL_TYPE_EDGE, wasSynchronous, resultSlice, options);
}

/// @brief generate response object: { error, code, edge }
void RestGraphHandler::generateEdgeCreated(
    bool wasSynchronous, VPackSlice resultSlice,
    const velocypack::Options& options) {
  generateCreated(TRI_COL_TYPE_EDGE, wasSynchronous, resultSlice, options);
}

/// @brief generate response object: { error, code, vertex/edge, old?, new? }
// TODO Maybe a class enum in Graph.h to discern Vertex/Edge is better than
// abusing document/edge collection types?
void RestGraphHandler::generateModified(TRI_col_type_e colType,
                                        bool wasSynchronous,
                                        VPackSlice resultSlice,
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
      VelocyPackHelper::copyObjectWithout(resultSlice, {"old", "new"});
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
                                        bool wasSynchronous,
                                        VPackSlice resultSlice,
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
      VelocyPackHelper::copyObjectWithout(resultSlice, {"old", "new"});
  // Note: This doesn't really contain the object, only _id, _key, _rev, _oldRev
  VPackSlice objectSlice = objectBuilder.slice();
  VPackSlice newSlice = resultSlice.get("new");

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
void RestGraphHandler::generateResultWithField(std::string const& key,
                                               VPackSlice value,
                                               VPackOptions const& options) {
  VPackBuilder obj;
  obj.add(VPackValue(VPackValueType::Object, true));
  obj.add(key, value);
  obj.close();
  generateResultMergedWithObject(obj.slice(), options);
}

/// @brief generate response object: MERGE({ error, code }, obj)
void RestGraphHandler::generateResultMergedWithObject(
    VPackSlice obj, VPackOptions const& options) {
  _response->setContentType(_request->contentTypeResponse());

  try {
    VPackBuilder result;
    result.add(VPackValue(VPackValueType::Object, true));
    result.add(StaticStrings::Error, VPackValue(false));
    result.add(StaticStrings::Code,
               VPackValue(static_cast<int>(_response->responseCode())));
    result.close();
    VPackBuilder merged =
        VelocyPackHelper::merge(result.slice(), obj, false, false);

    writeResult(std::move(*merged.buffer().get()), options);
  } catch (...) {
    // Building the error response failed
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  "cannot generate output");
  }
}

// TODO this is nearly exactly the same as vertexActionRead. reuse somehow?
Result RestGraphHandler::edgeActionRead(
    const std::shared_ptr<const graph::Graph> graph,
    const std::string& definitionName, const std::string& key) {

  bool isValidRevision;
  TRI_voc_rid_t revision = extractRevision("if-match", isValidRevision);
  if (!isValidRevision) {
    revision =
        UINT64_MAX;  // an impossible rev, so precondition failed will happen
  }
  auto maybeRev = boost::make_optional(revision != 0, revision);

  auto ctx = std::make_shared<transaction::StandaloneContext>(_vocbase);
  GraphOperations gops{*graph, ctx};
  auto resultT = gops.getEdge(definitionName, key, maybeRev);

  if (!resultT.ok()) {
    generateTransactionError(definitionName, resultT, "");
    return resultT.copy_result();
  }

  OperationResult& result = resultT.get().first;

  Result res = resultT.get().second;

  if (!result.ok()) {
    if (result.is(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
      generateDocumentNotFound(definitionName, key);
    } else if (maybeRev && result.is(TRI_ERROR_ARANGO_CONFLICT)) {
      generatePreconditionFailed(result.slice());
    } else {
      generateTransactionError(definitionName, res, key);
    }
    return result.result;
  }

  if (!res.ok()) {
    generateTransactionError(definitionName, res, key);
    return res;
  }

  // use default options
  generateEdgeRead(result.slice(), *ctx->getVPackOptionsForDump());
  return Result();
}

std::shared_ptr<Graph const> RestGraphHandler::getGraph(
    std::shared_ptr<transaction::Context> ctx, const std::string& graphName) {
  std::shared_ptr<Graph const> graph = _graphCache.getGraph(std::move(ctx), graphName);

  // TODO remove exception, handle return value instead
  if (graph == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_NOT_FOUND);
  }

  return graph;
}
// TODO this is very similar to (edge|vertex)ActionRead. find a way to reduce
// the duplicate code.
// TODO The tests check that, if "returnOld: true" is passed,  the result
// contains the old value in the field "old". This is not documented in
// HTTP/Gharial!
Result RestGraphHandler::edgeActionRemove(
    const std::shared_ptr<const Graph> graph, const std::string& definitionName,
    const std::string& key) {

  bool waitForSync =
      _request->parsedValue(StaticStrings::WaitForSyncString, false);

  bool returnOld = _request->parsedValue(StaticStrings::ReturnOldString, false);

  bool isValidRevision;
  TRI_voc_rid_t revision = extractRevision("if-match", isValidRevision);
  if (!isValidRevision) {
    revision =
        UINT64_MAX;  // an impossible rev, so precondition failed will happen
  }
  auto maybeRev = boost::make_optional(revision != 0, revision);

  auto ctx = std::make_shared<transaction::StandaloneContext>(_vocbase);
  GraphOperations gops{*graph, ctx};

  auto resultT =
      gops.removeEdge(definitionName, key, maybeRev, waitForSync, returnOld);

  if (!resultT.ok()) {
    generateTransactionError(definitionName, resultT, "");
    return resultT.copy_result();
  }

  OperationResult& result = resultT.get().first;

  Result res = resultT.get().second;

  if (result.fail()) {
    generateTransactionError(result);
    return result.result;
  }

  if (!res.ok()) {
    generateTransactionError(definitionName, res, key);
    return res;
  }

  generateRemoved(true, result._options.waitForSync, result.slice().get("old"),
                  *ctx->getVPackOptionsForDump());

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

Result RestGraphHandler::vertexActionUpdate(
    std::shared_ptr<const graph::Graph> graph,
    const std::string& collectionName, const std::string& key) {
  return vertexModify(std::move(graph), collectionName, key, true);
}

Result RestGraphHandler::vertexActionReplace(
    std::shared_ptr<const graph::Graph> graph,
    const std::string& collectionName, const std::string& key) {
  return vertexModify(std::move(graph), collectionName, key, false);
}

Result RestGraphHandler::vertexActionCreate(
    std::shared_ptr<const graph::Graph> graph,
    const std::string& collectionName) {
  return vertexCreate(std::move(graph), collectionName);
}

Result RestGraphHandler::edgeActionUpdate(
    std::shared_ptr<const graph::Graph> graph,
    const std::string& collectionName, const std::string& key) {
  return edgeModify(std::move(graph), collectionName, key, true);
}

Result RestGraphHandler::edgeActionReplace(
    std::shared_ptr<const graph::Graph> graph,
    const std::string& collectionName, const std::string& key) {
  return edgeModify(std::move(graph), collectionName, key, false);
}

Result RestGraphHandler::edgeModify(std::shared_ptr<const graph::Graph> graph,
                                    const std::string& collectionName,
                                    const std::string& key, bool isPatch) {
  return documentModify(std::move(graph), collectionName, key, isPatch,
                        TRI_COL_TYPE_EDGE);
}

Result RestGraphHandler::edgeCreate(std::shared_ptr<const graph::Graph> graph,
                                    const std::string& collectionName) {
  return documentCreate(std::move(graph), collectionName,
                        TRI_COL_TYPE_EDGE);
}

Result RestGraphHandler::edgeActionCreate(
    std::shared_ptr<const graph::Graph> graph,
    const std::string& collectionName) {
  return edgeCreate(std::move(graph), collectionName);
}

Result RestGraphHandler::vertexModify(std::shared_ptr<const graph::Graph> graph,
                                      const std::string& collectionName,
                                      const std::string& key, bool isPatch) {
  return documentModify(std::move(graph), collectionName, key, isPatch,
                        TRI_COL_TYPE_DOCUMENT);
}

Result RestGraphHandler::vertexCreate(std::shared_ptr<const graph::Graph> graph,
                                      const std::string& collectionName) {
  return documentCreate(std::move(graph), collectionName,
                        TRI_COL_TYPE_DOCUMENT);
}

// /_api/gharial/{graph-name}/edge/{definition-name}
Result RestGraphHandler::editEdgeDefinition(
    std::shared_ptr<const graph::Graph> graph,
    const std::string& edgeDefinitionName) {
  return modifyEdgeDefinition(std::move(graph), EdgeDefinitionAction::EDIT,
                              edgeDefinitionName);
}

Result RestGraphHandler::createEdgeDefinition(
    std::shared_ptr<const graph::Graph> graph) {
  return modifyEdgeDefinition(std::move(graph), EdgeDefinitionAction::CREATE);
}

// /_api/gharial/{graph-name}/edge
Result RestGraphHandler::modifyEdgeDefinition(std::shared_ptr<const graph::Graph> graph,
    EdgeDefinitionAction action, std::string edgeDefinitionName) {

  // edgeDefinitionName == "" <=> action == CREATE
  TRI_ASSERT((action == EdgeDefinitionAction::CREATE)
             == edgeDefinitionName.empty());
  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    return false;
  }
  bool waitForSync =
      _request->parsedValue(StaticStrings::WaitForSyncString, false);
  bool dropCollections =
          _request->parsedValue(Graph::_attrDropCollections, false);

  auto ctx = std::make_shared<transaction::StandaloneContext>(_vocbase);

  GraphOperations gops{*graph, ctx};
  ResultT<std::pair<OperationResult, Result>> resultT{
      Result(TRI_ERROR_INTERNAL)};
  
  if (action == EdgeDefinitionAction::CREATE) {
    resultT = gops.addEdgeDefinition(body, waitForSync);
  } else if (action == EdgeDefinitionAction::EDIT) {
    resultT = gops.editEdgeDefinition(body, waitForSync, edgeDefinitionName);
  } else if (action == EdgeDefinitionAction::REMOVE) {
    resultT = gops.eraseEdgeDefinition(
      waitForSync, edgeDefinitionName, dropCollections
    );
  } else {
    TRI_ASSERT(false);
  }

  OperationResult& result = resultT.get().first;
  Result res = resultT.get().second;

  if (result.fail()) {
    generateTransactionError(result);
    return result.result;
  }

  if (!res.ok()) {
    generateTransactionError("", res, ""); // TODO: how to do properly?
    return res;
  }

  // TODO invalidate graph in _graphCache

  ctx = std::make_shared<transaction::StandaloneContext>(_vocbase);

  graph = _graphCache.getGraph(ctx, graph->name());
  VPackBuilder builder;
  graph->graphToVpack(builder);

  generateCreatedEdgeDefinition(waitForSync, builder.slice(), *ctx->getVPackOptionsForDump());

  return Result();
}

Result RestGraphHandler::modifyVertexDefinition(std::shared_ptr<const graph::Graph> graph,
                                              VertexDefinitionAction action, std::string vertexDefinitionName) {
  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    return false;
  }

  // TODO maybe merge this function with modifyEdgeDefinition?
  bool waitForSync =
          _request->parsedValue(StaticStrings::WaitForSyncString, false);
  bool dropCollections =
          _request->parsedValue(Graph::_attrDropCollections, false);

  auto ctx = std::make_shared<transaction::StandaloneContext>(_vocbase);

  GraphOperations gops{*graph, ctx};
  ResultT<std::pair<OperationResult, Result>> resultT{
          Result(TRI_ERROR_INTERNAL)};

  if (action == VertexDefinitionAction::CREATE) {
    resultT = gops.addOrphanCollection(body, waitForSync);
  } else if (action == VertexDefinitionAction::REMOVE) {
    resultT = gops.eraseOrphanCollection(
      waitForSync, vertexDefinitionName, dropCollections
    );
  } else {
    TRI_ASSERT(false);
  }

  OperationResult& result = resultT.get().first;
  Result res = resultT.get().second;

  if (result.fail()) {
    generateTransactionError(result);
    return result.result;
  }

  if (!res.ok()) {
    generateTransactionError("", res, ""); // TODO: how to do properly?
    return res;
  }

  // TODO invalidate graph in _graphCache

  ctx = std::make_shared<transaction::StandaloneContext>(_vocbase);

  graph = _graphCache.getGraph(ctx, graph->name());
  VPackBuilder builder;
  graph->graphToVpack(builder);

  generateCreatedEdgeDefinition(waitForSync, builder.slice(), *ctx->getVPackOptionsForDump());

  return Result();
}

Result RestGraphHandler::removeEdgeDefinition(std::shared_ptr<const graph::Graph> graph,
                                               const std::string& edgeDefinitionName) {
  return modifyEdgeDefinition(graph, EdgeDefinitionAction::REMOVE, edgeDefinitionName);
}


// TODO The tests check that, if "returnOld: true" is passed,  the result
// contains the old value in the field "old"; and if "returnNew: true" is
// passed, the field "new" contains the new value (along with "vertex"!).
// This is not documented in HTTP/Gharial!
// TODO the document API also supports mergeObjects, silent and ignoreRevs;
// should gharial, too?
Result RestGraphHandler::documentModify(
    std::shared_ptr<const graph::Graph> graph,
    const std::string& collectionName, const std::string& key, bool isPatch,
    TRI_col_type_e colType) {

  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    return false;
  }

  bool waitForSync =
      _request->parsedValue(StaticStrings::WaitForSyncString, false);
  bool returnNew = _request->parsedValue(StaticStrings::ReturnNewString, false);
  bool returnOld = _request->parsedValue(StaticStrings::ReturnOldString, false);
  // Note: the default here differs from the one in the RestDoumentHandler
  bool keepNull = _request->parsedValue(StaticStrings::KeepNullString, true);

  // extract the revision, if single document variant and header given:
  std::unique_ptr<VPackBuilder> builder;
  TRI_voc_rid_t revision = 0;
  bool isValidRevision;
  revision = extractRevision("if-match", isValidRevision);
  if (!isValidRevision) {
    revision = UINT64_MAX;  // an impossible revision, so precondition failed
  }
  auto maybeRev = boost::make_optional(revision != 0, revision);

  auto ctx = std::make_shared<transaction::StandaloneContext>(_vocbase);
  GraphOperations gops{*graph, ctx};

  ResultT<std::pair<OperationResult, Result>> resultT{
      Result(TRI_ERROR_INTERNAL)};
  // TODO get rid of this branching, rather use several functions and reuse the
  // common code another way.
  if (isPatch && colType == TRI_COL_TYPE_DOCUMENT) {
    resultT = gops.updateVertex(collectionName, key, body, maybeRev,
                                waitForSync, returnOld, returnNew, keepNull);
  } else if (!isPatch && colType == TRI_COL_TYPE_DOCUMENT) {
    resultT = gops.replaceVertex(collectionName, key, body, maybeRev,
                                 waitForSync, returnOld, returnNew, keepNull);
  } else if (isPatch && colType == TRI_COL_TYPE_EDGE) {
    resultT = gops.updateEdge(collectionName, key, body, maybeRev, waitForSync,
                              returnOld, returnNew, keepNull);
  } else if (!isPatch && colType == TRI_COL_TYPE_EDGE) {
    resultT = gops.replaceEdge(collectionName, key, body, maybeRev, waitForSync,
                               returnOld, returnNew, keepNull);
  } else {
    TRI_ASSERT(false);
  }

  if (!resultT.ok()) {
    generateTransactionError(collectionName, resultT, "");
    return resultT.copy_result();
  }

  OperationResult& result = resultT.get().first;

  Result res = resultT.get().second;

  if (result.fail()) {
    generateTransactionError(result);
    return false;
  }

  if (!res.ok()) {
    generateTransactionError(collectionName, res, key, 0);
    return false;
  }

  switch (colType) {
    case TRI_COL_TYPE_DOCUMENT:
      generateVertexModified(result._options.waitForSync, result.slice(),
                             *ctx->getVPackOptionsForDump());
      break;
    case TRI_COL_TYPE_EDGE:
      generateEdgeModified(result._options.waitForSync, result.slice(),
                           *ctx->getVPackOptionsForDump());
      break;
    default:
      TRI_ASSERT(false);
  }

  return true;
}

Result RestGraphHandler::documentCreate(
    std::shared_ptr<const graph::Graph> graph,
    const std::string& collectionName,
    TRI_col_type_e colType) {

  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    return false;
  }

  bool waitForSync =
    _request->parsedValue(StaticStrings::WaitForSyncString, false);
  bool returnNew = _request->parsedValue(StaticStrings::ReturnNewString, false);

  auto ctx = std::make_shared<transaction::StandaloneContext>(_vocbase);
  GraphOperations gops{*graph, ctx};

  ResultT<std::pair<OperationResult, Result>> resultT{
      Result(TRI_ERROR_INTERNAL)};
  if (colType == TRI_COL_TYPE_DOCUMENT) {
    resultT = gops.createVertex(collectionName, body, waitForSync, returnNew);
  } else if (colType == TRI_COL_TYPE_EDGE) {
    resultT = gops.createEdge(collectionName, body, waitForSync, returnNew);
  } else {
    TRI_ASSERT(false);
  }

  if (!resultT.ok()) {
    generateTransactionError(collectionName, resultT, "");
    return resultT.copy_result();
  }

  OperationResult& result = resultT.get().first;

  Result res = resultT.get().second;

  if (result.fail()) {
    generateTransactionError(result);
    return false;
  }

  if (!res.ok()) {
    generateTransactionError(collectionName, res, nullptr, 0);
    return false;
  }

  switch (colType) {
    case TRI_COL_TYPE_DOCUMENT:
      generateVertexCreated(result._options.waitForSync, result.slice(),
                             *ctx->getVPackOptionsForDump());
      break;
    case TRI_COL_TYPE_EDGE:
      generateEdgeCreated(result._options.waitForSync, result.slice(),
                           *ctx->getVPackOptionsForDump());
      break;
    default:
      TRI_ASSERT(false);
  }

  return true;
}

Result RestGraphHandler::vertexActionRemove(
    const std::shared_ptr<const graph::Graph> graph,
    const std::string& collectionName, const std::string& key) {

  bool waitForSync =
      _request->parsedValue(StaticStrings::WaitForSyncString, false);

  bool returnOld = _request->parsedValue(StaticStrings::ReturnOldString, false);

  bool isValidRevision;
  TRI_voc_rid_t revision = extractRevision("if-match", isValidRevision);
  if (!isValidRevision) {
    revision =
        UINT64_MAX;  // an impossible rev, so precondition failed will happen
  }
  auto maybeRev = boost::make_optional(revision != 0, revision);

  auto ctx = std::make_shared<transaction::StandaloneContext>(_vocbase);
  GraphOperations gops{*graph, ctx};

  auto resultT =
      gops.removeVertex(collectionName, key, maybeRev, waitForSync, returnOld);

  if (!resultT.ok()) {
    generateTransactionError(collectionName, resultT, "");
    return resultT.copy_result();
  }

  OperationResult& result = resultT.get().first;

  Result res = resultT.get().second;

  if (result.fail()) {
    generateTransactionError(result);
    return result.result;
  }

  if (!res.ok()) {
    generateTransactionError(collectionName, res, key);
    return res;
  }

  generateRemoved(true, result._options.waitForSync, result.slice().get("old"),
                  *ctx->getVPackOptionsForDump());

  return Result();
}

Result RestGraphHandler::graphActionReadGraphConfig(
    const std::shared_ptr<const graph::Graph> graph) {

  auto ctx = std::make_shared<transaction::StandaloneContext>(_vocbase);
  VPackBuilder builder;
  graph->graphToVpack(builder);
  generateGraphConfig(builder.slice(), *ctx->getVPackOptionsForDump());

  return Result();
}

Result RestGraphHandler::graphActionRemoveGraph(
    const std::shared_ptr<const graph::Graph> graph) {
  bool waitForSync =
      _request->parsedValue(StaticStrings::WaitForSyncString, false);
  bool dropCollections =
      _request->parsedValue(Graph::_attrDropCollections, false);

  auto ctx = std::make_shared<transaction::StandaloneContext>(_vocbase);
  GraphOperations gops{*graph, ctx};
  auto resultT = gops.removeGraph(waitForSync, dropCollections);

  if (!resultT.ok()) {
    generateTransactionError("", resultT, "");
    return resultT.copy_result();
  }

  OperationResult& result = resultT.get().first;

  Result res = resultT.get().second;

  if (result.fail()) {
    generateTransactionError(result);
    return result.result;
  }

  if (!res.ok()) {
    generateTransactionError("", res, "");
    return res;
  }

  generateGraphRemoved(true, result._options.waitForSync,
                  *ctx->getVPackOptionsForDump());

  return Result();
}

Result RestGraphHandler::graphActionCreateGraph() {

  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    return false;
  }
  bool waitForSync =
      _request->parsedValue(StaticStrings::WaitForSyncString, false);

  {
    auto ctx = std::make_shared<transaction::StandaloneContext>(_vocbase);
    GraphManager gmngr{ctx};
    auto result = gmngr.createGraph(body, waitForSync); // TODO CHANGE return type
    // TODO check result value!
  }
 
  std::string graphName = body.get(StaticStrings::DataSourceName).copyString();

  auto ctx = std::make_shared<transaction::StandaloneContext>(_vocbase);
  std::shared_ptr<Graph const> graph = getGraph(ctx, graphName);

  VPackBuilder builder;
  graph->graphToVpack(builder);

  generateCreatedGraphConfig(waitForSync, builder.slice(), *ctx->getVPackOptionsForDump());

  return Result();
}

Result RestGraphHandler::graphActionReadGraphs() {
  auto ctx = std::make_shared<transaction::StandaloneContext>(_vocbase);

  GraphManager gmngr{ctx};
  VPackBuilder builder;
  gmngr.readGraphs(builder, arangodb::aql::PART_MAIN);

  generateGraphConfig(builder.slice(), *ctx->getVPackOptionsForDump());

  return Result();
}

Result RestGraphHandler::graphActionReadConfig(
    const std::shared_ptr<const graph::Graph> graph, TRI_col_type_e colType,
    GraphProperty property) {
  VPackBuilder builder;

  if (colType == TRI_COL_TYPE_DOCUMENT && property == GraphProperty::VERTICES) {
    graph->verticesToVpack(builder);
  } else if (colType == TRI_COL_TYPE_EDGE && property == GraphProperty::EDGES) {
    graph->edgesToVpack(builder);
  } else {
    TRI_ASSERT(false);
  }

  auto ctx = std::make_shared<transaction::StandaloneContext>(_vocbase);

  generateGraphConfig(builder.slice(), *ctx->getVPackOptionsForDump());

  return Result();
}
