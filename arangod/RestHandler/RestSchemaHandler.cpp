//
// Created by koichi on 6/17/25.
//

#include "RestHandler/RestVocbaseBaseHandler.h"
#include "RestHandler/RestSchemaHandler.h"
#include "Aql/QueryRegistry.h"
#include "Aql/Query.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/OperationOrigin.h"
#include "Graph/GraphManager.h"

#include <velocypack/Builder.h>
#include <velocypack/vpack.h>
#include <Basics/voc-errors.h>
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
    generateError(ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "Schema endpoint only accepts GET request");
    return RestStatus::DONE;
  }

  auto maybeSampleNum = validateParameter(
    "sampleNum", defaultSampleNum);
  auto maybeExampleNum = validateParameter(
    "exampleNum", defaultExampleNum, true);

  if (!maybeSampleNum || !maybeExampleNum)
    return RestStatus::DONE;

  uint64_t sampleNum = *maybeSampleNum;
  uint64_t exampleNum = *maybeExampleNum;
  if (sampleNum < exampleNum) {
    generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
       "Parameter exampleNum must be equal to or smaller than sampleNum");
    return RestStatus::DONE;
  }

  auto const& suffix = _request->suffixes();
  switch (suffix.size()) {
    case 0:
      return lookupSchema(sampleNum, exampleNum);
    case 2:
      if (suffix[0] == "collection")
        return lookupSchemaCollection(suffix[1], sampleNum, exampleNum);
      if (suffix[0] == "graph")
        return lookupSchemaGraph(suffix[1], sampleNum, exampleNum);
      if (suffix[0] == "view")
        return RestStatus::DONE;
      [[fallthrough]]; // If suffix[0] is none of "collection", "graph" or "view", go to default
    default:
      generateError(ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
      "Illegal suffixes provided: must be /schema, /schema/collection/<collectionName>, "
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
  generateResult(ResponseCode::OK, resultSlice, _queryResult.context);
  return RestStatus::DONE;
}

RestStatus RestSchemaHandler::lookupSchema(uint64_t sampleNum, uint64_t exampleNum) {
  auto ctx = std::make_shared<transaction::StandaloneContext>(
    _vocbase, transaction::OperationOriginTestCase{});

  velocypack::Builder resultBuilder;
  std::set<std::string> colSet;
  resultBuilder.openObject();

  if (!getAllGraphsAndCollections(resultBuilder, colSet))
    return RestStatus::DONE;

  auto collectionSchemaArray = std::make_shared<velocypack::Builder>();
  collectionSchemaArray->openArray();
  for (auto const& colName : colSet) {
    if (colName.starts_with("_"))
      continue;

    velocypack::Builder tempBuilder;
    if (!getCollection(colName, sampleNum, exampleNum, tempBuilder))
      return RestStatus::DONE;
    collectionSchemaArray->add(tempBuilder.sharedSlice());
  }
  collectionSchemaArray->close();

  resultBuilder.add("collections", collectionSchemaArray->slice());

  resultBuilder.close();

  _queryResult.data = std::make_shared<velocypack::Builder>(resultBuilder);
  _queryResult.context = std::move(ctx);
  return handleQueryResult();
}

RestStatus RestSchemaHandler::lookupSchemaCollection(
    std::string const& colName, uint64_t sampleNum, uint64_t exampleNum) {
  auto found = _vocbase.lookupCollection(colName);
  if (found == nullptr) {
    generateError(ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                  "collection '" + colName + "' not found");
    return RestStatus::DONE;
  }

  velocypack::Builder tempBuilder;
  if (!getCollection(colName, sampleNum, exampleNum, tempBuilder))
    return RestStatus::DONE;
  _queryResult.data = std::make_shared<velocypack::Builder>(tempBuilder);
  _queryResult.context = std::make_shared<transaction::StandaloneContext>(
    _vocbase, transaction::OperationOriginTestCase{});
  return handleQueryResult();
}

RestStatus RestSchemaHandler::lookupSchemaGraph(std::string const& graphName,
  uint64_t sampleNum, uint64_t exampleNum) {
  velocypack::Builder resultBuilder;
  std::set<std::string> colSet;

  resultBuilder.openObject();
  if (!getGraphAndCollections(graphName, resultBuilder, colSet))
    return RestStatus::DONE;

  velocypack::Builder colsBuilder;
  colsBuilder.openArray();
  for (auto& colName : colSet) {
    velocypack::Builder colBuilder;
    if (!getCollection(colName, sampleNum, exampleNum, colBuilder))
      return RestStatus::DONE;
    colsBuilder.add(colBuilder.slice());
  }
  colsBuilder.close();

  resultBuilder.add("collections", colsBuilder.slice());
  resultBuilder.close();

  _queryResult.data = std::make_shared<velocypack::Builder>(resultBuilder);
  _queryResult.context = std::make_shared<transaction::StandaloneContext>(
    _vocbase, transaction::OperationOriginTestCase{});
  return handleQueryResult();
}

bool RestSchemaHandler::getCollection(std::string const& colName,
    uint64_t sampleNum, uint64_t exampleNum, velocypack::Builder& colBuilder) {
  auto colPtr = _vocbase.lookupCollection(colName);
  if (!colPtr) {
    generateError(ResponseCode::NOT_FOUND, TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
          std::format("Collection {} not found", colName));
    return false;
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
    aql::QueryOptions{velocypack::Parser::fromJson("{}")->slice()}
    );

  aql::QueryResult qr;
  try {
    while (query->execute(qr) == aql::ExecutionState::WAITING) { }
  } catch (basics::Exception const& e) {
    generateError(ResponseCode::SERVER_ERROR, e.code(),
      std::format("Schema query for {} threw: {}", colName, e.what())
    );
    return false;
  }

  if (qr.result.fail()) {
    generateError(ResponseCode::SERVER_ERROR, qr.result.errorNumber(),
      std::format("Schema query failed for {}: {}",
        colName, qr.result.errorMessage()));
    return false;
  }
  auto schemaSlice = qr.data->slice();
  if (!schemaSlice.isArray()) {
    generateError(ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
      std::format("Unexpected schema result for {}", colName));
    return false;
  }

  colBuilder.openObject();
  colBuilder.add("collectionName", velocypack::Value(colName));
  auto col = _vocbase.lookupCollection(colName);
  if (col && col->type() == TRI_COL_TYPE_DOCUMENT) {
    colBuilder.add("collectionType", velocypack::Value("document"));
    if (!getNumOfDocumentsOrEdges(colName, colBuilder))
      return false;
  } else {
    colBuilder.add("collectionType", velocypack::Value("edge"));
    if (!getNumOfDocumentsOrEdges(colName, colBuilder, false))
      return false;
  }
  colBuilder.add("schema", qr.data->slice());
  if (!getExamples(colName, exampleNum, colBuilder))
    return false;
  colBuilder.close();
  return true;
}

bool RestSchemaHandler::getGraphAndCollections(std::string const& graphName,
  velocypack::Builder& graphBuilder, std::set<std::string>& colSet) {
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
    while (query->execute(qr) == aql::ExecutionState::WAITING) { }
  } catch (basics::Exception const& e) {
    generateError(ResponseCode::SERVER_ERROR, e.code(),
      std::format("Graph query for '{}' threw: {}", graphName, e.what()));
    return false;
  }

  if (qr.result.fail()) {
    generateError(ResponseCode::SERVER_ERROR, qr.result.errorNumber(),
      std::format("Graph query failed for '{}': {}",
        graphName, qr.result.errorMessage()));
    return false;
  }

  auto slice = qr.data->slice();
  if (!slice.isArray()) {
    generateError(ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
      std::format("Unexpected graph result shape for '{}'", graphName));
    return false;
  }
  if (slice.length() == 0) {
    generateError(ResponseCode::NOT_FOUND, TRI_ERROR_GRAPH_NOT_FOUND,
      std::format("Graph not found: '{}'", graphName));
    return false;
  }
  graphBuilder.add("graphs", slice);

  if (!getConnectedCollections(graphName, colSet))
    return false;

  return true;
}

