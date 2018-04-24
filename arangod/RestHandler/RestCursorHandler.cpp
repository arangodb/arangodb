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
#include "Basics/VelocyPackHelper.h"
#include "Transaction/Context.h"
#include "Utils/Cursor.h"
#include "Utils/CursorRepository.h"

#include <velocypack/Iterator.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestCursorHandler::RestCursorHandler(
    GeneralRequest* request, GeneralResponse* response,
    arangodb::aql::QueryRegistry* queryRegistry)
    : RestVocbaseBaseHandler(request, response),
      _queryRegistry(queryRegistry),
      _queryLock(),
      _query(nullptr),
      _hasStarted(false),
      _queryKilled(false),
      _isValidForFinalize(false) {}

RestStatus RestCursorHandler::execute() {
  // extract the sub-request type
  rest::RequestType const type = _request->requestType();

  if (type == rest::RequestType::POST) {
    createQueryCursor();
  } else if (type == rest::RequestType::PUT) {
    modifyQueryCursor();
  } else if (type == rest::RequestType::DELETE_REQ) {
    deleteQueryCursor();
  } else {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  }

  return RestStatus::DONE;
}

bool RestCursorHandler::cancel() {
  RestVocbaseBaseHandler::cancel();
  return cancelQuery();
}

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
  if (!querySlice.isString() || querySlice.getStringLength() == 0) {
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
  VPackSlice opts = options->slice();

  CursorRepository* cursors = _vocbase.cursorRepository();
  TRI_ASSERT(cursors != nullptr);

  bool stream = VelocyPackHelper::getBooleanValue(opts, "stream", false);
  size_t batchSize =
      VelocyPackHelper::getNumericValue<size_t>(opts, "batchSize", 1000);
  double ttl = VelocyPackHelper::getNumericValue<double>(opts, "ttl", 30);
  bool count = VelocyPackHelper::getBooleanValue(opts, "count", false);

  if (stream) {
    if (count) {
      generateError(Result(TRI_ERROR_BAD_PARAMETER, "cannot use 'count' option for a streaming query"));
    } else {
      Cursor* cursor = cursors->createQueryStream(
          querySlice.copyString(), bindVarsBuilder, options, batchSize, ttl);
      TRI_DEFER(cursors->release(cursor));
      generateCursorResult(rest::ResponseCode::CREATED, cursor);
    }
    return;  // done
  }

  VPackValueLength l;
  char const* queryStr = querySlice.getString(l);
  TRI_ASSERT(l > 0);

  aql::Query query(
    false,
    _vocbase,
    arangodb::aql::QueryString(queryStr, static_cast<size_t>(l)),
    bindVarsBuilder,
    options,
    arangodb::aql::PART_MAIN
  );

  registerQuery(&query);
  aql::QueryResult queryResult = query.execute(_queryRegistry);
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

  _response->setContentType(rest::ContentType::JSON);
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

    VPackBuffer<uint8_t> buffer;
    VPackBuilder result(buffer, &options);
    try {
      VPackObjectBuilder b(&result);
      result.add(VPackValue("result"));
      result.add(VPackValue(qResult.begin(), VPackValueType::External));
      result.add("hasMore", VPackValue(false));
      if (VelocyPackHelper::getBooleanValue(opts, "count", false)) {
        result.add("count", VPackValue(n));
      }
      result.add("cached", VPackValue(queryResult.cached));
      if (queryResult.cached || !queryResult.extra) {
        result.add("extra", VPackValue(VPackValueType::Object));
        result.close();
      } else {
        result.add("extra", queryResult.extra->slice());
      }
      result.add(StaticStrings::Error, VPackValue(false));
      result.add(StaticStrings::Code, VPackValue(static_cast<int>(ResponseCode::CREATED)));
    } catch (...) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
    generateResult(rest::ResponseCode::CREATED, std::move(buffer),
                   queryResult.context);
  } else {
    // result is bigger than batchSize, and a cursor will be created
    TRI_ASSERT(queryResult.result.get() != nullptr);
    // steal the query result, cursor will take over the ownership
    Cursor* cursor = cursors->createFromQueryResult(std::move(queryResult),
                                                    batchSize, ttl, count);

    TRI_DEFER(cursors->release(cursor));
    generateCursorResult(rest::ResponseCode::CREATED, cursor);
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
  VPackObjectBuilder obj(&options);

  bool hasCache = false;
  VPackSlice opts = slice.get("options");
  bool isStream = false;
  if (opts.isObject()) {
    isStream = VelocyPackHelper::getBooleanValue(opts, "stream", false);
    for (auto const& it : VPackObjectIterator(opts)) {
      if (!it.key.isString() || it.value.isNone()) {
        continue;
      }
      std::string keyName = it.key.copyString();
      if (keyName == "count" || keyName == "batchSize" || keyName == "ttl" ||
          (isStream && keyName == "fullCount")) {
        continue;  // filter out top-level keys
      } else if (keyName == "cache") {
        hasCache = true;  // don't honor if appears below
      }
      options.add(keyName, it.value);
    }
  }

  if (!isStream) {  // ignore cache & count for streaming queries
    bool val = VelocyPackHelper::getBooleanValue(slice, "count", false);
    options.add("count", VPackValue(val));
    if (!hasCache && slice.hasKey("cache")) {
      val = VelocyPackHelper::getBooleanValue(slice, "cache", false);
      options.add("cache", VPackValue(val));
    }
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

  VPackSlice memoryLimit = slice.get("memoryLimit");
  if (memoryLimit.isNumber()) {
    options.add("memoryLimit", memoryLimit);
  }

  VPackSlice ttl = slice.get("ttl");
  options.add("ttl", VPackValue(ttl.isNumber() ? ttl.getNumber<double>() : 30));

  return options;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief append the contents of the cursor into the response body
//////////////////////////////////////////////////////////////////////////////

void RestCursorHandler::generateCursorResult(rest::ResponseCode code,
                                             arangodb::Cursor* cursor) {
  // dump might delete the cursor
  std::shared_ptr<transaction::Context> ctx = cursor->context();

  VPackBuffer<uint8_t> buffer;
  VPackBuilder result(buffer);
  result.openObject();
  result.add(StaticStrings::Error, VPackValue(false));
  result.add(StaticStrings::Code, VPackValue(static_cast<int>(code)));
  Result r = cursor->dump(result);
  result.close();

  if (r.ok()) {
    _response->setContentType(rest::ContentType::JSON);
    generateResult(code, std::move(buffer), std::move(ctx));
  } else {
    generateError(r);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_post_api_cursor
////////////////////////////////////////////////////////////////////////////////

void RestCursorHandler::createQueryCursor() {
  if (_request->payload().isEmptyObject()) {
    generateError(rest::ResponseCode::BAD, 600);
    return;
  }

  std::vector<std::string> const& suffixes = _request->suffixes();

  if (!suffixes.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting POST /_api/cursor");
    return;
  }

  try {
    bool parseSuccess = false;
    VPackSlice body = this->parseVPackBody(parseSuccess);

    if (!parseSuccess) {
      // error message generated in parseVPackBody
      return;
    }
    
    // tell RestCursorHandler::finalizeExecute that the request
    // could be parsed successfully and that it may look at it
    _isValidForFinalize = true;

    processQuery(body);
  } catch (...) {
    unregisterQuery();
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_post_api_cursor_identifier
////////////////////////////////////////////////////////////////////////////////

void RestCursorHandler::modifyQueryCursor() {
  std::vector<std::string> const& suffixes = _request->suffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting PUT /_api/cursor/<cursor-id>");
    return;
  }

  std::string const& id = suffixes[0];

  auto cursors = _vocbase.cursorRepository();
  TRI_ASSERT(cursors != nullptr);

  auto cursorId = static_cast<arangodb::CursorId>(
      arangodb::basics::StringUtils::uint64(id));
  bool busy;
  Cursor* cursor = cursors->find(cursorId, Cursor::CURSOR_VPACK, busy);

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

  TRI_DEFER(cursors->release(cursor));
  generateCursorResult(rest::ResponseCode::OK, cursor);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_post_api_cursor_delete
////////////////////////////////////////////////////////////////////////////////

void RestCursorHandler::deleteQueryCursor() {
  std::vector<std::string> const& suffixes = _request->suffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting DELETE /_api/cursor/<cursor-id>");
    return;
  }

  std::string const& id = suffixes[0];

  auto cursors = _vocbase.cursorRepository();
  TRI_ASSERT(cursors != nullptr);

  auto cursorId = static_cast<arangodb::CursorId>(
      arangodb::basics::StringUtils::uint64(id));
  bool found = cursors->remove(cursorId, Cursor::CURSOR_VPACK);

  if (!found) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_CURSOR_NOT_FOUND);
    return;
  }

  VPackBuilder builder;
  builder.openObject();
  builder.add("id", VPackValue(id));
  builder.add(StaticStrings::Error, VPackValue(false));
  builder.add(StaticStrings::Code,
              VPackValue(static_cast<int>(rest::ResponseCode::ACCEPTED)));
  builder.close();

  generateResult(rest::ResponseCode::ACCEPTED, builder.slice());
}
