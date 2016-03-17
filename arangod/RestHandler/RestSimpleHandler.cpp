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
#include "Basics/VelocyPackHelper.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "VocBase/Traverser.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::rest;

RestSimpleHandler::RestSimpleHandler(
    HttpRequest* request,
    std::pair<arangodb::ApplicationV8*, arangodb::aql::QueryRegistry*>* pair)
    : RestVocbaseBaseHandler(request),
      _applicationV8(pair->first),
      _queryRegistry(pair->second),
      _queryLock(),
      _query(nullptr),
      _queryKilled(false) {}

HttpHandler::status_t RestSimpleHandler::execute() {
  // extract the request type
  HttpRequest::HttpRequestType type = _request->requestType();

  if (type == HttpRequest::HTTP_REQUEST_PUT) {
    bool parsingSuccess = true;
    std::shared_ptr<VPackBuilder> parsedBody =
        parseVelocyPackBody(&VPackOptions::Defaults, parsingSuccess);

    if (!parsingSuccess) {
      return status_t(HANDLER_DONE);
    }

    VPackSlice body = parsedBody.get()->slice();

    if (!body.isObject()) {
      generateError(HttpResponse::BAD, TRI_ERROR_TYPE_ERROR,
                    "expecting JSON object body");
      return status_t(HANDLER_DONE);
    }

    char const* prefix = _request->requestPath();

    if (strcmp(prefix, RestVocbaseBaseHandler::SIMPLE_REMOVE_PATH.c_str()) ==
        0) {
      removeByKeys(body);
    } else if (strcmp(prefix,
                      RestVocbaseBaseHandler::SIMPLE_LOOKUP_PATH.c_str()) ==
               0) {
      lookupByKeys(body);
    } else {
      generateError(HttpResponse::BAD, TRI_ERROR_TYPE_ERROR,
                    "unsupported value for <operation>");
    }

    return status_t(HANDLER_DONE);
  }

  generateError(HttpResponse::METHOD_NOT_ALLOWED,
                TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  return status_t(HANDLER_DONE);
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
        generateError(HttpResponse::BAD, TRI_ERROR_TYPE_ERROR,
                      "expecting string for <collection>");
        return;
      }

      collectionName = value.copyString();

      if (!collectionName.empty()) {
        auto const* col =
            TRI_LookupCollectionByNameVocBase(_vocbase, collectionName.c_str());

        if (col != nullptr && collectionName.compare(col->_name) != 0) {
          // user has probably passed in a numeric collection id.
          // translate it into a "real" collection name
          collectionName = std::string(col->_name);
        }
      }
    }

    VPackSlice const keys = slice.get("keys");

    if (!keys.isArray()) {
      generateError(HttpResponse::BAD, TRI_ERROR_TYPE_ERROR,
                    "expecting array for <keys>");
      return;
    }

    bool waitForSync = false;
    {
      VPackSlice const value = slice.get("options");
      if (value.isObject()) {
        VPackSlice wfs = value.get("waitForSync");
        if (wfs.isBool()) {
          waitForSync = wfs.getBool();
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

    arangodb::aql::Query query(
        _applicationV8, false, _vocbase, aql.c_str(), aql.size(),
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
      createResponse(HttpResponse::OK);
      _response->setContentType("application/json; charset=utf-8");

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
      result.add("code", VPackValue(_response->responseCode()));
      result.close();
      VPackSlice s = result.slice();

      arangodb::basics::VPackStringBufferAdapter buffer(
          _response->body().stringBuffer());
      VPackDumper dumper(&buffer);
      dumper.dump(s);
    }
  } catch (arangodb::basics::Exception const& ex) {
    unregisterQuery();
    generateError(HttpResponse::responseCode(ex.code()), ex.code(), ex.what());
  } catch (...) {
    unregisterQuery();
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock RestLookupByKeys
////////////////////////////////////////////////////////////////////////////////

void RestSimpleHandler::lookupByKeys(VPackSlice const& slice) {
  try {
    std::string collectionName;
    {
      VPackSlice const value = slice.get("collection");
      if (!value.isString()) {
        generateError(HttpResponse::BAD, TRI_ERROR_TYPE_ERROR,
                      "expecting string for <collection>");
        return;
      }
      collectionName = value.copyString();

      if (!collectionName.empty()) {
        auto const* col =
            TRI_LookupCollectionByNameVocBase(_vocbase, collectionName.c_str());

        if (col != nullptr && collectionName.compare(col->_name) != 0) {
          // user has probably passed in a numeric collection id.
          // translate it into a "real" collection name
          collectionName = std::string(col->_name);
        }
      }
    }

    VPackSlice const keys = slice.get("keys");

    if (!keys.isArray()) {
      generateError(HttpResponse::BAD, TRI_ERROR_TYPE_ERROR,
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

    arangodb::aql::Query query(
        _applicationV8, false, _vocbase, aql.c_str(), aql.size(),
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

    VPackBuilder result;
    {
      VPackObjectBuilder guard(&result);
      createResponse(HttpResponse::OK);
      _response->setContentType("application/json; charset=utf-8");

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

          expressions.reserve(length);

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

          result.openArray();
          for (auto const& tmp : VPackArrayIterator(qResult)) {
            if (!tmp.isNone()) {
              bool add = true;
              for (auto& e : expressions) {
                if (!e->isEdgeAccess && !e->matchesCheck(tmp)) {
                  add = false;
                  std::string _id =
                      arangodb::basics::VelocyPackHelper::checkAndGetStringValue(
                          tmp, "_id");
                  filteredIds.emplace_back(_id);
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
      result.add("code", VPackValue(_response->responseCode()));

      // reserve 48 bytes per result document by default
      int res = _response->body().reserve(48 * resultSize);

      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(res);
      }
    }

    auto transactionContext = std::make_shared<StandaloneTransactionContext>(_vocbase);
    auto customTypeHandler = transactionContext->orderCustomTypeHandler();
    VPackOptions options = VPackOptions::Defaults; // copy defaults
    options.customTypeHandler = customTypeHandler.get();
     
    arangodb::basics::VPackStringBufferAdapter buffer(
        _response->body().stringBuffer());
    VPackDumper dumper(&buffer, &options);
    dumper.dump(result.slice());
  } catch (arangodb::basics::Exception const& ex) {
    unregisterQuery();
    generateError(HttpResponse::responseCode(ex.code()), ex.code(), ex.what());
  } catch (std::exception const& ex) {
    unregisterQuery();
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    unregisterQuery();
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL);
  }
}