bool RestSchemaHandler::getAllGraphsAndCollections(
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
    while (query->execute(qr) == aql::ExecutionState::WAITING) { }
  } catch (basics::Exception const& ex) {
    generateError(ResponseCode::SERVER_ERROR, ex.code(),
      std::format("Graph query threw exception: {}", ex.what()));
    return false;
  }

  if (qr.result.fail()) {
    generateError(ResponseCode::SERVER_ERROR, qr.result.errorNumber(),
      std::format("Graph query failed: {}", qr.result.errorMessage()));
    return false;
  }

  auto slice = qr.data->slice();
  if (!slice.isArray()) {
    generateError(ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
      std::string("Unexpected result shape for GRAPH query: not an array"));
    return false;
  }

  graphBuilder.add("graphs", slice);

  for (auto entry : velocypack::ArrayIterator(slice)) {
    auto nameSlice = entry.get("name");
    if (!nameSlice.isString()) {
      generateError(ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
        std::format("Graph entry missing or invalid name attribute: {}", entry.toJson()));
      return false;
    }
    auto graphName = nameSlice.copyString();
    if (!getConnectedCollections(graphName, colSet))
      return false;
  }
  return true;
}

bool RestSchemaHandler::getNumOfDocumentsOrEdges(std::string const& colName,
  velocypack::Builder& builder, bool isDocument) {
  const std::string queryStr{"RETURN LENGTH(@@collection)"};

  auto bindVars = std::make_shared<velocypack::Builder>();
  bindVars->openObject();
  bindVars->add("@collection", velocypack::Value(colName));
  bindVars->close();

  auto query = aql::Query::create(
    std::make_shared<transaction::StandaloneContext>(
      _vocbase, transaction::OperationOriginTestCase{}),
    aql::QueryString{queryStr}, bindVars,
    aql::QueryOptions{velocypack::Parser::fromJson("{}")->slice()}
  );

  aql::QueryResult qr;
  try {
    while (query->execute(qr) == aql::ExecutionState::WAITING) { }
  } catch (basics::Exception const& e) {
    generateError(ResponseCode::SERVER_ERROR, e.code(),
      std::format("Count query for {} threw: {}",
        colName, e.what()));
    return false;
  }

  if (qr.result.fail()) {
    generateError(ResponseCode::SERVER_ERROR, qr.result.errorNumber(),
    std::format("Count query for {} failed: {}",
      colName, qr.result.errorMessage()));
    return false;
  }

  auto result = qr.data->slice();

  if (result.isNone() || !result.isArray()) {
    generateError(ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
    std::format("Count query did not return an array for {}", colName));
    return false;
  }

  if (result.length() == 0 || result.length() > 1) {
    generateError(ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
    std::format("Count query returned empty array or "
                "invalid format of array for {}", colName));
    return false;
  }

  auto countSlice = result.at(0);
  if (!countSlice.isNumber()) {
    generateError(ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
    std::format("Count query returned non-numeric result for {}", colName));
    return false;
  }

  if (isDocument)
    builder.add("numOfDocuments", result.at(0));
  else
    builder.add("numOfEdges", result.at(0));

  return true;
}

