////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Koichi Nakata
////////////////////////////////////////////////////////////////////////////////

#include "RestHandler/RestVocbaseBaseHandler.h"
#include "RestHandler/RestSchemaHandler.h"
#include "Aql/QueryRegistry.h"
#include "Aql/Query.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/OperationOrigin.h"
#include "Graph/GraphManager.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchLinkHelper.h"

#include <velocypack/Builder.h>
#include <velocypack/vpack.h>
#include <Basics/voc-errors.h>
#include <IResearch/IResearchLink.h>
#include <Utils/CollectionNameResolver.h>
#include <VocBase/LogicalCollection.h>
#include <VocBase/Methods/Collections.h>

using namespace arangodb;
using namespace arangodb::rest;

namespace {
constexpr std::string_view moduleName("graph management");
}

const std::string RestSchemaHandler::queryString = R"(
    LET docs = (
      FOR d IN @@collection
        SORT RAND()
        LIMIT @sampleNum
        RETURN d
    )

    LET total = LENGTH(docs)

    FOR d IN docs
      LET keys = ATTRIBUTES(d)
      FOR key IN keys
        FILTER key != "_rev"
        COLLECT attribute = key
        AGGREGATE
          count = COUNT(d),
          types = UNIQUE(TYPENAME(d[key]))
        RETURN {
          attribute,
          types,
          optional: count < total
        }
    )";

RestSchemaHandler::RestSchemaHandler(ArangodServer& server,
                                     GeneralRequest* request,
                                     GeneralResponse* response,
                                     aql::QueryRegistry* queryRegistry)
    : RestCursorHandler(server, request, response, queryRegistry),
      _graphManager(_vocbase, transaction::OperationOriginREST{::moduleName}) {}

RestStatus RestSchemaHandler::execute() {
  if (_request->requestType() != RequestType::GET) {
    generateError(ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "Schema endpoint only accepts GET request");
    return RestStatus::DONE;
  }

  auto maybeSampleNum = validateParameter("sampleNum", defaultSampleNum);
  auto maybeExampleNum =
      validateParameter("exampleNum", defaultExampleNum, true);

  if (!maybeSampleNum || !maybeExampleNum) {
    return RestStatus::DONE;
  }

  uint64_t sampleNum = *maybeSampleNum;
  uint64_t exampleNum = *maybeExampleNum;
  if (sampleNum < exampleNum) {
    generateError(
        ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "Parameter exampleNum must be equal to or smaller than sampleNum");
    return RestStatus::DONE;
  }

  auto const& suffix = _request->suffixes();
  switch (suffix.size()) {
    case 0:
      return lookupSchema(sampleNum, exampleNum);
    case 2:
      if (suffix[0] == "collection") {
        return lookupSchemaCollection(suffix[1], sampleNum, exampleNum);
      }
      if (suffix[0] == "graph") {
        return lookupSchemaGraph(suffix[1], sampleNum, exampleNum);
      }
      if (suffix[0] == "view") {
        return lookupSchemaView(suffix[1], sampleNum, exampleNum);
      }
      [[fallthrough]];  // If suffix[0] is none of "collection", "graph" or
                        // "view", go to default
    default:
      generateError(ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                    "Illegal suffixes provided: must be /schema, "
                    "/schema/collection/<collectionName>, "
                    "/schema/graph/<graphName>, or /schema/<viewName>");
      return RestStatus::DONE;
  }
}

RestStatus RestSchemaHandler::handleQueryResult() {
  if (_queryResult.result.fail()) {
    generateError(_queryResult.result);
    return RestStatus::DONE;
  }
  auto resultSlice = _queryResult.data->slice();
  generateResult(ResponseCode::OK, resultSlice);
  return RestStatus::DONE;
}

RestStatus RestSchemaHandler::lookupSchema(uint64_t sampleNum,
                                           uint64_t exampleNum) {
  auto resultBuilder = std::make_shared<velocypack::Builder>();
  std::set<std::string> colSet;
  resultBuilder->openObject();

  Result graphsRes = getAllGraphsAndCollections(*resultBuilder, colSet);
  if (graphsRes.fail()) {
    return RestStatus::DONE;
  }

  Result viewsRes = getAllViewsAndCollections(*resultBuilder, colSet);
  if (viewsRes.fail()) {
    return RestStatus::DONE;
  }

  Result colsRes =
      getCollections(colSet, sampleNum, exampleNum, *resultBuilder);
  if (colsRes.fail()) {
    return RestStatus::DONE;
  }
  resultBuilder->close();

  _queryResult.data = resultBuilder;
  return handleQueryResult();
}

