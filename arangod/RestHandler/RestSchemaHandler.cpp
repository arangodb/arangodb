////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2025-2025 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "Basics/voc-errors.h"
#include "Futures/Utilities.h"
#include "Graph/GraphManager.h"
#include "IResearch/IResearchLink.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchView.h"
#include "Logger/LogMacros.h"
#include "RestHandler/RestSchemaHandler.h"
#include "RestHandler/RestVocbaseBaseHandler.h"
#include "Transaction/Methods.h"
#include "Transaction/OperationOrigin.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Indexes.h"

#include <velocypack/Builder.h>
#include <velocypack/vpack.h>

using namespace arangodb;
using namespace arangodb::rest;

namespace {
constexpr std::string_view moduleName("graph management");
}

const std::string RestSchemaHandler::queryStr = R"(
    LET samples = (
      FOR d IN @@collection
        SORT RAND()
        LIMIT @sampleNum
        RETURN UNSET(d, "_rev")
    )
    LET total = LENGTH(samples)

    LET schemas = (
      FOR d IN samples
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
    )
    RETURN {
      num: LENGTH(@@collection),
      schemas: schemas,
      examples: SLICE(samples, 0, @exampleNum)
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
  ExecContext const& exec = ExecContext::current();
  // Permission:
  // - /schema, /graph/*, /view/* -> require RW on DB
  // - /collection/* -> require RO on DB + RW on collection
  auto const& suffix = _request->suffixes();
  switch (suffix.size()) {
    case 0: {
      // /_api/schema path
      if (!exec.canUseDatabase(auth::Level::RW)) {
        generateError(ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                      "insufficient database permissions");
        return RestStatus::DONE;
      }
      if (lookupSchema(sampleNum, exampleNum).fail()) {
        return RestStatus::DONE;
      }
      return handleQueryResult();
    }
    case 2: {
      if (suffix[0] == "collection") {
        // /_api/schema/collection/<collection-name>
        if (!exec.canUseDatabase(auth::Level::RO) ||
            !exec.canUseCollection(suffix[1], auth::Level::RW)) {
          LOG_DEVEL << "place 1";
          generateError(ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                        "insufficient permissions on collection or database");
          return RestStatus::DONE;
        }
        if (lookupSchemaCollection(suffix[1], sampleNum, exampleNum).fail()) {
          return RestStatus::DONE;
        }
        return handleQueryResult();
      }
      if (suffix[0] == "graph") {
        // /_api/schema/graph/<graph-name>
        if (!exec.canUseDatabase(auth::Level::RW)) {
          generateError(ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                        "insufficient database permissions");
          return RestStatus::DONE;
        }
        if (lookupSchemaGraph(suffix[1], sampleNum, exampleNum).fail()) {
          return RestStatus::DONE;
        }
        return handleQueryResult();
      }
      if (suffix[0] == "view") {
        // /_api/schema/view/<view-name>
        if (!exec.canUseDatabase(auth::Level::RW)) {
          generateError(ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                        "insufficient database permissions");
          return RestStatus::DONE;
        }
        if (lookupSchemaView(suffix[1], sampleNum, exampleNum).fail()) {
          return RestStatus::DONE;
        }
        return handleQueryResult();
      }
      [[fallthrough]];  // If suffix[0] is none of "collection", "graph" or
      // "view", go to default
    }
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

Result RestSchemaHandler::lookupSchema(uint64_t sampleNum,
                                       uint64_t exampleNum) {
  auto resultBuilder = std::make_shared<velocypack::Builder>();
  std::set<std::string> colSet;
  resultBuilder->openObject();

  Result graphsRes = getAllGraphsAndCollections(*resultBuilder, colSet);
  if (graphsRes.fail()) {
    return graphsRes;
  }

  Result viewsRes = getAllViewsAndCollections(*resultBuilder, colSet);
  if (viewsRes.fail()) {
    return viewsRes;
  }

  Result colsRes =
      getAllCollections(colSet, sampleNum, exampleNum, *resultBuilder);
  if (colsRes.fail()) {
    return colsRes;
  }
  resultBuilder->close();
  _queryResult.data = resultBuilder;
  return Result{};
}

Result RestSchemaHandler::lookupSchemaCollection(std::string const& colName,
                                                 uint64_t sampleNum,
                                                 uint64_t exampleNum) {
  auto found = CollectionNameResolver(_vocbase).getCollection(colName);
  if (found == nullptr) {
    Result res{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
               std::format("Collection {} not found", colName)};
    generateError(ResponseCode::NOT_FOUND, res.errorNumber(),
                  res.errorMessage());
    return res;
  }

  auto resultBuilder = std::make_shared<velocypack::Builder>();
  resultBuilder->openObject();
  Result colRes = getCollection(colName, sampleNum, exampleNum, *resultBuilder);
  if (colRes.fail()) {
    return colRes;
  }
  resultBuilder->close();
  _queryResult.data = resultBuilder;
  return Result{};
}

Result RestSchemaHandler::lookupSchemaGraph(std::string const& graphName,
                                            uint64_t sampleNum,
                                            uint64_t exampleNum) {
  auto resultBuilder = std::make_shared<velocypack::Builder>();
  std::set<std::string> colSet;

  resultBuilder->openObject();
  Result graphRes = getGraphAndCollections(graphName, *resultBuilder, colSet);
  if (graphRes.fail()) {
    return graphRes;
  }

  Result colsRes =
      getAllCollections(colSet, sampleNum, exampleNum, *resultBuilder);
  if (colsRes.fail()) {
    return colsRes;
  }
  resultBuilder->close();

  _queryResult.data = resultBuilder;
  return Result{};
}

Result RestSchemaHandler::lookupSchemaView(std::string const& viewName,
                                           uint64_t sampleNum,
                                           uint64_t exampleNum) {
  velocypack::Builder viewsArrBuilder;
  viewsArrBuilder.openArray();
  std::set<std::string> colSet;
  Result viewRes = getViewAndCollections(viewName, viewsArrBuilder, colSet);
  if (viewRes.fail()) {
    return viewRes;
  }
  viewsArrBuilder.close();

  auto resultBuilder = std::make_shared<velocypack::Builder>();
  resultBuilder->openObject();
  resultBuilder->add("views", viewsArrBuilder.slice());

  Result colsRes =
      getAllCollections(colSet, sampleNum, exampleNum, *resultBuilder);
  if (colsRes.fail()) {
    return colsRes;
  }
  resultBuilder->close();

  _queryResult.data = resultBuilder;
  return Result{};
}

Result RestSchemaHandler::getCollection(std::string const& colName,
                                        uint64_t sampleNum, uint64_t exampleNum,
                                        velocypack::Builder& colBuilder) {
  auto colPtr = CollectionNameResolver(_vocbase).getCollection(colName);
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
  bindVars->add("exampleNum", VPackValue(exampleNum));
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

  auto dataArr = qr.data->slice();
  TRI_ASSERT(dataArr.isArray() && dataArr.length() > 0);
  auto data = dataArr[0];
  TRI_ASSERT(data.isObject() && data.hasKey("num") && data.hasKey("schemas") &&
             data.hasKey("examples"));

  colBuilder.add("collectionName", velocypack::Value(colName));
  if (colPtr->type() == TRI_COL_TYPE_DOCUMENT) {
    colBuilder.add("collectionType", velocypack::Value("document"));
    colBuilder.add("numOfDocuments", data.get("num"));
  } else {
    colBuilder.add("collectionType", velocypack::Value("edge"));
    colBuilder.add("numOfEdges", data.get("num"));
  }

  Result indexRes = getIndexes(colName, colBuilder);
  if (indexRes.fail()) {
    return indexRes;
  }

  colBuilder.add("schema", data.get("schemas"));
  colBuilder.add("examples", data.get("examples"));

  return Result{};
}

Result RestSchemaHandler::getAllCollections(std::set<std::string> const& colSet,
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
  TRI_ASSERT(slice.isArray());

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
  TRI_ASSERT(slice.isArray());
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

  velocypack::Builder viewBuilder;
  viewBuilder.add(velocypack::Value(velocypack::ValueType::Object));
  viewBuilder.add("viewName", velocypack::Value(viewName));
  viewBuilder.add("links", velocypack::Value(velocypack::ValueType::Array));
  auto data = dataBuilder.slice();
  if (data.hasKey("links")) {  // If not, linksBuilder will be an empty array
    for (auto li : velocypack::ObjectIterator(data.get("links"))) {
      auto colName = li.key.copyString();
      auto colValue = li.value;
      TRI_ASSERT(colValue.isObject() && colValue.hasKey("fields") &&
                 colValue.hasKey("includeAllFields") &&
                 colValue.hasKey("analyzers"));
      viewBuilder.add(velocypack::Value(velocypack::ValueType::Object));
      viewBuilder.add("collectionName", velocypack::Value(colName));
      viewBuilder.add("fields", velocypack::ValueType::Array);
      std::set<std::string> includedAttrSet;
      for (auto fi : velocypack::ObjectIterator(colValue.get("fields"))) {
        viewBuilder.add(velocypack::Value(velocypack::ValueType::Object));
        TRI_ASSERT(fi.value.hasKey("analyzers"));

        viewBuilder.add("attribute", velocypack::Value(fi.key.copyString()));
        viewBuilder.add("analyzers", fi.value.get("analyzers"));
        viewBuilder
            .close();  // Closing object -> {attribute: ***, analyzers: ***}
        includedAttrSet.insert(fi.key.copyString());
      }
      viewBuilder.close();  // Closing array -> fields: [{}, {}]
      if (colValue.get("includeAllFields").isTrue()) {
        viewBuilder.add("allAttributeAnalyzers", colValue.get("analyzers"));
      }
      viewBuilder
          .close();  // Closing object -> {collectionName: ***, fields: []}
      colSet.insert(colName);
    }
  }
  viewBuilder.close();  // Closing array -> links: [{}, {}]
  viewBuilder.close();  // Closing object -> {viewName: ***, links: []}

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

Result RestSchemaHandler::getIndexes(std::string const& colName,
                                     velocypack::Builder& builder) {
  auto colPtr = CollectionNameResolver(_vocbase).getCollection(colName);
  if (!colPtr) {
    Result res{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
               std::format("Collection {} not found", colName)};
    generateError(ResponseCode::NOT_FOUND, res.errorNumber(),
                  res.errorMessage());
    return res;
  }
  velocypack::Builder indexesBuilder;
  Result indexRes = methods::Indexes::getAll(*colPtr, Index::makeFlags(), false,
                                             indexesBuilder)
                        .waitAndGet();
  if (indexRes.fail()) {
    generateError(ResponseCode::SERVER_ERROR, indexRes.errorNumber(),
                  indexRes.errorMessage());
    return indexRes;
  }

  builder.add("indexes", velocypack::Value(velocypack::ValueType::Array));
  auto indexesData = indexesBuilder.slice();
  TRI_ASSERT(indexesData.isArray());
  for (auto ind : VPackArrayIterator(indexesData)) {
    TRI_ASSERT(ind.isObject() && ind.get("fields").isArray() &&
               ind.get("name").isString() && ind.get("sparse").isBoolean() &&
               ind.get("type").isString() && ind.get("unique").isBoolean());

    const auto indType = ind.get("type").stringView();
    if (indType != "primary" && indType != "edge") {
      builder.add(velocypack::Value(velocypack::ValueType::Object));
      builder.add("fields", ind.get("fields"));
      builder.add("name", ind.get("name").stringView());
      builder.add("sparse", ind.get("sparse").getBoolean());
      builder.add("type", ind.get("type").stringView());
      builder.add("unique", ind.get("unique").getBoolean());
      builder.close();  // Closing Json Object -> {fields: ***, name: ***, ...}
    }
  }
  builder
      .close();  // Closing Json Array -> indexes: [{fields: ***, ...}, {}, {}]

  return Result{};
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