////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RestSimpleHandler.h"
#include "Aql/BindParameters.h"
#include "Aql/QueryString.h"
#include "Basics/Exceptions.h"
#include "Basics/MutexLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Basics/VelocyPackHelper.h"
#include "Graph/Traverser.h"
#include "Transaction/Context.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::rest;

RestSimpleHandler::RestSimpleHandler(application_features::ApplicationServer& server,
                                     GeneralRequest* request, GeneralResponse* response,
                                     arangodb::aql::QueryRegistry* queryRegistry)
    : RestCursorHandler(server, request, response, queryRegistry), _silent(true) {}

RestStatus RestSimpleHandler::execute() {
  // extract the request type
  auto const type = _request->requestType();

  if (type == rest::RequestType::PUT) {
    bool parsingSuccess = false;
    VPackSlice const body = this->parseVPackBody(parsingSuccess);
    if (!parsingSuccess) {
      return RestStatus::DONE;
    }

    if (!body.isObject()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_TYPE_ERROR,
                    "expecting JSON object body");
      return RestStatus::DONE;
    }

    std::string const& prefix = _request->requestPath();

    if (prefix == RestVocbaseBaseHandler::SIMPLE_REMOVE_PATH) {
      return removeByKeys(body);
    } else if (prefix == RestVocbaseBaseHandler::SIMPLE_LOOKUP_PATH) {
      return lookupByKeys(body);
    } else {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_TYPE_ERROR,
                    "unsupported value for <operation>");
    }

    return RestStatus::DONE;
  }

  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  return RestStatus::DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock RestRemoveByKeys
////////////////////////////////////////////////////////////////////////////////

RestStatus RestSimpleHandler::removeByKeys(VPackSlice const& slice) {
  TRI_ASSERT(slice.isObject());
  std::string collectionName;
  {
    VPackSlice const value = slice.get("collection");

    if (!value.isString()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_TYPE_ERROR,
                    "expecting string for <collection>");
      return RestStatus::DONE;
    }

    collectionName = value.copyString();

    if (!collectionName.empty() && collectionName[0] >= '0' && collectionName[0] <= '9') {
      // If we have a numeric name we probably have to translate it.
      CollectionNameResolver resolver(_vocbase);

      collectionName = resolver.getCollectionName(collectionName);
    }
  }

  VPackSlice const keys = slice.get("keys");

  if (!keys.isArray()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_TYPE_ERROR,
                  "expecting array for <keys>");
    return RestStatus::DONE;
  }

  bool waitForSync = false;
  bool returnOld = false;
  _silent = true;
  {
    VPackSlice const value = slice.get("options");
    if (value.isObject()) {
      VPackSlice wfs = value.get("waitForSync");
      if (wfs.isBool()) {
        waitForSync = wfs.getBool();
      }
      wfs = value.get("silent");
      if (wfs.isBool()) {
        _silent = wfs.getBool();
      }
      wfs = value.get("returnOld");
      if (wfs.isBool()) {
        returnOld = wfs.getBool();
      }
    }
  }

  std::string aql(
      "FOR key IN @keys REMOVE key IN @@collection OPTIONS { ignoreErrors: "
      "true, waitForSync: ");
  aql.append(waitForSync ? "true" : "false");
  aql.append(" }");
  if (!_silent) {
    if (returnOld) {
      aql.append(" RETURN OLD");
    } else {
      aql.append(" RETURN {_id: OLD._id, _key: OLD._key, _rev: OLD._rev}");
    }
  }

  VPackBuilder data;
  data.openObject();
  data.add("query", VPackValue(aql));
  data.add(VPackValue("bindVars"));
  data.openObject();  // bindVars
  data.add("@collection", VPackValue(collectionName));
  data.add("keys", keys);
  data.close();  // bindVars
  data.close();

  return registerQueryOrCursor(data.slice());
}