RestStatus RestSchemaHandler::lookupSchemaCollection(std::string const& colName,
                                                     uint64_t sampleNum,
                                                     uint64_t exampleNum) {
  auto found = _vocbase.lookupCollection(colName);
  if (found == nullptr) {
    generateError(ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                  std::format("Collection {} not found", colName));
    return RestStatus::DONE;
  }

  auto resultBuilder = std::make_shared<velocypack::Builder>();
  resultBuilder->openObject();
  Result colRes = getCollection(colName, sampleNum, exampleNum, *resultBuilder);
  if (colRes.fail()) {
    return RestStatus::DONE;
  }
  resultBuilder->close();
  _queryResult.data = resultBuilder;
  return handleQueryResult();
}

RestStatus RestSchemaHandler::lookupSchemaGraph(std::string const& graphName,
                                                uint64_t sampleNum,
                                                uint64_t exampleNum) {
  auto resultBuilder = std::make_shared<velocypack::Builder>();
  std::set<std::string> colSet;

  resultBuilder->openObject();
  Result graphRes = getGraphAndCollections(graphName, *resultBuilder, colSet);
  if (graphRes.fail()) {
    return RestStatus::DONE;
  }

  getCollections(colSet, sampleNum, exampleNum, *resultBuilder);
  resultBuilder->close();

  _queryResult.data = resultBuilder;
  return handleQueryResult();
}

RestStatus RestSchemaHandler::lookupSchemaView(std::string const& viewName,
                                               uint64_t sampleNum,
                                               uint64_t exampleNum) {
  velocypack::Builder viewsArrBuilder;
  viewsArrBuilder.openArray();
  std::set<std::string> colSet;
  Result viewRes = getViewAndCollections(viewName, viewsArrBuilder, colSet);
  if (viewRes.fail()) {
    return RestStatus::DONE;
  }
  viewsArrBuilder.close();

  auto resultBuilder = std::make_shared<velocypack::Builder>();
  resultBuilder->openObject();
  resultBuilder->add("views", viewsArrBuilder.slice());

  getCollections(colSet, sampleNum, exampleNum, *resultBuilder);
  resultBuilder->close();

  _queryResult.data = resultBuilder;
  return handleQueryResult();
}

Result RestSchemaHandler::getCollections(std::set<std::string> const& colSet,
                                         uint64_t sampleNum,
                                         uint64_t exampleNum,
                                         velocypack::Builder& colsBuilder) {
  auto colsArrayBuilder = std::make_shared<velocypack::Builder>();
  colsArrayBuilder->openArray();
  for (auto const& colName : colSet) {
    if (colName.starts_with("_")) {
      continue;
    }
    velocypack::Builder colBuilder;
    colBuilder.openObject();
    Result colRes = getCollection(colName, sampleNum, exampleNum, colBuilder);
    if (colRes.fail()) {
      return colRes;
    }
    colBuilder.close();
    colsArrayBuilder->add(colBuilder.slice());
  }
  colsArrayBuilder->close();
  colsBuilder.add("collections", colsArrayBuilder->slice());
  return Result{};
}

