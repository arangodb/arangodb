////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "Basics/Exceptions.h"
#include "Basics/MutexLocker.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Basics/VelocyPackHelper.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "Utils/TransactionContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Traverser.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::rest;

RestSimpleHandler::RestSimpleHandler(
    GeneralRequest* request, GeneralResponse* response,
    arangodb::aql::QueryRegistry* queryRegistry)
    : RestVocbaseBaseHandler(request, response),
      _queryRegistry(queryRegistry),
      _queryLock(),
      _query(nullptr),
      _queryKilled(false) {}

RestHandler::status RestSimpleHandler::execute() {
  // extract the request type
  auto const type = _request->requestType();

  if (type == rest::RequestType::PUT) {
    bool parsingSuccess = true;
    std::shared_ptr<VPackBuilder> parsedBody =
        parseVelocyPackBody(&VPackOptions::Defaults, parsingSuccess);

    if (!parsingSuccess) {
      return status::DONE;
    }

    VPackSlice body = parsedBody.get()->slice();

    if (!body.isObject()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_TYPE_ERROR,
                    "expecting JSON object body");
      return status::DONE;
    }

    std::string const& prefix = _request->requestPath();

    if (prefix == RestVocbaseBaseHandler::SIMPLE_REMOVE_PATH) {
      removeByKeys(body);
    } else if (prefix == RestVocbaseBaseHandler::SIMPLE_LOOKUP_PATH) {
      lookupByKeys(body);
    } else {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_TYPE_ERROR,
                    "unsupported value for <operation>");
    }

    return status::DONE;
  }

  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  return status::DONE;
}

