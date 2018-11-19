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
#include "Cluster/ServerState.h"
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
      _query(nullptr),
      _queryResult(),
      _queryRegistry(queryRegistry),
      _leasedCursor(nullptr),
      _hasStarted(false),
      _queryKilled(false),
      _isValidForFinalize(false) {}

RestCursorHandler::~RestCursorHandler() {
  if (_leasedCursor) {
    auto cursors = _vocbase.cursorRepository();
    TRI_ASSERT(cursors != nullptr);
    cursors->release(_leasedCursor);
  }
}

RestStatus RestCursorHandler::execute() {
  // extract the sub-request type
  rest::RequestType const type = _request->requestType();

  if (type == rest::RequestType::POST) {
    return createQueryCursor();
  } else if (type == rest::RequestType::PUT) {
    return modifyQueryCursor();
  } else if (type == rest::RequestType::DELETE_REQ) {
    return deleteQueryCursor();
  }
  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  return RestStatus::DONE;
}

RestStatus RestCursorHandler::continueExecute() {
  // extract the sub-request type
  rest::RequestType const type = _request->requestType();
  
  if (_query != nullptr) { // non-stream query
    if (type == rest::RequestType::POST ||
        type == rest::RequestType::PUT) {
      return processQuery();
    }
  } else if (_leasedCursor) { // stream cursor query
    Cursor* cs = _leasedCursor;
    _leasedCursor = nullptr;
    if (type == rest::RequestType::POST) {
      return generateCursorResult(rest::ResponseCode::CREATED, cs);
    } else if (type == rest::RequestType::PUT) {
      if (_request->requestPath() == SIMPLE_QUERY_ALL_PATH) {
        // RestSimpleQueryHandler::allDocuments uses PUT for cursor creation
        return generateCursorResult(ResponseCode::CREATED, cs);
      }
      return generateCursorResult(ResponseCode::OK, cs);
    }
  }

  // Other parts of the query cannot be paused
  TRI_ASSERT(false);
  return RestStatus::DONE;
}

bool RestCursorHandler::cancel() {
  RestVocbaseBaseHandler::cancel();
  return cancelQuery();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register the query either as streaming cursor or in _query
/// the query is not executed here.
/// this method is also used by derived classes
///
/// return If true, we need to continue processing,
///        If false we are done (error or stream)
////////////////////////////////////////////////////////////////////////////////

RestStatus RestCursorHandler::registerQueryOrCursor(VPackSlice const& slice) {
  TRI_ASSERT(_query == nullptr);

  if (!slice.isObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_QUERY_EMPTY);
    return RestStatus::DONE;
  }
  VPackSlice const querySlice = slice.get("query");
  if (!querySlice.isString() || querySlice.getStringLength() == 0) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_QUERY_EMPTY);
    return RestStatus::DONE;
  }

  VPackSlice const bindVars = slice.get("bindVars");
  if (!bindVars.isNone()) {
    if (!bindVars.isObject() && !bindVars.isNull()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_TYPE_ERROR,
                    "expecting object for <bindVars>");
      return RestStatus::DONE;
    }
  }

  std::shared_ptr<VPackBuilder> bindVarsBuilder;
  if (!bindVars.isNone()) {
    bindVarsBuilder.reset(new VPackBuilder);
    bindVarsBuilder->add(bindVars);
  }

  TRI_ASSERT(_options == nullptr);
  buildOptions(slice);
  TRI_ASSERT(_options != nullptr);
  VPackSlice opts = _options->slice();

  bool stream = VelocyPackHelper::getBooleanValue(opts, "stream", false);
  size_t batchSize =
      VelocyPackHelper::getNumericValue<size_t>(opts, "batchSize", 1000);
  double ttl = VelocyPackHelper::getNumericValue<double>(opts, "ttl", 30);
  bool count = VelocyPackHelper::getBooleanValue(opts, "count", false);

  if (stream) {
    if (count) {
      generateError(Result(TRI_ERROR_BAD_PARAMETER, "cannot use 'count' option for a streaming query"));
      return RestStatus::DONE;
    } else {
      CursorRepository* cursors = _vocbase.cursorRepository();
      TRI_ASSERT(cursors != nullptr);
      Cursor* cursor = cursors->createQueryStream(querySlice.copyString(),
                                                  bindVarsBuilder, _options,
                                                  batchSize, ttl, /*contextExt*/false);
   
      return generateCursorResult(rest::ResponseCode::CREATED, cursor);
    }
  }

  // non-stream case. Execute query, then build a cursor
  //  with the entire result set.
  VPackValueLength l;
  char const* queryStr = querySlice.getString(l);
  TRI_ASSERT(l > 0);

  auto query = std::make_unique<aql::Query>(
    false,
    _vocbase,
    arangodb::aql::QueryString(queryStr, static_cast<size_t>(l)),
    bindVarsBuilder,
    _options,
    arangodb::aql::PART_MAIN
  );

  std::shared_ptr<aql::SharedQueryState> ss = query->sharedState();
  auto self = shared_from_this();
  ss->setContinueHandler([this, self, ss] {
    continueHandlerExecution();
  });

  registerQuery(std::move(query));
  return processQuery();
}