Result RestSchemaHandler::getCollection(std::string const& colName,
                                        uint64_t sampleNum, uint64_t exampleNum,
                                        velocypack::Builder& colBuilder) {
  auto colPtr = _vocbase.lookupCollection(colName);
  if (!colPtr) {
    Result res{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
               std::format("Collection {} not found", colName)};
    generateError(ResponseCode::NOT_FOUND, res.errorNumber(),
                  res.errorMessage());
    return res;
  }

  auto bindVars = std::make_shared<velocypack::Builder>();
  bindVars->openObject();
  bindVars->add("@collection", VPackValue(colName));
  bindVars->add("sampleNum", VPackValue(sampleNum));
  bindVars->close();

  auto query = aql::Query::create(
      std::make_shared<transaction::StandaloneContext>(
          _vocbase, transaction::OperationOriginTestCase{}),
      aql::QueryString{queryString}, bindVars,
      aql::QueryOptions{velocypack::Parser::fromJson("{}")->slice()});

  aql::QueryResult qr;
  try {
    while (query->execute(qr) == aql::ExecutionState::WAITING) {
    }
  } catch (basics::Exception const& e) {
    Result res{e.code(),
               std::format("Schema query for {} threw: {}", colName, e.what())};
    generateError(ResponseCode::SERVER_ERROR, res.errorNumber(),
                  res.errorMessage());
    return res;
  }

  if (qr.result.fail()) {
    Result res{qr.result.errorNumber(),
               std::format("Schema query failed for {}: {}", colName,
                           qr.result.errorMessage())};
    generateError(ResponseCode::SERVER_ERROR, res.errorNumber(),
                  res.errorMessage());
    return res;
  }
  auto schemaSlice = qr.data->slice();
  if (!schemaSlice.isArray()) {
    Result res{TRI_ERROR_INTERNAL,
               std::format("Unexpected schema result for {}", colName)};
    generateError(ResponseCode::SERVER_ERROR, res.errorNumber(),
                  res.errorMessage());
    return res;
  }

  colBuilder.add("collectionName", velocypack::Value(colName));
  bool isDocument = (colPtr->type() == TRI_COL_TYPE_DOCUMENT);
  colBuilder.add("collectionType", isDocument ? velocypack::Value("document")
                                              : velocypack::Value("edge"));
  Result numRes = getNumOfDocumentsOrEdges(colName, colBuilder, isDocument);
  if (numRes.fail()) {
    return numRes;
  }

  colBuilder.add("schema", qr.data->slice());
  Result exRes = getExamples(colName, exampleNum, colBuilder);
  if (exRes.fail()) {
    return exRes;
  }
  return Result{};
}

Result RestSchemaHandler::getGraphAndCollections(
    std::string const& graphName, velocypack::Builder& graphBuilder,
    std::set<std::string>& colSet) {
  const std::string graphQueryString = R"(
    FOR g IN _graphs
      FILTER g._key == @graphName
      RETURN {
        name: g._key,
        relations: g.edgeDefinitions
      }
  )";

  auto bindVars = std::make_shared<velocypack::Builder>();
  bindVars->openObject();
  bindVars->add("graphName", VPackValue(graphName));
  bindVars->close();

  auto query = aql::Query::create(
      std::make_shared<transaction::StandaloneContext>(
          _vocbase, transaction::OperationOriginTestCase{}),
      aql::QueryString{graphQueryString}, bindVars,
      aql::QueryOptions{velocypack::Parser::fromJson("{}")->slice()});

  aql::QueryResult qr;
  try {
    while (query->execute(qr) == aql::ExecutionState::WAITING) {
    }
  } catch (basics::Exception const& e) {
    Result res{e.code(), std::format("Graph query for '{}' threw: {}",
                                     graphName, e.what())};
    generateError(ResponseCode::SERVER_ERROR, res.errorNumber(),
                  res.errorMessage());
    return res;
  }

  if (qr.result.fail()) {
    Result res{qr.result.errorNumber(),
               std::format("Graph query failed for '{}': {}", graphName,
                           qr.result.errorMessage())};
    generateError(ResponseCode::SERVER_ERROR, res.errorNumber(),
                  res.errorMessage());
    return res;
  }

  auto slice = qr.data->slice();
  if (!slice.isArray()) {
    Result res{
        TRI_ERROR_INTERNAL,
        std::format("Unexpected graph result shape for '{}'", graphName)};
    generateError(ResponseCode::SERVER_ERROR, res.errorNumber(),
                  res.errorMessage());
    return res;
  }
  if (slice.length() == 0) {
    Result res{TRI_ERROR_GRAPH_NOT_FOUND,
               std::format("Graph not found: '{}'", graphName)};
    generateError(ResponseCode::NOT_FOUND, res.errorNumber(),
                  res.errorMessage());
    return res;
  }
  graphBuilder.add("graphs", slice);

  Result connRes = getConnectedCollections(graphName, colSet);
  if (connRes.fail()) {
    return connRes;
  }
  return Result{};
}