bool RestSchemaHandler::getExamples(std::string const& colName,
  uint64_t exampleNum, velocypack::Builder& builder) {
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
      aql::QueryOptions{velocypack::Parser::fromJson("{}")->slice()}
  );

  aql::QueryResult qr;
  try {
    while (query->execute(qr) == aql::ExecutionState::WAITING) { }
  } catch (basics::Exception const& e) {
    generateError(ResponseCode::SERVER_ERROR, e.code(),
      std::format("Example query for {} threw: {}",
        colName, e.what()));
    return false;
  }

  if (qr.result.fail()) {
    generateError(ResponseCode::SERVER_ERROR, qr.result.errorNumber(),
      std::format("Example query failed for {}: {}",
        colName, qr.result.errorMessage()));
    return false;
  }

  auto result = qr.data->slice();
  if (result.isNone() || !result.isArray()) {
    generateError(ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
    std::format("Example query did not return an array for {}", colName));
    return false;
  }

  builder.add("examples", result);
  return true;
}

std::optional<uint64_t> RestSchemaHandler::validateParameter(
  const std::string& param,uint64_t defaultValue, bool allowZero){
  bool passed = false;

  auto const& val = _request->value(param, passed);
  if (!passed)
    return defaultValue;

  if (!std::all_of(val.begin(), val.end(), [](char c){ return std::isdigit(c); })) {
    generateError(ResponseCode::BAD,TRI_ERROR_HTTP_BAD_PARAMETER,
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

bool RestSchemaHandler::getConnectedCollections(std::string const& graphName,
  std::set<std::string>& colSet) {
  auto resultGraph = _graphManager.lookupGraphByName(graphName);

  if (resultGraph.fail()) {
    if (resultGraph.errorNumber() == TRI_ERROR_GRAPH_NOT_FOUND) {
      generateError(ResponseCode::NOT_FOUND, resultGraph.errorNumber(),
        std::format("Graph {} not found", graphName));
    } else {
      generateError(ResponseCode::SERVER_ERROR, resultGraph.errorNumber(),
        std::format("Error looking up graph {}: ",
          graphName, resultGraph.errorMessage()));
    }
    return false;
  }

  auto& graphPtr = resultGraph.get();
  if (!graphPtr) {
    generateError(ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
      std::format("Graph lookup returned null pointer for {}", graphName));
    return false;
  }

  for (auto vertex : graphPtr->vertexCollections())
    colSet.insert(vertex);
  for (auto& edge : graphPtr->edgeCollections())
    colSet.insert(edge);
  return true;
}
