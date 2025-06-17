//
// Created by koichi on 6/17/25.
//

#include "RestSchemaHandler.h"
#include "Aql/BindParameters.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Transaction/OperationOrigin.h"
#include "Utils/CollectionNameResolver.h"
#include <velocypack/Builder.h>

using namespace arangodb;
using namespace arangodb::rest;

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

  // path: /_api/schema/<collection-name>
  auto const& suffix = _request->suffixes();
  if (suffix.size() != 1) {
    generateError(ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
    return RestStatus::DONE;
  }
  std::string const collection = suffix[0];

  // sampleNum is an optional parameter
  bool passed = false;
  int64_t sampleNum = 100;
  auto const& val = _request->value("sampleNum", passed);
  if (passed) {
    // int64("no-number-value") will return 0
    sampleNum = basics::StringUtils::int64(val);
    sampleNum = std::max<int64_t>(1, sampleNum);
  }

  // Check that the collection exists
  auto found = _vocbase.lookupCollection(collection);
  if (found == nullptr) {
    generateError(ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                  "collection '" + collection + "' not found");
    return RestStatus::DONE;
  }

  return waitForFuture(lookupSchema(collection, sampleNum));
}

futures::Future<RestStatus> RestSchemaHandler::lookupSchema(
    std::string const& collection, int64_t sampleNum) {

  // AQL uses @collection and @sampleNum
  std::string const aql = R"(
    LET total = @sampleNum

    LET docs = (
      FOR d IN @@collection
        SORT RAND()
        LIMIT total
        RETURN d
    )

    FOR d IN docs
      LET keys = ATTRIBUTES(d, true)
      FOR key IN keys
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

  velocypack::Builder bind;
  bind.openObject();
  bind.add("@collection", VPackValue(collection));
  bind.add("sampleNum", VPackValue(sampleNum));
  bind.close();

  velocypack::Builder payload;
  payload.openObject();
  payload.add("query", VPackValue(aql));
  payload.add("bindVars", bind.slice());
  payload.close();

  return registerQueryOrCursor(
    payload.slice(),
    transaction::OperationOriginREST{"schema endpoint"});
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