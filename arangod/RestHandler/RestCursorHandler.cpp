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
#include "Basics/MutexLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackDumper.h"
#include "Basics/VelocyPackHelper.h"
#include "Utils/Cursor.h"
#include "Utils/CursorRepository.h"
#include "Utils/TransactionContext.h"

#include <velocypack/Iterator.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::rest;

RestCursorHandler::RestCursorHandler(
    GeneralRequest* request, GeneralResponse* response,
    arangodb::aql::QueryRegistry* queryRegistry)
    : RestVocbaseBaseHandler(request, response),
      _queryRegistry(queryRegistry),
      _queryLock(),
      _query(nullptr),
      _hasStarted(false),
      _queryKilled(false) {}

RestHandler::status RestCursorHandler::execute() {
  // extract the sub-request type
  auto const type = _request->requestType();

  if (type == rest::RequestType::POST) {
    createCursor();
    return status::DONE;
  }

  if (type == rest::RequestType::PUT) {
    modifyCursor();
    return status::DONE;
  }

  if (type == rest::RequestType::DELETE_REQ) {
    deleteCursor();
    return status::DONE;
  }

  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  return status::DONE;
}

bool RestCursorHandler::cancel() { return cancelQuery(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief processes the query and returns the results/cursor
/// this method is also used by derived classes
////////////////////////////////////////////////////////////////////////////////

void RestCursorHandler::processQuery(VPackSlice const& slice) {
  if (!slice.isObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_QUERY_EMPTY);
    return;
  }
  VPackSlice const querySlice = slice.get("query");
  if (!querySlice.isString()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_QUERY_EMPTY);
    return;
  }

  VPackSlice const bindVars = slice.get("bindVars");
  if (!bindVars.isNone()) {
    if (!bindVars.isObject() && !bindVars.isNull()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_TYPE_ERROR,
                    "expecting object for <bindVars>");
      return;
    }
  }

  std::shared_ptr<VPackBuilder> bindVarsBuilder;
  if (!bindVars.isNone()) {
    bindVarsBuilder.reset(new VPackBuilder);
    bindVarsBuilder->add(bindVars);
  }

  auto options = std::make_shared<VPackBuilder>(buildOptions(slice));
  VPackValueLength l;
  char const* queryString = querySlice.getString(l);

  arangodb::aql::Query query(false, _vocbase, queryString,
                             static_cast<size_t>(l), bindVarsBuilder, options,
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

  VPackSlice qResult = queryResult.result->slice();

  if (qResult.isNone()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  TRI_ASSERT(qResult.isArray());

  {
    resetResponse(rest::ResponseCode::CREATED);
    _response->setContentType(rest::ContentType::JSON);
    std::shared_ptr<VPackBuilder> extra = buildExtra(queryResult);
    VPackSlice opts = options->slice();

    size_t batchSize =
        arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(
            opts, "batchSize", 1000);
    size_t const n = static_cast<size_t>(qResult.length());

    if (n <= batchSize) {
      // result is smaller than batchSize and will be returned directly. no need
      // to create a cursor

      VPackOptions options = VPackOptions::Defaults;
      options.buildUnindexedArrays = true;
      options.buildUnindexedObjects = true;

      // conservatively allocate a few bytes per value to be returned
      int res;
      if (n >= 10000) {
        res = _response->reservePayload(128 * 1024);
      } else if (n >= 1000) {
        res = _response->reservePayload(64 * 1024);
      } else {
        res = _response->reservePayload(n * 48);
      }

      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(res);
      }

      VPackBuilder result(&options);
      try {
        VPackObjectBuilder b(&result);
        result.add(VPackValue("result"));
        result.add(VPackValue(qResult.begin(), VPackValueType::External));
        result.add("hasMore", VPackValue(false));
        if (arangodb::basics::VelocyPackHelper::getBooleanValue(opts, "count",
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
        result.add("code", VPackValue((int)_response->responseCode()));
      } catch (...) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }
      generateResult(rest::ResponseCode::CREATED, result.slice(),
                     queryResult.context);
      return;
    }

    // result is bigger than batchSize, and a cursor will be created
    auto cursors = _vocbase->cursorRepository();
    TRI_ASSERT(cursors != nullptr);

    double ttl = arangodb::basics::VelocyPackHelper::getNumericValue<double>(
        opts, "ttl", 30);
    bool count = arangodb::basics::VelocyPackHelper::getBooleanValue(
        opts, "count", false);

    TRI_ASSERT(queryResult.result.get() != nullptr);

    // steal the query result, cursor will take over the ownership
    arangodb::VelocyPackCursor* cursor = cursors->createFromQueryResult(
        std::move(queryResult), batchSize, extra, ttl, count);

    try {
      VPackBuilder result;
      result.openObject();
      result.add("error", VPackValue(false));
      result.add("code", VPackValue((int)_response->responseCode()));
      cursor->dump(result);
      result.close();

      _response->setContentType(rest::ContentType::JSON);
      generateResult(_response->responseCode(), result.slice(),
                     cursor->result()->context);

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

  if (_queryKilled) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
  }

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
    _hasStarted = true;
    return true;
  } else if (!_hasStarted) {
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
      extra->add(queryResult.profile->slice());
      queryResult.profile = nullptr;
    }
    if (queryResult.warnings == nullptr) {
      extra->add("warnings", VPackValue(VPackValueType::Array));
      extra->close();
    } else {
      extra->add(VPackValue("warnings"));
      extra->add(queryResult.warnings->slice());
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
  if (_request->payload().isEmptyObject()) {
    generateError(rest::ResponseCode::BAD, 600);
    return;
  }

  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 0) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting POST /_api/cursor");
    return;
  }

  try {
    bool parseSuccess = true;
    std::shared_ptr<VPackBuilder> parsedBody =
        parseVelocyPackBody(&VPackOptions::Defaults, parseSuccess);

    if (!parseSuccess) {
      return;
    }
    VPackSlice body = parsedBody.get()->slice();

    processQuery(body);
  } catch (...) {
    unregisterQuery();
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_post_api_cursor_identifier
////////////////////////////////////////////////////////////////////////////////

void RestCursorHandler::modifyCursor() {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting PUT /_api/cursor/<cursor-id>");
    return;
  }

  std::string const& id = suffix[0];

  auto cursors = _vocbase->cursorRepository();
  TRI_ASSERT(cursors != nullptr);

  auto cursorId = static_cast<arangodb::CursorId>(
      arangodb::basics::StringUtils::uint64(id));
  bool busy;
  auto cursor = cursors->find(cursorId, busy);

  if (cursor == nullptr) {
    if (busy) {
      generateError(GeneralResponse::responseCode(TRI_ERROR_CURSOR_BUSY),
                    TRI_ERROR_CURSOR_BUSY);
    } else {
      generateError(GeneralResponse::responseCode(TRI_ERROR_CURSOR_NOT_FOUND),
                    TRI_ERROR_CURSOR_NOT_FOUND);
    }
    return;
  }

  try {
    VPackBuilder builder;
    builder.openObject();
    builder.add("error", VPackValue(false));
    builder.add("code", VPackValue(static_cast<int>(rest::ResponseCode::OK)));
    cursor->dump(builder);
    builder.close();

    _response->setContentType(rest::ContentType::JSON);
    generateResult(rest::ResponseCode::OK, builder.slice());

    cursors->release(cursor);
  } catch (...) {
    cursors->release(cursor);
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_post_api_cursor_delete
////////////////////////////////////////////////////////////////////////////////

void RestCursorHandler::deleteCursor() {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting DELETE /_api/cursor/<cursor-id>");
    return;
  }

  std::string const& id = suffix[0];

  auto cursors = _vocbase->cursorRepository();
  TRI_ASSERT(cursors != nullptr);

  auto cursorId = static_cast<arangodb::CursorId>(
      arangodb::basics::StringUtils::uint64(id));
  bool found = cursors->remove(cursorId);

  if (!found) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_CURSOR_NOT_FOUND);
    return;
  }

  VPackBuilder builder;
  builder.openObject();
  builder.add("id", VPackValue(id));
  builder.add("error", VPackValue(false));
  builder.add("code",
              VPackValue(static_cast<int>(rest::ResponseCode::ACCEPTED)));
  builder.close();

  generateResult(rest::ResponseCode::ACCEPTED, builder.slice());
}