Result RestSchemaHandler::getAllGraphsAndCollections(
    velocypack::Builder& graphBuilder, std::set<std::string>& colSet) {
  const std::string graphQueryString = R"(
    FOR g IN _graphs
    RETURN {
      name: g._key,
      relations: g.edgeDefinitions
    }
  )";

  auto query = aql::Query::create(
      std::make_shared<transaction::StandaloneContext>(
          _vocbase, transaction::OperationOriginTestCase{}),
      aql::QueryString{graphQueryString}, nullptr,
      aql::QueryOptions{velocypack::Parser::fromJson("{}")->slice()});

  aql::QueryResult qr;
  try {
    while (query->execute(qr) == aql::ExecutionState::WAITING) {
    }
  } catch (basics::Exception const& e) {
    Result res{e.code(),
               std::format("Graph query threw exception: {}", e.what())};
    generateError(ResponseCode::SERVER_ERROR, res.errorNumber(),
                  res.errorMessage());
    return res;
  }

  if (qr.result.fail()) {
    Result res{qr.result.errorNumber(),
               std::format("Graph query failed: {}", qr.result.errorMessage())};
    generateError(ResponseCode::SERVER_ERROR, res.errorNumber(),
                  res.errorMessage());
    return res;
  }

  auto slice = qr.data->slice();
  if (!slice.isArray()) {
    Result res{
        TRI_ERROR_INTERNAL,
        std::string("Unexpected result shape for GRAPH query: not an array")};
    generateError(ResponseCode::SERVER_ERROR, res.errorNumber(),
                  res.errorMessage());
    return res;
  }
  graphBuilder.add("graphs", slice);

  for (auto entry : velocypack::ArrayIterator(slice)) {
    auto nameSlice = entry.get("name");
    if (!nameSlice.isString()) {
      Result res{
          TRI_ERROR_INTERNAL,
          std::format("Graph entry missing or invalid name attribute: {}",
                      entry.toJson())};
      generateError(ResponseCode::SERVER_ERROR, res.errorNumber(),
                    res.errorMessage());
      return res;
    }
    auto graphName = nameSlice.copyString();
    Result connRes = getConnectedCollections(graphName, colSet);
    if (connRes.fail()) {
      return connRes;
    }
  }
  return Result{};
}

Result RestSchemaHandler::getViewAndCollections(
    std::string const& viewName, velocypack::Builder& viewsArrBuilder,
    std::set<std::string>& colSet) {
  auto view = CollectionNameResolver(_vocbase).getView(viewName);
  if (!view) {
    Result res{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
               std::format("View {} not found", viewName)};
    generateError(ResponseCode::NOT_FOUND, res.errorNumber(),
                  res.errorMessage());
    return res;
  }

  velocypack::Builder dataBuilder;
  dataBuilder.openObject();
  auto propRes = view->properties(dataBuilder,
                                  LogicalDataSource::Serialization::Properties);
  if (!propRes.ok()) {
    generateError(ResponseCode::SERVER_ERROR, propRes.errorNumber(),
                  propRes.errorMessage());
    return propRes;
  }
  dataBuilder.close();

  auto data = dataBuilder.slice();
  if (!data.hasKey("links")) {
    Result res{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
               std::format("View {} has no links", viewName)};
    generateError(ResponseCode::NOT_FOUND, res.errorNumber(),
                  res.errorMessage());
    return res;
  }

  velocypack::Builder linksBuilder;
  linksBuilder.openArray();
  for (auto li : velocypack::ObjectIterator(data.get("links"))) {
    auto colName = li.key.copyString();
    auto colValue = li.value;
    if (!colValue.hasKey("fields")) {
      Result res{
          TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
          std::format("View {}: link {} missing fields", viewName, colName)};
      generateError(ResponseCode::NOT_FOUND, res.errorNumber(),
                    res.errorMessage());
      return res;
    }
    velocypack::Builder fieldsBuilder;
    fieldsBuilder.openArray();
    for (auto fi : velocypack::ObjectIterator(colValue.get("fields"))) {
      velocypack::Builder fieldBuilder;
      fieldBuilder.openObject();
      fieldBuilder.add("attribute", velocypack::Value(fi.key.copyString()));
      if (!fi.value.hasKey("analyzers")) {
        Result res{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                   std::format("Link {} missing analyzers", viewName, colName)};
        generateError(ResponseCode::NOT_FOUND, res.errorNumber(),
                      res.errorMessage());
        return res;
      }
      fieldBuilder.add("analyzers", fi.value.get("analyzers"));
      fieldBuilder.close();
      fieldsBuilder.add(fieldBuilder.slice());
    }
    fieldsBuilder.close();

    velocypack::Builder colBuilder;
    colBuilder.openObject();
    colBuilder.add("collectionName", velocypack::Value(colName));
    colSet.insert(colName);
    colBuilder.add("fields", fieldsBuilder.slice());
    colBuilder.close();
    linksBuilder.add(colBuilder.slice());
  }
  linksBuilder.close();

  velocypack::Builder viewBuilder;
  viewBuilder.openObject();
  viewBuilder.add("viewName", velocypack::Value(viewName));
  viewBuilder.add("links", linksBuilder.slice());
  viewBuilder.close();

  viewsArrBuilder.add(viewBuilder.slice());
  return Result{};
}

