//
// Created by koichi on 6/17/25.
//

#include "RestHandler/RestVocbaseBaseHandler.h"
#include "RestHandler/RestSchemaHandler.h"
#include "Aql/QueryRegistry.h"
#include "Aql/Query.h"
#include "Basics/VelocyPackHelper.h"
#include "Futures/Future.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/OperationOrigin.h"
#include "Graph/GraphManager.h"

#include "Logger/LogMacros.h"

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

const std::string RestSchemaHandler::graphQueryString = R"(
    FOR edge IN @@collection
      LET fromColl = PARSE_IDENTIFIER(edge._from).collection
      LET toColl = PARSE_IDENTIFIER(edge._to).collection

      COLLECT from = fromColl, to = toColl
      RETURN {from, to}
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

  uint64_t sampleNum = validateSampleNum();
  if (sampleNum == 0)
    return RestStatus::DONE;

  // path: /_api/schema or /_api/schema/<collection-name>
  auto const& suffix = _request->suffixes();
  if (suffix.empty()) {
    lookupSchema(sampleNum);
    return handleQueryResult();
  }
  if (suffix.size() != 1) {
    generateError(ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
    return RestStatus::DONE;
  }
  std::string const collection = suffix[0];

  // Check that the collection exists
  auto found = _vocbase.lookupCollection(collection);
  if (found == nullptr) {
    generateError(ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                  "collection '" + collection + "' not found");
    return RestStatus::DONE;
  }

  return waitForFuture(lookupCollectionSchema(collection, sampleNum));
}

futures::Future<RestStatus> RestSchemaHandler::lookupCollectionSchema(
    std::string const& collection, uint64_t sampleNum) {

  velocypack::Builder bind;
  bind.openObject();
  bind.add("@collection", VPackValue(collection));
  bind.add("sampleNum", VPackValue(sampleNum));
  bind.close();

  velocypack::Builder payload;
  payload.openObject();
  payload.add("query", VPackValue(queryString));
  payload.add("bindVars", bind.slice());
  payload.close();

  return registerQueryOrCursor(
    payload.slice(),
    transaction::OperationOriginREST{"schema endpoint"});
}

RestStatus RestSchemaHandler::lookupSchema(uint64_t sampleNum) {
  auto ctx = std::make_shared<transaction::StandaloneContext>(
    _vocbase, transaction::OperationOriginTestCase{});

  auto collectionSchemaArray = std::make_shared<velocypack::Builder>();
  collectionSchemaArray->openArray();
  for (auto const& colName : _vocbase.collectionNames()) {
    if (colName.starts_with("_"))
      continue;

    auto bindVars = std::make_shared<velocypack::Builder>();
    bindVars->openObject();
    bindVars->add("@collection", velocypack::Value(colName));
    bindVars->add("sampleNum",   velocypack::Value(sampleNum));
    bindVars->close();

    auto query = aql::Query::create(
      ctx, aql::QueryString{queryString}, bindVars,
      aql::QueryOptions{velocypack::Parser::fromJson("{}")->slice()}
    );

    aql::QueryResult qr;
    while (query->execute(qr) == aql::ExecutionState::WAITING) { }

    auto queryResultBuilder = std::make_shared<velocypack::Builder>();
    queryResultBuilder->openObject();
    queryResultBuilder->add("collectionName",   velocypack::Value(colName));

    auto col = _vocbase.lookupCollection(colName);
    std::string typeName =
      (col && col->type() == TRI_COL_TYPE_EDGE) ? "edge" : "document";
    queryResultBuilder->add("collectionType", velocypack::Value(typeName));
    queryResultBuilder->add("schema", qr.data->slice());
    queryResultBuilder->close();

    collectionSchemaArray->add(queryResultBuilder->slice());
  }
  collectionSchemaArray->close();

  auto resultBuilder = std::make_shared<velocypack::Builder>();
  resultBuilder->openObject();
  resultBuilder->add("collections", collectionSchemaArray->slice());
  resultBuilder->add("graphs", lookupGraph(ctx));
  resultBuilder->close();

  _queryResult.data = std::move(resultBuilder);
  _queryResult.context = std::move(ctx);
  return RestStatus::DONE;
}

const velocypack::Slice RestSchemaHandler::lookupGraph(std::shared_ptr<transaction::StandaloneContext> ctx) {
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

RestStatus RestSchemaHandler::handleQueryResult() {
  if (_queryResult.result.fail()) {
    generateError(_queryResult.result);
    return RestStatus::DONE;
  }
  auto resultSlice = _queryResult.data->slice();
  generateResult(ResponseCode::OK, resultSlice, _queryResult.context);
  return RestStatus::DONE;
}

// sampleNum: How many samples to pick up from each collection
uint64_t RestSchemaHandler::validateSampleNum() {
  bool passed = false;
  uint64_t sampleNum = 100;
  auto const& val = _request->value("sampleNum", passed);
  if (passed) {
    if (val.empty() || !std::all_of(val.begin(), val.end(),
                                    [](char c) { return std::isdigit(c); })) {
      generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid parameter sampleNum, must be a positive integer");
      return 0;
    }
    try {
      unsigned long long userSampleNum = std::stoull(val);
      if (userSampleNum == 0) {
        generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid parameter sampleNum, must be a positive integer");
        return 0;
      }

      sampleNum = userSampleNum;
    } catch (std::out_of_range&) {
      generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid parameter sampleNum, must be a positive integer");
      return 0;
    } catch (...) {
      generateError(ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid parameter sampleNum, must be a positive integer");
      return 0;
    }
  }
  return sampleNum;
}