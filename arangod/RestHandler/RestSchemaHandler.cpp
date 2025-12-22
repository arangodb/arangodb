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
constexpr std::string_view moduleName("schema endpoint");
}

const std::string RestSchemaHandler::_queryStr = R"(
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
                                     GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response),
      _graphManager(_vocbase, transaction::OperationOriginREST{::moduleName}),
      _nameResolver(_vocbase) {}

RestStatus RestSchemaHandler::execute() {
  if (_request->requestType() != RequestType::GET) {
    generateError(ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "Schema endpoint only accepts GET request");
    return RestStatus::DONE;
  }
  auto sampleRes = validateParameter("sampleNum", defaultSampleNum);
  auto exampleRes = validateParameter("exampleNum", defaultExampleNum, true);

  if (sampleRes.fail()) {
    generateError(sampleRes.result());
    return RestStatus::DONE;
  }
  if (exampleRes.fail()) {
    generateError(exampleRes.result());
    return RestStatus::DONE;
  }

  uint64_t sampleNum = sampleRes.get();
  uint64_t exampleNum = exampleRes.get();
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
      Result schemaRes = lookupSchema(sampleNum, exampleNum);
      if (schemaRes.fail()) {
        generateError(schemaRes);
        return RestStatus::DONE;
      }
      generateResult(ResponseCode::OK, _queryResult->slice());
      return RestStatus::DONE;
    }
    case 2: {
      if (suffix[0] == "collection") {
        // /_api/schema/collection/<collection-name>
        if (!exec.canUseDatabase(auth::Level::RO) ||
            !exec.canUseCollection(suffix[1], auth::Level::RW)) {
          generateError(ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                        "insufficient permissions on collection or database");
          return RestStatus::DONE;
        }
        Result colRes =
            lookupSchemaCollection(suffix[1], sampleNum, exampleNum);
        if (colRes.fail()) {
          generateError(colRes);
          return RestStatus::DONE;
        }
        generateResult(ResponseCode::OK, _queryResult->slice());
        return RestStatus::DONE;
      }
      if (suffix[0] == "graph") {
        // /_api/schema/graph/<graph-name>
        if (!exec.canUseDatabase(auth::Level::RW)) {
          generateError(ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                        "insufficient database permissions");
          return RestStatus::DONE;
        }
        Result graphRes = lookupSchemaGraph(suffix[1], sampleNum, exampleNum);
        if (graphRes.fail()) {
          generateError(graphRes);
          return RestStatus::DONE;
        }
        generateResult(ResponseCode::OK, _queryResult->slice());
        return RestStatus::DONE;
      }
      if (suffix[0] == "view") {
        // /_api/schema/view/<view-name>
        if (!exec.canUseDatabase(auth::Level::RW)) {
          generateError(ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                        "insufficient database permissions");
          return RestStatus::DONE;
        }
        Result viewRes = lookupSchemaView(suffix[1], sampleNum, exampleNum);
        if (viewRes.fail()) {
          generateError(viewRes);
          return RestStatus::DONE;
        }
        generateResult(ResponseCode::OK, _queryResult->slice());
        return RestStatus::DONE;
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
  _queryResult = resultBuilder;
  return Result{};
}

Result RestSchemaHandler::lookupSchemaCollection(std::string const& colName,
                                                 uint64_t sampleNum,
                                                 uint64_t exampleNum) {
  auto found = _nameResolver.getCollection(colName);
  if (found == nullptr) {
    return Result{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                  std::format("Collection {} not found", colName)};
  }

  auto resultBuilder = std::make_shared<velocypack::Builder>();
  resultBuilder->openObject();
  Result colRes = getCollection(colName, sampleNum, exampleNum, *resultBuilder);
  if (colRes.fail()) {
    return colRes;
  }
  resultBuilder->close();
  _queryResult = resultBuilder;
  return Result{};
}

Result RestSchemaHandler::lookupSchemaGraph(std::string const& graphName,
                                            uint64_t sampleNum,
                                            uint64_t exampleNum) {
  auto resultBuilder = std::make_shared<velocypack::Builder>();
  std::set<std::string> colSet;

  resultBuilder->add(velocypack::Value(velocypack::ValueType::Object));
  resultBuilder->add("graphs", velocypack::Value(velocypack::ValueType::Array));
  auto gmRes = _graphManager.lookupGraphByName(graphName);
  if (gmRes.fail()) {
    return Result{gmRes.errorNumber(), gmRes.errorMessage()};
  }
  auto& graphPtr = gmRes.get();
  TRI_ASSERT(graphPtr);
  Result graphRes = getGraphAndCollections(*graphPtr, *resultBuilder, colSet);
  if (graphRes.fail()) {
    return graphRes;
  }
  resultBuilder->close();  // Closing Array -> graphs: [***]

  Result colsRes =
      getAllCollections(colSet, sampleNum, exampleNum, *resultBuilder);
  if (colsRes.fail()) {
    return colsRes;
  }
  resultBuilder->close();

  _queryResult = resultBuilder;
  return Result{};
}

Result RestSchemaHandler::lookupSchemaView(std::string const& viewName,
                                           uint64_t sampleNum,
                                           uint64_t exampleNum) {
  auto resultBuilder = std::make_shared<velocypack::Builder>();
  resultBuilder->add(velocypack::Value(velocypack::ValueType::Object));
  resultBuilder->add("views", velocypack::Value(velocypack::ValueType::Array));
  std::set<std::string> colSet;
  Result viewRes = getViewAndCollections(viewName, *resultBuilder, colSet);
  if (viewRes.fail()) {
    return viewRes;
  }
  resultBuilder->close();  // Closing Array -> views: [{}, {}]

  Result colsRes =
      getAllCollections(colSet, sampleNum, exampleNum, *resultBuilder);
  if (colsRes.fail()) {
    return colsRes;
  }
  resultBuilder->close();

  _queryResult = resultBuilder;
  return Result{};
}

Result RestSchemaHandler::getCollection(std::string const& colName,
                                        uint64_t sampleNum, uint64_t exampleNum,
                                        velocypack::Builder& colBuilder) {
  auto colPtr = _nameResolver.getCollection(colName);
  if (!colPtr) {
    return Result{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                  std::format("Collection {} not found", colName)};
  }

  auto bindVars = std::make_shared<velocypack::Builder>();
  bindVars->openObject();
  bindVars->add("@collection", VPackValue(colName));
  bindVars->add("sampleNum", VPackValue(sampleNum));
  bindVars->add("exampleNum", VPackValue(exampleNum));
  bindVars->close();

  auto origin = transaction::OperationOriginAQL{"querying collection's schema"};
  auto query = aql::Query::create(
      transaction::StandaloneContext::create(_vocbase, origin),
      aql::QueryString{_queryStr}, bindVars,
      aql::QueryOptions{velocypack::Parser::fromJson("{}")->slice()});

  aql::QueryResult qr = query->executeSync();
  if (qr.result.fail()) {
    return Result{qr.result.errorNumber(),
                  std::format("Schema query failed for {}: {}", colName,
                              qr.result.errorMessage())};
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

  Result indexRes = getIndexes(*colPtr, colBuilder);
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
  colsBuilder.add("collections",
                  velocypack::Value(velocypack::ValueType::Array));
  for (auto const& colName : colSet) {
    if (colName.starts_with("_")) {
      continue;
    }
    colsBuilder.add(velocypack::Value(velocypack::ValueType::Object));
    // getCollection() assumes JSON Object is open
    Result colRes = getCollection(colName, sampleNum, exampleNum, colsBuilder);
    if (colRes.fail()) {
      return colRes;
    }
    colsBuilder
        .close();  // Closing Object -> {collectionName: ***, ..., examples: []}
  }
  colsBuilder.close();  // Closing Array -> collections: [{}, {}]
  return Result{};
}

Result RestSchemaHandler::getGraphAndCollections(
    graph::Graph const& graph, velocypack::Builder& graphBuilder,
    std::set<std::string>& colSet) {
  graphBuilder.add(velocypack::Value(velocypack::ValueType::Object));
  graphBuilder.add("name", velocypack::Value(graph.name()));
  graphBuilder.add("relations",
                   velocypack::Value(velocypack::ValueType::Array));

  for (const auto& edgeDef : graph.edgeDefinitions()) {
    const std::string colName = edgeDef.first;
    graphBuilder.add(velocypack::Value(velocypack::ValueType::Object));
    graphBuilder.add("collection", velocypack::Value(colName));
    graphBuilder.add("from", velocypack::Value(velocypack::ValueType::Array));
    colSet.insert(colName);
    for (const auto& fr : edgeDef.second.getFrom()) {
      graphBuilder.add(velocypack::Value(fr));
      colSet.insert(fr);
    }
    graphBuilder.close();  // Closing Array -> from: [***]
    graphBuilder.add("to", velocypack::Value(velocypack::ValueType::Array));
    for (const auto& to : edgeDef.second.getTo()) {
      graphBuilder.add(velocypack::Value(to));
      colSet.insert(to);
    }
    graphBuilder.close();  // Closing Array -> to: [***]
    graphBuilder
        .close();  // Closing Object -> {collection: ***, from: [], to: []}
  }
  graphBuilder.close();  // Closing Array -> relations: [***]

  graphBuilder.add("orphans", velocypack::Value(velocypack::ValueType::Array));
  for (const auto& orphan : graph.orphanCollections()) {
    graphBuilder.add(velocypack::Value(orphan));
    colSet.insert(orphan);
  }
  graphBuilder.close();  // Closing Array -> orphans: [***]
  graphBuilder.close();  // Closing Object -> {name: ***, relations: []}

  return Result{};
}

Result RestSchemaHandler::getAllGraphsAndCollections(
    velocypack::Builder& graphBuilder, std::set<std::string>& colSet) {
  auto gmRes = _graphManager.lookupAllGraphs();
  if (gmRes.fail()) {
    return Result{gmRes.errorNumber(), gmRes.errorMessage()};
  }
  auto& graphList = gmRes.get();
  graphBuilder.add("graphs", velocypack::Value(velocypack::ValueType::Array));
  for (auto const& graphPtr : graphList) {
    TRI_ASSERT(graphPtr);
    Result graphRes = getGraphAndCollections(*graphPtr, graphBuilder, colSet);
    if (graphRes.fail()) {
      return graphRes;
    }
  }
  graphBuilder.close();  // Closing Array -> graphs: [***]
  return Result{};
}

Result RestSchemaHandler::getViewAndCollections(
    std::string const& viewName, velocypack::Builder& viewsArrBuilder,
    std::set<std::string>& colSet) {
  auto view = _nameResolver.getView(viewName);
  if (!view) {
    return Result{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                  std::format("View {} not found", viewName)};
  }

  velocypack::Builder dataBuilder;
  dataBuilder.openObject();
  auto propRes = view->properties(dataBuilder,
                                  LogicalDataSource::Serialization::Properties);
  if (!propRes.ok()) {
    return propRes;
  }
  dataBuilder.close();

  velocypack::Builder viewBuilder;
  viewBuilder.add(velocypack::Value(velocypack::ValueType::Object));
  viewBuilder.add("viewName", velocypack::Value(viewName));
  viewBuilder.add("links", velocypack::Value(velocypack::ValueType::Array));
  auto data = dataBuilder.slice();
  if (data.hasKey("links")) {  // If not, linksBuilder will be an empty array
    for (auto const& li : velocypack::ObjectIterator(data.get("links"))) {
      auto colName = li.key.copyString();
      auto colValue = li.value;
      TRI_ASSERT(colValue.isObject() && colValue.hasKey("fields") &&
                 colValue.hasKey("includeAllFields") &&
                 colValue.hasKey("analyzers"));
      viewBuilder.add(velocypack::Value(velocypack::ValueType::Object));
      viewBuilder.add("collectionName", velocypack::Value(colName));
      viewBuilder.add("fields", velocypack::ValueType::Array);
      auto defaultAnalyzer = colValue.get("analyzers");
      std::set<std::string> includedAttrSet;
      for (auto const& fi :
           velocypack::ObjectIterator(colValue.get("fields"))) {
        viewBuilder.add(velocypack::Value(velocypack::ValueType::Object));
        viewBuilder.add("attribute", velocypack::Value(fi.key.copyString()));
        // Some fields don't have "analyzers" attribute when they define it at
        // the field level.
        viewBuilder.add("analyzers", fi.value.hasKey("analyzers")
                                         ? fi.value.get("analyzers")
                                         : defaultAnalyzer);

        viewBuilder
            .close();  // Closing object -> {attribute: ***, analyzers: ***}
        includedAttrSet.insert(fi.key.copyString());
      }
      viewBuilder.close();  // Closing array -> fields: [{}, {}]
      if (colValue.get("includeAllFields").isTrue()) {
        viewBuilder.add("allAttributeAnalyzers", defaultAnalyzer);
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

  viewsBuilder.add("views", velocypack::Value(velocypack::ValueType::Array));
  for (auto view : views) {
    TRI_ASSERT(view != nullptr);
    if (!view) {
      continue;
    }
    Result viewRes = getViewAndCollections(view->name(), viewsBuilder, colSet);
    if (viewRes.fail()) {
      return viewRes;
    }
  }
  viewsBuilder.close();  // Closing Array -> views: [{}, {}, ...]
  return Result{};
}

Result RestSchemaHandler::getIndexes(LogicalCollection const& col,
                                     velocypack::Builder& builder) {
  velocypack::Builder indexesBuilder;
  Result indexRes =
      methods::Indexes::getAll(col, Index::makeFlags(), false, indexesBuilder)
          .waitAndGet();
  if (indexRes.fail()) {
    return indexRes;
  }

  builder.add("indexes", velocypack::Value(velocypack::ValueType::Array));
  auto indexesData = indexesBuilder.slice();
  TRI_ASSERT(indexesData.isArray());
  std::vector<std::string> keepAttrs = {"fields", "name", "sparse", "type",
                                        "unique"};
  for (auto ind : VPackArrayIterator(indexesData)) {
    TRI_ASSERT(ind.isObject());
    const auto indType = ind.get("type").stringView();
    if (indType != "primary" && indType != "edge") {
      // Some indexes might not have 'unique' attribute (or other attributes),
      // but that's fine, just add whatever attribute the index holds
      velocypack::Builder extracted = VPackCollection::keep(ind, keepAttrs);
      builder.add(extracted.slice());
    }
  }
  builder
      .close();  // Closing Json Array -> indexes: [{fields: ***, ...}, {}, {}]

  return Result{};
}

ResultT<uint64_t> RestSchemaHandler::validateParameter(const std::string& param,
                                                       uint64_t defaultValue,
                                                       bool allowZero) {
  bool passed = false;

  auto const& val = _request->value(param, passed);
  if (!passed) {
    return defaultValue;
  }

  if (!std::all_of(val.begin(), val.end(),
                   [](char c) { return std::isdigit(c); })) {
    return Result{
        TRI_ERROR_HTTP_BAD_PARAMETER,
        std::format("Invalid value for {}: must contain only digits", param)};
  }

  unsigned long long userInput = 0;
  try {
    userInput = std::stoull(val);
  } catch (std::out_of_range&) {
    return Result{TRI_ERROR_HTTP_BAD_PARAMETER,
                  std::format("Value for {} is too large", param)};
  } catch (...) {
    return Result{TRI_ERROR_HTTP_BAD_PARAMETER,
                  std::format("Unexpected error parsing {}", param)};
  }
  if (userInput == 0 && !allowZero) {
    return Result{TRI_ERROR_HTTP_BAD_PARAMETER,
                  std::format("{} must be greater than 0", param)};
  }
  return userInput;
}