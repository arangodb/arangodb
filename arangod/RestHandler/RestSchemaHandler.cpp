//
// Created by koichi on 6/17/25.
//

#include "RestHandler/RestVocbaseBaseHandler.h"
#include "RestHandler/RestSchemaHandler.h"
#include "Aql/QueryRegistry.h"
#include "Aql/Query.h"
#include "Basics/VelocyPackHelper.h"
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
  : RestCursorHandler(server, request, response, queryRegistry) {}

RestStatus RestSchemaHandler::execute() {
  if (_request->requestType() != RequestType::GET) {
    generateError(ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

  uint64_t sampleNum = validateParameter("sampleNum");
  uint64_t exampleNum = validateParameter("exampleNum");

  if (sampleNum == 0 || exampleNum == 0) { return RestStatus::DONE; }
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
        return RestStatus::DONE;
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

RestStatus RestSchemaHandler::lookupSchema(uint64_t sampleNum, uint64_t exampleNum) {
  auto ctx = std::make_shared<transaction::StandaloneContext>(
    _vocbase, transaction::OperationOriginTestCase{});

  auto collectionSchemaArray = std::make_shared<velocypack::Builder>();
  collectionSchemaArray->openArray();
  for (auto const& colName : _vocbase.collectionNames()) {
    if (colName.starts_with("_")) { continue; }
    collectionSchemaArray->add(lookupCollection(colName, sampleNum, exampleNum));
  }
  collectionSchemaArray->close();

  auto resultBuilder = std::make_shared<velocypack::Builder>();
  resultBuilder->openObject();
  resultBuilder->add("collections", collectionSchemaArray->slice());
  resultBuilder->add("graphs", lookupGraph(ctx));
  resultBuilder->close();

  _queryResult.data = std::move(resultBuilder);
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

  _queryResult.data = std::make_shared<velocypack::Builder>(
    lookupCollection(colName, sampleNum, exampleNum));
  _queryResult.context = std::make_shared<transaction::StandaloneContext>(
    _vocbase, transaction::OperationOriginTestCase{});
  return handleQueryResult();
}

const velocypack::Slice RestSchemaHandler::lookupGraph(
  std::shared_ptr<transaction::StandaloneContext> ctx) {
  const std::string graphQueryString = R"(
    FOR g IN _graphs
    RETURN {
      name: g._key,
      relations: g.edgeDefinitions
    }
)";

  auto query = aql::Query::create(
      ctx, aql::QueryString{graphQueryString},
      std::make_shared<velocypack::Builder>(),
      aql::QueryOptions{velocypack::Parser::fromJson("{}")->slice()}
      );

  aql::QueryResult qr;
  while (query->execute(qr) == aql::ExecutionState::WAITING) { }

  return qr.data->slice();
}

const velocypack::Slice RestSchemaHandler::lookupCollection(std::string const& colName,
    uint64_t sampleNum, uint64_t exampleNum) {
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
  while (query->execute(qr) == aql::ExecutionState::WAITING) { }

  auto resultBuilder = std::make_shared<velocypack::Builder>();
  resultBuilder->openObject();
  resultBuilder->add("collectionName", velocypack::Value(colName));

  auto col = _vocbase.lookupCollection(colName);
  if (col && col->type() == TRI_COL_TYPE_DOCUMENT) {
    resultBuilder->add("collectionType", velocypack::Value("document"));
    resultBuilder->add("numOfDocuments", getCount(colName));
  } else {
    resultBuilder->add("collectionType", velocypack::Value("edge"));
    resultBuilder->add("numOfEdges", getCount(colName));
  }
  resultBuilder->add("schema", qr.data->slice());
  resultBuilder->add("examples", getExamples(colName, exampleNum));
  resultBuilder->close();

  return resultBuilder->slice();
};

const velocypack::Slice RestSchemaHandler::getCount(std::string const& colName) {
  const std::string queryStr = R"(
    RETURN LENGTH(@@collection)
  )";

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
  while (query->execute(qr) == aql::ExecutionState::WAITING) { }

  auto arr = qr.data->slice();
  if (arr.length() > 0)
    return arr.at(0);

  return qr.data->slice();
}

const velocypack::Slice RestSchemaHandler::getExamples(
  std::string const& colName, uint64_t exampleNum) {
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
  while (query->execute(qr) == aql::ExecutionState::WAITING) { }
  return qr.data->slice();
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

uint64_t RestSchemaHandler::validateParameter(const std::string& param) {
  bool passed = false;
  uint64_t numValue = 0;
  if (param == "sampleNum") { numValue = 100; }
  else if (param == "exampleNum") { numValue = 1; }
  else { return numValue; }

  auto const& val = _request->value(param, passed);
  if (passed) {
    if (!std::all_of(val.begin(), val.end(),
      [](char c) { return std::isdigit(c); })) {
      generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        std::format("Invalid value for {}: must contain only digits", param));
      return 0;
    }
    try {
      unsigned long long userInput = std::stoull(val);
      if (userInput == 0) {
        generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                      std::format("{} must be greater than 0", param));
        return 0;
      }
      numValue = userInput;
    } catch (std::out_of_range&) {
      generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    std::format("Value for {} is too large", param));
      return 0;
    } catch (...) {
      generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    std::format("Unexpected error parsing {}", param));
      return 0;
    }
  }
  return numValue;
}