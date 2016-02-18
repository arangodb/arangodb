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

#include "RestCursorHandler.h"
#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "Basics/Exceptions.h"
#include "Basics/json.h"
#include "Basics/MutexLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Utils/Cursor.h"
#include "Utils/CursorRepository.h"
#include "V8Server/ApplicationV8.h"

#include <velocypack/Dumper.h>
#include <velocypack/Iterator.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::rest;

RestCursorHandler::RestCursorHandler(
    HttpRequest* request,
    std::pair<arangodb::ApplicationV8*, arangodb::aql::QueryRegistry*>* pair)
    : RestVocbaseBaseHandler(request),
      _applicationV8(pair->first),
      _queryRegistry(pair->second),
      _queryLock(),
      _query(nullptr),
      _queryKilled(false) {}

HttpHandler::status_t RestCursorHandler::execute() {
  // extract the sub-request type
  HttpRequest::HttpRequestType type = _request->requestType();

  if (type == HttpRequest::HTTP_REQUEST_POST) {
    createCursor();
    return status_t(HANDLER_DONE);
  }

  if (type == HttpRequest::HTTP_REQUEST_PUT) {
    modifyCursor();
    return status_t(HANDLER_DONE);
  }

  if (type == HttpRequest::HTTP_REQUEST_DELETE) {
    deleteCursor();
    return status_t(HANDLER_DONE);
  }

  generateError(HttpResponse::METHOD_NOT_ALLOWED,
                TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  return status_t(HANDLER_DONE);
}

bool RestCursorHandler::cancel() { return cancelQuery(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief processes the query and returns the results/cursor
/// this method is also used by derived classes
////////////////////////////////////////////////////////////////////////////////

void RestCursorHandler::processQuery(VPackSlice const& slice) {
  if (!slice.isObject()) {
    generateError(HttpResponse::BAD, TRI_ERROR_QUERY_EMPTY);
    return;
  }
  VPackSlice const querySlice = slice.get("query");
  if (!querySlice.isString()) {
    generateError(HttpResponse::BAD, TRI_ERROR_QUERY_EMPTY);
    return;
  }

  VPackSlice const bindVars = slice.get("bindVars");
  if (!bindVars.isNone()) {
    if (!bindVars.isObject() && !bindVars.isNull()) {
      generateError(HttpResponse::BAD, TRI_ERROR_TYPE_ERROR,
                    "expecting object for <bindVars>");
      return;
    }
  }

  VPackBuilder optionsBuilder = buildOptions(slice);
  VPackSlice options = optionsBuilder.slice();
  VPackValueLength l;
  char const* queryString = querySlice.getString(l);

  arangodb::aql::Query query(
      _applicationV8, false, _vocbase, queryString, static_cast<size_t>(l),
      (!bindVars.isNone()
           ? arangodb::basics::VelocyPackHelper::velocyPackToJson(bindVars)
           : nullptr),
      arangodb::basics::VelocyPackHelper::velocyPackToJson(options),
      arangodb::aql::PART_MAIN);

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

  TRI_ASSERT(TRI_IsArrayJson(queryResult.json));

  {
    createResponse(HttpResponse::CREATED);
    _response->setContentType("application/json; charset=utf-8");

    std::shared_ptr<VPackBuilder> extra = buildExtra(queryResult);

    size_t batchSize =
        arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(
            options, "batchSize", 1000);
    size_t const n = TRI_LengthArrayJson(queryResult.json);

    if (n <= batchSize) {
      // result is smaller than batchSize and will be returned directly. no need
      // to create a cursor

      VPackBuilder result;
      try {
        VPackObjectBuilder b(&result);
        result.add(VPackValue("result"));
        int res = arangodb::basics::JsonHelper::toVelocyPack(queryResult.json, result);
        if (res != TRI_ERROR_NO_ERROR) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
        }
        result.add("hasMore", VPackValue(false));
        if (arangodb::basics::VelocyPackHelper::getBooleanValue(options, "count",
                                                                false)) {
          result.add("count", VPackValue(n));
        }
        result.add("cached", VPackValue(queryResult.cached));
        if (queryResult.cached) {
          result.add("extra", VPackValue(VPackValueType::Object));
          result.close();
        } else {
          result.add("extra", extra->slice());
        }
        result.add("error", VPackValue(false));
        result.add("code", VPackValue(_response->responseCode()));
      } catch (...) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }

      arangodb::basics::VPackStringBufferAdapter bufferAdapter(
          _response->body().stringBuffer());
      VPackDumper dumper(&bufferAdapter);
      dumper.dump(result.slice());
      return;
    }

    // result is bigger than batchSize, and a cursor will be created
    auto cursors =
        static_cast<arangodb::CursorRepository*>(_vocbase->_cursorRepository);
    TRI_ASSERT(cursors != nullptr);

    double ttl = arangodb::basics::VelocyPackHelper::getNumericValue<double>(
        options, "ttl", 30);
    bool count = arangodb::basics::VelocyPackHelper::getBooleanValue(
        options, "count", false);

    // steal the query JSON, cursor will take over the ownership
    auto j = queryResult.json;
    std::shared_ptr<VPackBuilder> builder = arangodb::basics::JsonHelper::toVelocyPack(j);
    {
      // Temporarily validate that transformation actually worked.
      // Can be removed again as soon as queryResult returns VPack
      VPackSlice validate = builder->slice();
      if (validate.isNone() && j != nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }
    }
    arangodb::JsonCursor* cursor = cursors->createFromVelocyPack(
        builder, batchSize, extra, ttl, count, queryResult.cached);

    try {
      _response->body().appendChar('{');
      cursor->dump(_response->body());
      _response->body().appendText(",\"error\":false,\"code\":");
      _response->body().appendInteger(
          static_cast<uint32_t>(_response->responseCode()));
      _response->body().appendChar('}');

      cursors->release(cursor);
    } catch (...) {
      cursors->release(cursor);
      throw;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register the currently running query
////////////////////////////////////////////////////////////////////////////////

void RestCursorHandler::registerQuery(arangodb::aql::Query* query) {
  MUTEX_LOCKER(mutexLocker, _queryLock);

  TRI_ASSERT(_query == nullptr);
  _query = query;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregister the currently running query
////////////////////////////////////////////////////////////////////////////////

void RestCursorHandler::unregisterQuery() {
  MUTEX_LOCKER(mutexLocker, _queryLock);

  _query = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cancel the currently running query
////////////////////////////////////////////////////////////////////////////////

bool RestCursorHandler::cancelQuery() {
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

bool RestCursorHandler::wasCanceled() {
  MUTEX_LOCKER(mutexLocker, _queryLock);
  return _queryKilled;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief build options for the query as JSON
////////////////////////////////////////////////////////////////////////////////

VPackBuilder RestCursorHandler::buildOptions(VPackSlice const& slice) const {
  VPackBuilder options;
  options.openObject();

  VPackSlice count = slice.get("count");
  if (count.isBool()) {
    options.add("count", count);
  } else {
    options.add("count", VPackValue(false));
  }

  VPackSlice batchSize = slice.get("batchSize");
  if (batchSize.isNumber()) {
    if ((batchSize.isDouble() && batchSize.getDouble() == 0.0) ||
        (batchSize.isInteger() && batchSize.getUInt() == 0)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_TYPE_ERROR, "expecting non-zero value for <batchSize>");
    }
    options.add("batchSize", batchSize);
  } else {
    options.add("batchSize", VPackValue(1000));
  }

  bool hasCache = false;
  VPackSlice cache = slice.get("cache");
  if (cache.isBool()) {
    hasCache = true;
    options.add("cache", cache);
  }

  VPackSlice opts = slice.get("options");
  if (opts.isObject()) {
    for (auto const& it : VPackObjectIterator(opts)) {
      if (!it.key.isString() || it.value.isNone()) {
        continue;
      }
      std::string keyName = it.key.copyString();

      if (keyName != "count" && keyName != "batchSize") {
        if (keyName == "cache" && hasCache) {
          continue;
        }
        options.add(keyName, it.value);
      }
    }
  }

  VPackSlice ttl = slice.get("ttl");
  if (ttl.isNumber()) {
    options.add("ttl", ttl);
  } else {
    options.add("ttl", VPackValue(30));
  }
  options.close();

  return options;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the "extra" attribute values from the result.
/// note that the "extra" object will take ownership from the result for
/// several values
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VPackBuilder> RestCursorHandler::buildExtra(
    arangodb::aql::QueryResult& queryResult) const {
  // build "extra" attribute
  auto extra = std::make_shared<VPackBuilder>();
  try {
    VPackObjectBuilder b(extra.get());
    if (queryResult.stats != nullptr) {
      VPackSlice stats = queryResult.stats->slice();
      if (!stats.isNone()) {
        extra->add("stats", stats);
      }
    }
    if (queryResult.profile != nullptr) {
      extra->add(VPackValue("profile"));
      int res = arangodb::basics::JsonHelper::toVelocyPack(queryResult.profile, *extra);
      if (res != TRI_ERROR_NO_ERROR) {
        return nullptr;
      }
      queryResult.profile = nullptr;
    }
    if (queryResult.warnings == nullptr) {
      extra->add("warnings", VPackValue(VPackValueType::Array));
      extra->close();
    } else {
      extra->add(VPackValue("warnings"));
      int res = arangodb::basics::JsonHelper::toVelocyPack(queryResult.warnings, *extra);
      if (res != TRI_ERROR_NO_ERROR) {
        return nullptr;
      }
      queryResult.warnings = nullptr;
    }
  } catch (...) {
    return nullptr;
  }
  return extra;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_post_api_cursor
////////////////////////////////////////////////////////////////////////////////

void RestCursorHandler::createCursor() {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 0) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting POST /_api/cursor");
    return;
  }

  try {
    bool parseSuccess = true;
    VPackOptions options;
    std::shared_ptr<VPackBuilder> parsedBody =
        parseVelocyPackBody(&options, parseSuccess);

    if (!parseSuccess) {
      return;
    }
    VPackSlice body = parsedBody.get()->slice();

    processQuery(body);
  } catch (arangodb::basics::Exception const& ex) {
    unregisterQuery();
    generateError(HttpResponse::responseCode(ex.code()), ex.code(), ex.what());
  } catch (std::bad_alloc const&) {
    unregisterQuery();
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
  } catch (std::exception const& ex) {
    unregisterQuery();
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    unregisterQuery();
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_post_api_cursor_identifier
////////////////////////////////////////////////////////////////////////////////

void RestCursorHandler::modifyCursor() {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 1) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting PUT /_api/cursor/<cursor-id>");
    return;
  }

  std::string const& id = suffix[0];

  auto cursors =
      static_cast<arangodb::CursorRepository*>(_vocbase->_cursorRepository);
  TRI_ASSERT(cursors != nullptr);

  auto cursorId = static_cast<arangodb::CursorId>(
      arangodb::basics::StringUtils::uint64(id));
  bool busy;
  auto cursor = cursors->find(cursorId, busy);

  if (cursor == nullptr) {
    if (busy) {
      generateError(HttpResponse::responseCode(TRI_ERROR_CURSOR_BUSY),
                    TRI_ERROR_CURSOR_BUSY);
    } else {
      generateError(HttpResponse::responseCode(TRI_ERROR_CURSOR_NOT_FOUND),
                    TRI_ERROR_CURSOR_NOT_FOUND);
    }
    return;
  }

  try {
    createResponse(HttpResponse::OK);
    _response->setContentType("application/json; charset=utf-8");

    _response->body().appendChar('{');
    cursor->dump(_response->body());
    _response->body().appendText(",\"error\":false,\"code\":");
    _response->body().appendInteger(
        static_cast<uint32_t>(_response->responseCode()));
    _response->body().appendChar('}');

    cursors->release(cursor);
  } catch (arangodb::basics::Exception const& ex) {
    cursors->release(cursor);

    generateError(HttpResponse::responseCode(ex.code()), ex.code(), ex.what());
  } catch (...) {
    cursors->release(cursor);

    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_post_api_cursor_delete
////////////////////////////////////////////////////////////////////////////////

void RestCursorHandler::deleteCursor() {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 1) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting DELETE /_api/cursor/<cursor-id>");
    return;
  }

  std::string const& id = suffix[0];

  auto cursors =
      static_cast<arangodb::CursorRepository*>(_vocbase->_cursorRepository);
  TRI_ASSERT(cursors != nullptr);

  auto cursorId = static_cast<arangodb::CursorId>(
      arangodb::basics::StringUtils::uint64(id));
  bool found = cursors->remove(cursorId);

  if (!found) {
    generateError(HttpResponse::NOT_FOUND, TRI_ERROR_CURSOR_NOT_FOUND);
    return;
  }

  createResponse(HttpResponse::ACCEPTED);
  _response->setContentType("application/json; charset=utf-8");

  arangodb::basics::Json json(arangodb::basics::Json::Object);
  json.set("id", arangodb::basics::Json(id));  // id as a string!
  json.set("error", arangodb::basics::Json(false));
  json.set("code", arangodb::basics::Json(
                       static_cast<double>(_response->responseCode())));

  json.dump(_response->body());
}