Result RestSchemaHandler::getAllViewsAndCollections(
    velocypack::Builder& viewsBuilder, std::set<std::string>& colSet) {
  std::vector<LogicalView::ptr> views;
  LogicalView::enumerate(_vocbase,
                         [&views](LogicalView::ptr const& view) -> bool {
                           views.emplace_back(view);
                           return true;
                         });

  velocypack::Builder viewsArrBuilder;
  viewsArrBuilder.openArray();
  for (auto view : views) {
    TRI_ASSERT(view != nullptr);
    if (!view) {
      continue;
    }
    Result viewRes =
        getViewAndCollections(view->name(), viewsArrBuilder, colSet);
    if (viewRes.fail()) {
      return viewRes;
    }
  }
  viewsArrBuilder.close();
  viewsBuilder.add("views", viewsArrBuilder.slice());
  return Result{};
}

Result RestSchemaHandler::getNumOfDocumentsOrEdges(std::string const& colName,
                                                   velocypack::Builder& builder,
                                                   bool isDocument) {
  const std::string queryStr{"RETURN LENGTH(@@collection)"};

  auto bindVars = std::make_shared<velocypack::Builder>();
  bindVars->openObject();
  bindVars->add("@collection", velocypack::Value(colName));
  bindVars->close();

  auto query = aql::Query::create(
      std::make_shared<transaction::StandaloneContext>(
          _vocbase, transaction::OperationOriginTestCase{}),
      aql::QueryString{queryStr}, bindVars,
      aql::QueryOptions{velocypack::Parser::fromJson("{}")->slice()});

  aql::QueryResult qr;
  try {
    while (query->execute(qr) == aql::ExecutionState::WAITING) {
    }
  } catch (basics::Exception const& e) {
    Result res{e.code(),
               std::format("Count query for {} threw: {}", colName, e.what())};
    generateError(ResponseCode::SERVER_ERROR, res.errorNumber(),
                  res.errorMessage());
    return res;
  }

  if (qr.result.fail()) {
    Result res{qr.result.errorNumber(),
               std::format("Count query for {} failed: {}", colName,
                           qr.result.errorMessage())};
    generateError(ResponseCode::SERVER_ERROR, res.errorNumber(),
                  res.errorMessage());
    return res;
  }

  auto result = qr.data->slice();
  if (result.isNone() || !result.isArray()) {
    Result res{
        TRI_ERROR_INTERNAL,
        std::format("Count query did not return an array for {}", colName)};
    generateError(ResponseCode::SERVER_ERROR, res.errorNumber(),
                  res.errorMessage());
    return res;
  }

  if (result.length() == 0 || result.length() > 1) {
    Result res{
        TRI_ERROR_INTERNAL,
        std::format("Count query returned invalid array size for {}", colName)};
    generateError(ResponseCode::SERVER_ERROR, res.errorNumber(),
                  res.errorMessage());
    return res;
  }

  auto countSlice = result.at(0);
  if (!countSlice.isNumber()) {
    Result res{
        TRI_ERROR_INTERNAL,
        std::format("Count query returned non-numeric result for {}", colName)};
    generateError(ResponseCode::SERVER_ERROR, res.errorNumber(),
                  res.errorMessage());
    return res;
  }

  builder.add(isDocument ? "numOfDocuments" : "numOfEdges", countSlice);
  return Result{};
}