//////////////////////////////////////////////////////////////////////////////
/// @brief Process the query registered in _query.
/// The function is repeatable, so whenever we need to WAIT
/// in AQL we can post a handler calling this function again.
//////////////////////////////////////////////////////////////////////////////

RestStatus RestCursorHandler::processQuery() {
  if (_query == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "Illegal state in RestQueryHandler, query not found.");
  }
  
  {
    // always clean up
    auto guard = scopeGuard([this]() {
      unregisterQuery();
    });
  
    // continue handler is registered earlier
    auto state = _query->execute(_queryRegistry, _queryResult);
    if (state == aql::ExecutionState::WAITING) {
      guard.cancel();
      return RestStatus::WAITING;
    }
    TRI_ASSERT(state == aql::ExecutionState::DONE);
  }
  
  // We cannot get into HASMORE here, or we would lose results.
  return handleQueryResult();
}

RestStatus RestCursorHandler::handleQueryResult() {
  if (_queryResult.code != TRI_ERROR_NO_ERROR) {
    if (_queryResult.code == TRI_ERROR_REQUEST_CANCELED ||
        (_queryResult.code == TRI_ERROR_QUERY_KILLED && wasCanceled())) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
    }

    THROW_ARANGO_EXCEPTION_MESSAGE(_queryResult.code, _queryResult.details);
  }

  VPackSlice qResult = _queryResult.result->slice();
  if (qResult.isNone()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  TRI_ASSERT(qResult.isArray());
  TRI_ASSERT(_options != nullptr);
  VPackSlice opts = _options->slice();

  size_t batchSize =
      VelocyPackHelper::getNumericValue<size_t>(opts, "batchSize", 1000);
  double ttl = VelocyPackHelper::getNumericValue<double>(opts, "ttl", 30);
  bool count = VelocyPackHelper::getBooleanValue(opts, "count", false);

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
      result.add("cached", VPackValue(_queryResult.cached));
      if (!_queryResult.extra) {
        result.add("extra", VPackValue(VPackValueType::Object));
        // no warnings
        result.add("warnings", VPackSlice::emptyArraySlice());
        result.close();
      } else {
        result.add("extra", _queryResult.extra->slice());
      }
      result.add(StaticStrings::Error, VPackValue(false));
      result.add(StaticStrings::Code, VPackValue(static_cast<int>(ResponseCode::CREATED)));
    } catch (std::exception const& ex) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, ex.what());
    } catch (...) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
    generateResult(rest::ResponseCode::CREATED, std::move(buffer),
                   _queryResult.context);
    return RestStatus::DONE;
  } else {
    // result is bigger than batchSize, and a cursor will be created
    CursorRepository* cursors = _vocbase.cursorRepository();
    TRI_ASSERT(cursors != nullptr);
    TRI_ASSERT(_queryResult.result.get() != nullptr);
    // steal the query result, cursor will take over the ownership
    Cursor* cursor = cursors->createFromQueryResult(std::move(_queryResult),
                                                    batchSize, ttl, count);
    return generateCursorResult(rest::ResponseCode::CREATED, cursor);
  }
}