RestStatus RestSimpleHandler::handleQueryResult() {
  if (_queryResult.result.fail()) {
    if (_queryResult.result.is(TRI_ERROR_REQUEST_CANCELED) ||
        (_queryResult.result.is(TRI_ERROR_QUERY_KILLED) && wasCanceled())) {
      generateError(GeneralResponse::responseCode(TRI_ERROR_REQUEST_CANCELED),
                    TRI_ERROR_REQUEST_CANCELED);
    } else {
      generateError(_queryResult.result);
    }
    return RestStatus::DONE;
  }

  // extract the request type
  auto const type = _request->requestType();
  std::string const& prefix = _request->requestPath();

  if (type == rest::RequestType::PUT) {
    if (prefix == RestVocbaseBaseHandler::SIMPLE_REMOVE_PATH) {
      handleQueryResultRemoveByKeys();
      return RestStatus::DONE;
    } else if (prefix == RestVocbaseBaseHandler::SIMPLE_LOOKUP_PATH) {
      handleQueryResultLookupByKeys();
      return RestStatus::DONE;
    }
  }

  // If we get here some checks before have already failed, we are
  // in an invalid state now.
  TRI_ASSERT(false);
  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  return RestStatus::DONE;
}

void RestSimpleHandler::handleQueryResultRemoveByKeys() {
  size_t ignored = 0;
  size_t removed = 0;
  if (_queryResult.extra) {
    VPackSlice stats = _queryResult.extra->slice().get("stats");
    if (!stats.isNone()) {
      TRI_ASSERT(stats.isObject());
      VPackSlice found = stats.get("writesIgnored");
      if (found.isNumber()) {
        ignored = found.getNumericValue<size_t>();
      }
      if ((found = stats.get("writesExecuted")).isNumber()) {
        removed = found.getNumericValue<size_t>();
      }
    }
  }

  VPackBuilder result;
  result.add(VPackValue(VPackValueType::Object));
  result.add("removed", VPackValue(removed));
  result.add("ignored", VPackValue(ignored));
  result.add(StaticStrings::Error, VPackValue(false));
  result.add(StaticStrings::Code, VPackValue(static_cast<int>(rest::ResponseCode::OK)));
  if (!_silent) {
    result.add("old", _queryResult.data->slice());
  }
  result.close();

  generateResult(rest::ResponseCode::OK, result.slice(), _queryResult.context);
}

void RestSimpleHandler::handleQueryResultLookupByKeys() {
  VPackBuffer<uint8_t> resultBuffer;
  VPackBuilder result(resultBuffer);
  {
    VPackObjectBuilder guard(&result);
    resetResponse(rest::ResponseCode::OK);

    response()->setContentType(rest::ContentType::JSON);
    result.add(VPackValue("documents"));

    result.addExternal(_queryResult.data->slice().begin());
    result.add(StaticStrings::Error, VPackValue(false));
    result.add(StaticStrings::Code,
               VPackValue(static_cast<int>(_response->responseCode())));
  }

  generateResult(rest::ResponseCode::OK, std::move(resultBuffer), _queryResult.context);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock RestLookupByKeys
////////////////////////////////////////////////////////////////////////////////

RestStatus RestSimpleHandler::lookupByKeys(VPackSlice const& slice) {
  if (response() == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid response");
  }

  std::string collectionName;
  {
    VPackSlice const value = slice.get("collection");

    if (!value.isString()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_TYPE_ERROR,
                    "expecting string for <collection>");
      return RestStatus::DONE;
    }

    collectionName = value.copyString();

    if (!collectionName.empty()) {
      auto col = _vocbase.lookupCollection(collectionName);

      if (col != nullptr && collectionName != col->name()) {
        // user has probably passed in a numeric collection id.
        // translate it into a "real" collection name
        collectionName = col->name();
      }
    }
  }

  VPackSlice const keys = slice.get("keys");

  if (!keys.isArray()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_TYPE_ERROR,
                  "expecting array for <keys>");
    return RestStatus::DONE;
  }

  std::string const aql(
      "FOR doc IN @@collection FILTER doc._key IN @keys RETURN doc");

  VPackBuilder data;
  data.openObject();
  data.add("query", VPackValue(aql));
  data.add(VPackValue("bindVars"));
  data.openObject();  // bindVars
  data.add("@collection", VPackValue(collectionName));
  data.add(VPackValue("keys"));
  arangodb::aql::BindParameters::stripCollectionNames(keys, collectionName, data);
  data.close();  // bindVars
  data.close();

  return registerQueryOrCursor(data.slice());
}