Result RestSchemaHandler::getExamples(std::string const& colName,
                                      uint64_t exampleNum,
                                      velocypack::Builder& builder) {
  const std::string queryStr = R"(
    FOR d IN @@collection LIMIT @exampleNum RETURN UNSET(d, "_rev")
  )";

  auto bindVars = std::make_shared<velocypack::Builder>();
  bindVars->openObject();
  bindVars->add("@collection", velocypack::Value(colName));
  bindVars->add("exampleNum", velocypack::Value(exampleNum));
  bindVars->close();

  auto query = aql::Query::create(
      std::make_shared<transaction::StandaloneContext>(
          _vocbase, transaction::OperationOriginTestCase{}),
      aql::QueryString{queryStr}, bindVars,
      aql::QueryOptions{velocypack::Parser::fromJson("{}")->slice()});

  aql::QueryResult qr;
  try {
    while (query->execute(qr) == aql::ExecutionState::WAITING) {
    }
  } catch (basics::Exception const& e) {
    Result res{e.code(), std::format("Example query for {} threw: {}", colName,
                                     e.what())};
    generateError(ResponseCode::SERVER_ERROR, res.errorNumber(),
                  res.errorMessage());
    return res;
  }

  if (qr.result.fail()) {
    Result res{qr.result.errorNumber(),
               std::format("Example query failed for {}: {}", colName,
                           qr.result.errorMessage())};
    generateError(ResponseCode::SERVER_ERROR, res.errorNumber(),
                  res.errorMessage());
    return res;
  }

  auto result = qr.data->slice();
  if (result.isNone() || !result.isArray()) {
    Result res{
        TRI_ERROR_INTERNAL,
        std::format("Example query did not return an array for {}", colName)};
    generateError(ResponseCode::SERVER_ERROR, res.errorNumber(),
                  res.errorMessage());
    return res;
  }

  builder.add("examples", result);
  return Result{};
}

std::optional<uint64_t> RestSchemaHandler::validateParameter(
    const std::string& param, uint64_t defaultValue, bool allowZero) {
  bool passed = false;

  auto const& val = _request->value(param, passed);
  if (!passed) {
    return defaultValue;
  }

  if (!std::all_of(val.begin(), val.end(),
                   [](char c) { return std::isdigit(c); })) {
    generateError(
        ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        std::format("Invalid value for {}: must contain only digits", param));
    return std::nullopt;
  }

  unsigned long long userInput = 0;
  try {
    userInput = std::stoull(val);
  } catch (std::out_of_range&) {
    generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  std::format("Value for {} is too large", param));
    return std::nullopt;
  } catch (...) {
    generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  std::format("Unexpected error parsing {}", param));
    return std::nullopt;
  }
  if (userInput == 0 && !allowZero) {
    generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  std::format("{} must be greater than 0", param));
    return std::nullopt;
  }
  return userInput;
}

Result RestSchemaHandler::getConnectedCollections(
    std::string const& graphName, std::set<std::string>& colSet) {
  auto resultGraph = _graphManager.lookupGraphByName(graphName);

  if (resultGraph.fail()) {
    Result res;
    if (resultGraph.errorNumber() == TRI_ERROR_GRAPH_NOT_FOUND) {
      res = Result{resultGraph.errorNumber(),
                   std::format("Graph {} not found", graphName)};
      generateError(ResponseCode::NOT_FOUND, res.errorNumber(),
                    res.errorMessage());
    } else {
      res = Result{resultGraph.errorNumber(),
                   std::format("Error looking up graph {}: ", graphName,
                               resultGraph.errorMessage())};
      generateError(ResponseCode::SERVER_ERROR, res.errorNumber(),
                    res.errorMessage());
    }
    return res;
  }

  auto& graphPtr = resultGraph.get();
  if (!graphPtr) {
    Result res{
        TRI_ERROR_INTERNAL,
        std::format("Graph lookup returned null pointer for {}", graphName)};
    generateError(ResponseCode::SERVER_ERROR, res.errorNumber(),
                  res.errorMessage());
    return res;
  }

  for (auto const& vertex : graphPtr->vertexCollections()) {
    colSet.insert(vertex);
  }
  for (auto const& edge : graphPtr->edgeCollections()) {
    colSet.insert(edge);
  }
  return Result{};
}