/// @brief returns the short id of the server which should handle this request
uint32_t RestCursorHandler::forwardingTarget() {
  rest::RequestType const type = _request->requestType();
  if (type != rest::RequestType::PUT && type != rest::RequestType::DELETE_REQ) {
    return 0;
  }

  std::vector<std::string> const& suffixes = _request->suffixes();
  if (suffixes.size() < 1) {
    return 0;
  }

  uint64_t tick = arangodb::basics::StringUtils::uint64(suffixes[0]);
  uint32_t sourceServer = TRI_ExtractServerIdFromTick(tick);
  
  return (sourceServer == ServerState::instance()->getShortId())
      ? 0
      : sourceServer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register the currently running query
////////////////////////////////////////////////////////////////////////////////

void RestCursorHandler::registerQuery(std::unique_ptr<arangodb::aql::Query> query) {
  MUTEX_LOCKER(mutexLocker, _queryLock);

  if (_queryKilled) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
  }

  TRI_ASSERT(_query == nullptr);
  _query = std::move(query);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregister the currently running query
////////////////////////////////////////////////////////////////////////////////

void RestCursorHandler::unregisterQuery() {
  MUTEX_LOCKER(mutexLocker, _queryLock);
  _query.reset();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cancel the currently running query
////////////////////////////////////////////////////////////////////////////////

bool RestCursorHandler::cancelQuery() {
  MUTEX_LOCKER(mutexLocker, _queryLock);

  if (_query != nullptr) {
    _query->kill();
    _queryKilled = true;
    _hasStarted = true;
 
    // cursor is canceled. now remove the continue handler we may have registered in the query
    std::shared_ptr<aql::SharedQueryState> ss = _query->sharedState();
    ss->setContinueCallback();
    
    return true;
  } 
  
  if (!_hasStarted) {
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

void RestCursorHandler::buildOptions(VPackSlice const& slice) {
  _options = std::make_shared<VPackBuilder>();
  VPackObjectBuilder obj(_options.get());

  bool hasCache = false;
  bool hasMemoryLimit = false;
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
      } else if (keyName == "memoryLimit" && it.value.isNumber()) {
        hasMemoryLimit = true;
      }
      _options->add(keyName, it.value);
    }
  }

  if (!isStream) {  // ignore cache & count for streaming queries
    bool val = VelocyPackHelper::getBooleanValue(slice, "count", false);
    _options->add("count", VPackValue(val));
    if (!hasCache && slice.hasKey("cache")) {
      val = VelocyPackHelper::getBooleanValue(slice, "cache", false);
      _options->add("cache", VPackValue(val));
    }
  }

  VPackSlice batchSize = slice.get("batchSize");
  if (batchSize.isNumber()) {
    if ((batchSize.isDouble() && batchSize.getDouble() == 0.0) ||
        (batchSize.isInteger() && batchSize.getUInt() == 0)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_TYPE_ERROR, "expecting non-zero value for <batchSize>");
    }
    _options->add("batchSize", batchSize);
  } else {
    _options->add("batchSize", VPackValue(1000));
  }

  if (!hasMemoryLimit) {
    VPackSlice memoryLimit = slice.get("memoryLimit");
    if (memoryLimit.isNumber()) {
      _options->add("memoryLimit", memoryLimit);
    }
  }

  VPackSlice ttl = slice.get("ttl");
  _options->add("ttl", VPackValue(ttl.isNumber() ? ttl.getNumber<double>() : 30));
}