bool RestSimpleHandler::cancel() { return cancelQuery(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief register the currently running query
////////////////////////////////////////////////////////////////////////////////

void RestSimpleHandler::registerQuery(arangodb::aql::Query* query) {
  MUTEX_LOCKER(mutexLocker, _queryLock);

  TRI_ASSERT(_query == nullptr);
  _query = query;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregister the currently running query
////////////////////////////////////////////////////////////////////////////////

void RestSimpleHandler::unregisterQuery() {
  MUTEX_LOCKER(mutexLocker, _queryLock);

  _query = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cancel the currently running query
////////////////////////////////////////////////////////////////////////////////

bool RestSimpleHandler::cancelQuery() {
  MUTEX_LOCKER(mutexLocker, _queryLock);

  if (_query != nullptr) {
    _query->killed(true);
    _queryKilled = true;
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the query was canceled
////////////////////////////////////////////////////////////////////////////////

bool RestSimpleHandler::wasCanceled() {
  MUTEX_LOCKER(mutexLocker, _queryLock);
  return _queryKilled;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock RestRemoveByKeys
////////////////////////////////////////////////////////////////////////////////

void RestSimpleHandler::removeByKeys(VPackSlice const& slice) {
  TRI_ASSERT(slice.isObject());
  try {
    std::string collectionName;
    {
      VPackSlice const value = slice.get("collection");

      if (!value.isString()) {
        generateError(rest::ResponseCode::BAD, TRI_ERROR_TYPE_ERROR,
                      "expecting string for <collection>");
        return;
      }

      collectionName = value.copyString();

      if (!collectionName.empty() && collectionName[0] >= '0' &&
          collectionName[0] <= '9') {
        // If we have a numeric name we probably have to translate it.
        CollectionNameResolver resolver(_vocbase);
        collectionName = resolver.getCollectionName(collectionName);
      }
    }

    VPackSlice const keys = slice.get("keys");

    if (!keys.isArray()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_TYPE_ERROR,
                    "expecting array for <keys>");
      return;
    }

    bool waitForSync = false;
    bool silent = true;
    bool returnOld = false;
    {
      VPackSlice const value = slice.get("options");
      if (value.isObject()) {
        VPackSlice wfs = value.get("waitForSync");
        if (wfs.isBool()) {
          waitForSync = wfs.getBool();
        }
        wfs = value.get("silent");
        if (wfs.isBool()) {
          silent = wfs.getBool();
        }
        wfs = value.get("returnOld");
        if (wfs.isBool()) {
          returnOld = wfs.getBool();
        }
      }
    }

    auto bindVars = std::make_shared<VPackBuilder>();
    bindVars->openObject();
    bindVars->add("@collection", VPackValue(collectionName));
    bindVars->add("keys", keys);
    bindVars->close();

    std::string aql(
        "FOR key IN @keys REMOVE key IN @@collection OPTIONS { ignoreErrors: "
        "true, waitForSync: ");
    aql.append(waitForSync ? "true" : "false");
    aql.append(" }");
    if (!silent) {
      if (returnOld) {
        aql.append(" RETURN OLD");
      } else {
        aql.append(" RETURN {_id: OLD._id, _key: OLD._key, _rev: OLD._rev}");
      }
    }

    arangodb::aql::Query query(false, _vocbase, aql.c_str(), aql.size(),
                               bindVars, nullptr, arangodb::aql::PART_MAIN);

    registerQuery(&query);
    auto queryResult = query.execute(_queryRegistry);
    unregisterQuery();

    if (queryResult.code != TRI_ERROR_NO_ERROR) {
      if (queryResult.code == TRI_ERROR_REQUEST_CANCELED ||
          (queryResult.code == TRI_ERROR_QUERY_KILLED && wasCanceled())) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
      }

      THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code, queryResult.details);
    }

    {
      size_t ignored = 0;
      size_t removed = 0;
      if (queryResult.stats != nullptr) {
        VPackSlice stats = queryResult.stats->slice();

        if (!stats.isNone()) {
          TRI_ASSERT(stats.isObject());
          VPackSlice found = stats.get("writesIgnored");
          if (found.isNumber()) {
            ignored = found.getNumericValue<size_t>();
          }

          found = stats.get("writesExecuted");
          if (found.isNumber()) {
            removed = found.getNumericValue<size_t>();
          }
        }
      }

      VPackBuilder result;
      result.add(VPackValue(VPackValueType::Object));
      result.add("removed", VPackValue(removed));
      result.add("ignored", VPackValue(ignored));
      result.add("error", VPackValue(false));
      result.add("code", VPackValue(static_cast<int>(rest::ResponseCode::OK)));
      if (!silent) {
        result.add("old", queryResult.result->slice());
      }
      result.close();

      generateResult(rest::ResponseCode::OK, result.slice(),
                     queryResult.context);
    }
  } catch (...) {
    unregisterQuery();
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock RestLookupByKeys
////////////////////////////////////////////////////////////////////////////////

void RestSimpleHandler::lookupByKeys(VPackSlice const& slice) {
  // TODO needs to generalized
  auto response = dynamic_cast<HttpResponse*>(_response.get());

  if (response == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  try {
    std::string collectionName;
    {
      VPackSlice const value = slice.get("collection");
      if (!value.isString()) {
        generateError(rest::ResponseCode::BAD, TRI_ERROR_TYPE_ERROR,
                      "expecting string for <collection>");
        return;
      }
      collectionName = value.copyString();

      if (!collectionName.empty()) {
        auto const* col = _vocbase->lookupCollection(collectionName);

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
      return;
    }

    auto bindVars = std::make_shared<VPackBuilder>();
    bindVars->openObject();
    bindVars->add("@collection", VPackValue(collectionName));
    VPackBuilder strippedBuilder =
        arangodb::aql::BindParameters::StripCollectionNames(
            keys, collectionName.c_str());

    bindVars->add("keys", strippedBuilder.slice());
    bindVars->close();

    std::string const aql(
        "FOR doc IN @@collection FILTER doc._key IN @keys RETURN doc");

    arangodb::aql::Query query(false, _vocbase, aql.c_str(), aql.size(),
                               bindVars, nullptr, arangodb::aql::PART_MAIN);

    registerQuery(&query);
    auto queryResult = query.execute(_queryRegistry);
    unregisterQuery();

    if (queryResult.code != TRI_ERROR_NO_ERROR) {
      if (queryResult.code == TRI_ERROR_REQUEST_CANCELED ||
          (queryResult.code == TRI_ERROR_QUERY_KILLED && wasCanceled())) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
      }

      THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code, queryResult.details);
    }

    size_t resultSize = 10;
    VPackSlice qResult = queryResult.result->slice();
    if (qResult.isArray()) {
      resultSize = static_cast<size_t>(qResult.length());
    }

    VPackBuffer<uint8_t> resultBuffer;
    VPackBuilder result(resultBuffer);
    {
      VPackObjectBuilder guard(&result);
      resetResponse(rest::ResponseCode::OK);

      // TODO this should be generalized
      response->setContentType(rest::ContentType::JSON);

      if (qResult.isArray()) {
        // This is for internal use of AQL Traverser only.
        // Should not be documented
        VPackSlice const postFilter = slice.get("filter");
        if (postFilter.isArray()) {
          std::vector<arangodb::traverser::TraverserExpression*> expressions;
          arangodb::basics::ScopeGuard guard{[]() -> void {},
                                             [&expressions]() -> void {
                                               for (auto& e : expressions) {
                                                 delete e;
                                               }
                                             }};

          VPackValueLength length = postFilter.length();
          expressions.reserve(static_cast<size_t>(length));

          for (auto const& it : VPackArrayIterator(postFilter)) {
            if (it.isObject()) {
              auto expression =
                  std::make_unique<traverser::TraverserExpression>(it);
              expressions.emplace_back(expression.get());
              expression.release();
            }
          }

          result.add(VPackValue("documents"));
          std::vector<std::string> filteredIds;

          // just needed to build the result
          SingleCollectionTransaction trx(
              StandaloneTransactionContext::Create(_vocbase), collectionName,
              TRI_TRANSACTION_READ);

          result.openArray();
          for (auto const& tmp : VPackArrayIterator(qResult)) {
            if (!tmp.isNone()) {
              bool add = true;
              for (auto& e : expressions) {
                if (!e->isEdgeAccess && !e->matchesCheck(&trx, tmp)) {
                  add = false;
                  std::string _id = trx.extractIdString(tmp);
                  filteredIds.emplace_back(std::move(_id));
                  break;
                }
              }
              if (add) {
                result.add(tmp);
              }
            }
          }
          result.close();

          result.add(VPackValue("filtered"));
          result.openArray();
          for (auto const& it : filteredIds) {
            result.add(VPackValue(it));
          }
          result.close();
        } else {
          result.add(VPackValue("documents"));
          result.add(qResult);
          queryResult.result = nullptr;
        }
      } else {
        result.add(VPackValue("documents"));
        result.add(qResult);
        queryResult.result = nullptr;
      }
      result.add("error", VPackValue(false));
      result.add("code",
                 VPackValue(static_cast<int>(_response->responseCode())));

      // reserve a few bytes per result document by default
      int res = response->reservePayload(32 * resultSize);

      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(res);
      }
    }

    generateResult(rest::ResponseCode::OK, std::move(resultBuffer),
                   queryResult.context);
  } catch (...) {
    unregisterQuery();
    throw;
  }
}
