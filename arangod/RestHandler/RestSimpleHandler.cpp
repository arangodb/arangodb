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

using namespace triagens::arango;
using namespace triagens::rest;


////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestSimpleHandler::RestSimpleHandler(
    HttpRequest* request, std::pair<triagens::arango::ApplicationV8*,
                                    triagens::aql::QueryRegistry*>* pair)
    : RestVocbaseBaseHandler(request),
      _applicationV8(pair->first),
      _queryRegistry(pair->second),
      _queryLock(),
      _query(nullptr),
      _queryKilled(false) {}


////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_t RestSimpleHandler::execute() {
  // extract the request type
  HttpRequest::HttpRequestType type = _request->requestType();

  if (type == HttpRequest::HTTP_REQUEST_PUT) {
    bool parsingSuccess = true;
    VPackOptions options;
    std::shared_ptr<VPackBuilder> parsedBody =
        parseVelocyPackBody(&options, parsingSuccess);

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

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestSimpleHandler::cancel() { return cancelQuery(); }


////////////////////////////////////////////////////////////////////////////////
/// @brief register the currently running query
////////////////////////////////////////////////////////////////////////////////

void RestSimpleHandler::registerQuery(triagens::aql::Query* query) {
  MUTEX_LOCKER(_queryLock);

  TRI_ASSERT(_query == nullptr);
  _query = query;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregister the currently running query
////////////////////////////////////////////////////////////////////////////////

void RestSimpleHandler::unregisterQuery() {
  MUTEX_LOCKER(_queryLock);

  _query = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cancel the currently running query
////////////////////////////////////////////////////////////////////////////////

bool RestSimpleHandler::cancelQuery() {
  MUTEX_LOCKER(_queryLock);

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
  MUTEX_LOCKER(_queryLock);
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

    VPackBuilder bindVars;
    bindVars.openObject();
    bindVars.add("@collection", VPackValue(collectionName));
    bindVars.add("keys", keys);
    bindVars.close();
    VPackSlice varsSlice = bindVars.slice();

    std::string aql(
        "FOR key IN @keys REMOVE key IN @@collection OPTIONS { ignoreErrors: "
        "true, waitForSync: ");
    aql.append(waitForSync ? "true" : "false");
    aql.append(" }");

    triagens::aql::Query query(
        _applicationV8, false, _vocbase, aql.c_str(), aql.size(),
        triagens::basics::VelocyPackHelper::velocyPackToJson(varsSlice),
        nullptr, triagens::aql::PART_MAIN);

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
      VPackSlice stats = queryResult.stats.slice();

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

      VPackBuilder result;
      result.add(VPackValue(VPackValueType::Object));
      result.add("removed", VPackValue(removed));
      result.add("ignored", VPackValue(ignored));
      result.add("error", VPackValue(false));
      result.add("code", VPackValue(_response->responseCode()));
      result.close();
      VPackSlice s = result.slice();

      triagens::basics::VPackStringBufferAdapter buffer(
          _response->body().stringBuffer());
      VPackDumper dumper(&buffer);
      dumper.dump(s);
    }
  } catch (triagens::basics::Exception const& ex) {
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

    VPackBuilder bindVars;
    bindVars.add(VPackValue(VPackValueType::Object));
    bindVars.add("@collection", VPackValue(collectionName));
    VPackBuilder strippedBuilder =
        triagens::aql::BindParameters::StripCollectionNames(
            keys, collectionName.c_str());
    VPackSlice stripped = strippedBuilder.slice();

    bindVars.add("keys", stripped);
    bindVars.close();
    VPackSlice varsSlice = bindVars.slice();

    std::string const aql(
        "FOR doc IN @@collection FILTER doc._key IN @keys RETURN doc");

    triagens::aql::Query query(
        _applicationV8, false, _vocbase, aql.c_str(), aql.size(),
        triagens::basics::VelocyPackHelper::velocyPackToJson(varsSlice),
        nullptr, triagens::aql::PART_MAIN);

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
    if (TRI_IsArrayJson(queryResult.json)) {
      resultSize = TRI_LengthArrayJson(queryResult.json);
    }

    {
      createResponse(HttpResponse::OK);
      _response->setContentType("application/json; charset=utf-8");

      triagens::basics::Json result(triagens::basics::Json::Object, 3);

      if (TRI_IsArrayJson(queryResult.json)) {
        size_t const n = TRI_LengthArrayJson(queryResult.json);

        // This is for internal use of AQL Traverser only.
        // Should not be documented
        VPackSlice const postFilter = slice.get("filter");
        if (postFilter.isArray()) {
          std::vector<triagens::arango::traverser::TraverserExpression*>
              expressions;
          triagens::basics::ScopeGuard guard{[]() -> void {},
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

          triagens::basics::Json filteredDocuments(
              triagens::basics::Json::Array, n);
          triagens::basics::Json filteredIds(triagens::basics::Json::Array);

          for (size_t i = 0; i < n; ++i) {
            TRI_json_t const* tmp = TRI_LookupArrayJson(queryResult.json, i);
            if (tmp != nullptr) {
              bool add = true;
              for (auto& e : expressions) {
                if (!e->isEdgeAccess && !e->matchesCheck(tmp)) {
                  add = false;
                  try {
                    std::string _id =
                        triagens::basics::JsonHelper::checkAndGetStringValue(
                            tmp, "_id");
                    triagens::basics::Json tmp(_id);
                    filteredIds.add(tmp.steal());
                  } catch (...) {
                    // This should never occur.
                  }
                  break;
                }
              }
              if (add) {
                filteredDocuments.add(TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, tmp));
              }
            }
          }

          result.set("documents", filteredDocuments);
          result.set("filtered", filteredIds);
        } else {
          result.set("documents", triagens::basics::Json(
                                      TRI_UNKNOWN_MEM_ZONE, queryResult.json,
                                      triagens::basics::Json::AUTOFREE));
          queryResult.json = nullptr;
        }
      } else {
        result.set("documents", triagens::basics::Json(
                                    TRI_UNKNOWN_MEM_ZONE, queryResult.json,
                                    triagens::basics::Json::AUTOFREE));
        queryResult.json = nullptr;
      }

      result.set("error", triagens::basics::Json(false));
      result.set("code", triagens::basics::Json(
                             static_cast<double>(_response->responseCode())));

      // reserve 48 bytes per result document by default
      int res = _response->body().reserve(48 * resultSize);

      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(res);
      }

      result.dump(_response->body());
    }
  } catch (triagens::basics::Exception const& ex) {
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