//////////////////////////////////////////////////////////////////////////////
/// @brief append the contents of the cursor into the response body
/// this function will also take care of the cursor and return it to the
/// registry if required
//////////////////////////////////////////////////////////////////////////////

RestStatus RestCursorHandler::generateCursorResult(rest::ResponseCode code,
                                                   arangodb::Cursor* cursor) {
  // always clean up
  auto guard = scopeGuard([this, &cursor]() {
    auto cursors = _vocbase.cursorRepository();
    TRI_ASSERT(cursors != nullptr);
    cursors->release(cursor);
  });

  // dump might delete the cursor
  std::shared_ptr<transaction::Context> ctx = cursor->context();

  VPackBuffer<uint8_t> buffer;
  VPackBuilder builder(buffer);
  builder.openObject();
  
  aql::ExecutionState state;
  Result r;
  auto self = shared_from_this();
  std::tie(state, r) = cursor->dump(builder, [this, self]() {
    continueHandlerExecution();
  });
  if (state == aql::ExecutionState::WAITING) {
    builder.clear();
    _leasedCursor = cursor;
    guard.cancel();
    return RestStatus::WAITING;
  }
  
  builder.add(StaticStrings::Error, VPackValue(false));
  builder.add(StaticStrings::Code, VPackValue(static_cast<int>(code)));
  builder.close();

  if (r.ok()) {
    _response->setContentType(rest::ContentType::JSON);
    generateResult(code, std::move(buffer), std::move(ctx));
  } else {
    generateError(r);
  }
  return RestStatus::DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_post_api_cursor
////////////////////////////////////////////////////////////////////////////////

RestStatus RestCursorHandler::createQueryCursor() {
  if (_request->payload().isEmptyObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_CORRUPTED_JSON);
    return RestStatus::DONE;
  }

  std::vector<std::string> const& suffixes = _request->suffixes();

  if (!suffixes.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting POST /_api/cursor");
    return RestStatus::DONE;
  }

  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);

  if (!parseSuccess) {
    // error message generated in parseVPackBody
    return RestStatus::DONE;
  }

  // tell RestCursorHandler::finalizeExecute that the request
  // could be parsed successfully and that it may look at it
  _isValidForFinalize = true;

  TRI_ASSERT(_query == nullptr);
  return registerQueryOrCursor(body);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_post_api_cursor_identifier
////////////////////////////////////////////////////////////////////////////////

RestStatus RestCursorHandler::modifyQueryCursor() {
  std::vector<std::string> const& suffixes = _request->suffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting PUT /_api/cursor/<cursor-id>");
    return RestStatus::DONE;
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
    return RestStatus::DONE;
  }

  return generateCursorResult(rest::ResponseCode::OK, cursor);;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_post_api_cursor_delete
////////////////////////////////////////////////////////////////////////////////

RestStatus RestCursorHandler::deleteQueryCursor() {
  std::vector<std::string> const& suffixes = _request->suffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting DELETE /_api/cursor/<cursor-id>");
    return RestStatus::DONE;
  }

  std::string const& id = suffixes[0];

  auto cursors = _vocbase.cursorRepository();
  TRI_ASSERT(cursors != nullptr);

  auto cursorId = static_cast<arangodb::CursorId>(
      arangodb::basics::StringUtils::uint64(id));
  bool found = cursors->remove(cursorId, Cursor::CURSOR_VPACK);

  if (!found) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_CURSOR_NOT_FOUND);
    return RestStatus::DONE;
  }

  VPackBuilder builder;
  builder.openObject();
  builder.add("id", VPackValue(id));
  builder.add(StaticStrings::Error, VPackValue(false));
  builder.add(StaticStrings::Code,
              VPackValue(static_cast<int>(rest::ResponseCode::ACCEPTED)));
  builder.close();

  generateResult(rest::ResponseCode::ACCEPTED, builder.slice());
  return RestStatus::DONE;
}
