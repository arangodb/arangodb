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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "RestHandler.h"

#include <velocypack/Exception.h>

#include "Basics/RecursiveLocker.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerState.h"
#include "Futures/Utilities.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/LogMacros.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "Rest/GeneralRequest.h"
#include "Rest/HttpResponse.h"
#include "Statistics/RequestStatistics.h"
#include "Utils/ExecContext.h"
#include "VocBase/ticks.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

thread_local RestHandler const* RestHandler::CURRENT_HANDLER = nullptr;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

RestHandler::RestHandler(application_features::ApplicationServer& server,
                         GeneralRequest* request, GeneralResponse* response)
    : _canceled(false),
      _request(request),
      _response(response),
      _server(server),
      _statistics(nullptr),
      _state(HandlerState::PREPARE),
      _handlerId(0) {}

RestHandler::~RestHandler() {
  RequestStatistics* stat = _statistics.exchange(nullptr);

  if (stat != nullptr) {
    stat->release();
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

void RestHandler::assignHandlerId() {
  _handlerId = TRI_NewServerSpecificTick();
}

uint64_t RestHandler::messageId() const {
  uint64_t messageId = 0UL;
  auto req = _request.get();
  auto res = _response.get();
  if (req) {
    messageId = req->messageId();
  } else if (res) {
    messageId = res->messageId();
  } else {
    LOG_TOPIC("4651e", WARN, Logger::COMMUNICATION)
        << "could not find corresponding request/response";
  }

  return messageId;
}

void RestHandler::setStatistics(RequestStatistics* stat) {
  RequestStatistics* old = _statistics.exchange(stat);
  if (old != nullptr) {
    old->release();
  }
}

futures::Future<Result> RestHandler::forwardRequest(bool& forwarded) {
  forwarded = false;
  if (!ServerState::instance()->isCoordinator()) {
    return futures::makeFuture(Result());
  }

  std::string serverId = forwardingTarget();
  if (serverId.empty()) {
    // no need to actually forward
    return futures::makeFuture(Result());
  }

  NetworkFeature const& nf = server().getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  if (pool == nullptr) {
    // nullptr happens only during controlled shutdown
    generateError(rest::ResponseCode::SERVICE_UNAVAILABLE,
                  TRI_ERROR_SHUTTING_DOWN, "shutting down server");
    return futures::makeFuture(Result(TRI_ERROR_SHUTTING_DOWN));
  }
  LOG_TOPIC("38d99", DEBUG, Logger::REQUESTS)
      << "forwarding request " << _request->messageId() << " to " << serverId;

  forwarded = true;

  bool useVst = false;
  if (_request->transportType() == Endpoint::TransportType::VST) {
    useVst = true;
  }
  std::string const& dbname = _request->databaseName();

  std::map<std::string, std::string> headers{_request->headers().begin(),
                                             _request->headers().end()};

  auto& values = _request->values();
  std::string params;
  for (auto const& i : values) {
    if (params.empty()) {
      params.push_back('?');
    } else {
      params.push_back('&');
    }
    params.append(StringUtils::urlEncode(i.first));
    params.push_back('=');
    params.append(StringUtils::urlEncode(i.second));
  }

  network::RequestOptions options;
  options.timeout = network::Timeout(300);
  options.contentType = rest::contentTypeToString(_request->contentType());
  options.acceptType = rest::contentTypeToString(_request->contentTypeResponse());

  auto requestType =
      fuerte::from_string(GeneralRequest::translateMethod(_request->requestType()));

  VPackStringRef resPayload = _request->rawPayload();
  VPackBuffer<uint8_t> payload(resPayload.size());
  payload.append(resPayload.data(), resPayload.size());

  auto future = network::sendRequest(pool, "server:" + serverId, requestType,
                                     "/_db/" + StringUtils::urlEncode(dbname) +
                                         _request->requestPath() + params,
                                     std::move(payload), std::move(headers), options);
  auto cb = [this, serverId, useVst,
             self = shared_from_this()](network::Response&& response) -> Result {
    int res = network::fuerteToArangoErrorCode(response);
    if (res != TRI_ERROR_NO_ERROR) {
      generateError(res);
      return Result(res);
    }

    resetResponse(static_cast<rest::ResponseCode>(response.response->statusCode()));
    _response->setContentType(fuerte::v1::to_string(response.response->contentType()));

    if (!useVst) {
      HttpResponse* httpResponse = dynamic_cast<HttpResponse*>(_response.get());
      if (_response == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "invalid response type");
      }
      httpResponse->body() = response.response->payloadAsString();
    } else {
      _response->setPayload(std::move(*response.response->stealPayload()), true);
    }
    

    auto const& resultHeaders = response.response->messageHeader().meta();
    for (auto const& it : resultHeaders) {
      _response->setHeader(it.first, it.second);
    }
    _response->setHeader(StaticStrings::RequestForwardedTo, serverId);

    return Result();
  };
  return std::move(future).thenValue(cb);
}

void RestHandler::handleExceptionPtr(std::exception_ptr eptr) noexcept {
  try {
    if (eptr) {
      std::rethrow_exception(eptr);
    }
  } catch (Exception const& ex) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC("11929", WARN, arangodb::Logger::FIXME)
    << "caught exception in " << name() << ": " << ex.what();
#endif
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    handleError(ex);
  } catch (arangodb::velocypack::Exception const& ex) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC("fdcbc", WARN, arangodb::Logger::FIXME)
    << "caught velocypack exception in " << name() << ": "
    << ex.what();
#endif
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    bool const isParseError =
    (ex.errorCode() == arangodb::velocypack::Exception::ParseError ||
     ex.errorCode() == arangodb::velocypack::Exception::UnexpectedControlCharacter);
    Exception err(isParseError ? TRI_ERROR_HTTP_CORRUPTED_JSON : TRI_ERROR_INTERNAL,
                  std::string("VPack error: ") + ex.what(), __FILE__, __LINE__);
    handleError(err);
  } catch (std::bad_alloc const& ex) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC("5c9f6", WARN, arangodb::Logger::FIXME)
    << "caught memory exception in " << name() << ": "
    << ex.what();
#endif
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    Exception err(TRI_ERROR_OUT_OF_MEMORY, ex.what(), __FILE__, __LINE__);
    handleError(err);
  } catch (std::exception const& ex) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC("252ea", WARN, arangodb::Logger::FIXME)
    << "caught exception in " << name() << ": " << ex.what();
#endif
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    Exception err(TRI_ERROR_INTERNAL, ex.what(), __FILE__, __LINE__);
    handleError(err);
  } catch (...) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC("f729d", WARN, arangodb::Logger::FIXME) << "caught unknown exception in " << name();
#endif
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    Exception err(TRI_ERROR_INTERNAL, __FILE__, __LINE__);
    handleError(err);
  }
}

void RestHandler::runHandlerStateMachine() {
  TRI_ASSERT(_callback);
  RECURSIVE_MUTEX_LOCKER(_executionMutex, _executionMutexOwner);

  while (true) {
    switch (_state) {
      case HandlerState::PREPARE:
        prepareEngine();
        break;

      case HandlerState::EXECUTE: {
        executeEngine(/*isContinue*/false);
        if (_state == HandlerState::PAUSED) {
          shutdownExecute(false);
          LOG_TOPIC("23a33", DEBUG, Logger::COMMUNICATION)
              << "Pausing rest handler execution";
          return;  // stop state machine
        }
        break;
      }

      case HandlerState::CONTINUED: {
        executeEngine(/*isContinue*/true);
        if (_state == HandlerState::PAUSED) {
          shutdownExecute(/*isFinalized*/false);
          LOG_TOPIC("23727", DEBUG, Logger::COMMUNICATION)
              << "Pausing rest handler execution";
          return;  // stop state machine
        }
        break;
      }

      case HandlerState::PAUSED:
        LOG_TOPIC("ae26f", DEBUG, Logger::COMMUNICATION)
            << "Resuming rest handler execution";
        _state = HandlerState::CONTINUED;
        break;

      case HandlerState::FINALIZE:
        RequestStatistics::SET_REQUEST_END(_statistics);
        shutdownEngine();
        
        // compress response if required
        compressResponse();
        // Callback may stealStatistics!
        _callback(this);
        break;

      case HandlerState::FAILED:
        RequestStatistics::SET_REQUEST_END(_statistics);
        // Callback may stealStatistics!
        _callback(this);
        // No need to finalize here!
        return;

      case HandlerState::DONE:
        return;
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

void RestHandler::prepareEngine() {
  // set end immediately so we do not get netative statistics
  RequestStatistics::SET_REQUEST_START_END(_statistics);

  if (_canceled) {
    _state = HandlerState::FAILED;
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);

    Exception err(TRI_ERROR_REQUEST_CANCELED,
                  "request has been canceled by user", __FILE__, __LINE__);
    handleError(err);
    return;
  }

  try {
    prepareExecute(false);
    _state = HandlerState::EXECUTE;
    return;
  } catch (Exception const& ex) {
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    handleError(ex);
  } catch (std::exception const& ex) {
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    Exception err(TRI_ERROR_INTERNAL, ex.what(), __FILE__, __LINE__);
    handleError(err);
  } catch (...) {
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    Exception err(TRI_ERROR_INTERNAL, __FILE__, __LINE__);
    handleError(err);
  }

  _state = HandlerState::FAILED;
}

/// Execute the rest handler state machine
void RestHandler::continueHandlerExecution() {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  {
    RECURSIVE_MUTEX_LOCKER(_executionMutex, _executionMutexOwner);
    TRI_ASSERT(_state == HandlerState::PAUSED);
  }
#endif
  runHandlerStateMachine();
}

void RestHandler::shutdownEngine() {
  RestHandler::CURRENT_HANDLER = this;

  // shutdownExecute is noexcept
  shutdownExecute(true);

  RestHandler::CURRENT_HANDLER = nullptr;
  _state = HandlerState::DONE;
}

void RestHandler::executeEngine(bool isContinue) {
  ExecContext* exec = static_cast<ExecContext*>(_request->requestContext());
  ExecContextScope scope(exec);

  RestHandler::CURRENT_HANDLER = this;

  try {
    RestStatus result = RestStatus::DONE;
    if (isContinue) {
      // only need to run prepareExecute() again when we are continuing
      // otherwise prepareExecute() was already run in the PREPARE phase
      prepareExecute(true);
      result = continueExecute();
    } else {
      result = execute();
    }

    RestHandler::CURRENT_HANDLER = nullptr;

    if (result == RestStatus::WAITING) {
      _state = HandlerState::PAUSED;  // wait for someone to continue the state
                                      // machine
      return;
    }

    if (_response == nullptr) {
      Exception err(TRI_ERROR_INTERNAL, "no response received from handler",
                    __FILE__, __LINE__);
      handleError(err);
    }

    _state = HandlerState::FINALIZE;
    return;
  } catch (Exception const& ex) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC("11928", WARN, arangodb::Logger::FIXME)
        << "caught exception in " << name() << ": " << ex.what();
#endif
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    handleError(ex);
  } catch (arangodb::velocypack::Exception const& ex) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC("fdcbb", WARN, arangodb::Logger::FIXME)
        << "caught velocypack exception in " << name() << ": "
        << ex.what();
#endif
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    bool const isParseError =
        (ex.errorCode() == arangodb::velocypack::Exception::ParseError ||
         ex.errorCode() == arangodb::velocypack::Exception::UnexpectedControlCharacter);
    Exception err(isParseError ? TRI_ERROR_HTTP_CORRUPTED_JSON : TRI_ERROR_INTERNAL,
                  std::string("VPack error: ") + ex.what(), __FILE__, __LINE__);
    handleError(err);
  } catch (std::bad_alloc const& ex) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC("5c9f5", WARN, arangodb::Logger::FIXME)
        << "caught memory exception in " << name() << ": "
        << ex.what();
#endif
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    Exception err(TRI_ERROR_OUT_OF_MEMORY, ex.what(), __FILE__, __LINE__);
    handleError(err);
  } catch (std::exception const& ex) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC("252e9", WARN, arangodb::Logger::FIXME)
        << "caught exception in " << name() << ": " << ex.what();
#endif
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    Exception err(TRI_ERROR_INTERNAL, ex.what(), __FILE__, __LINE__);
    handleError(err);
  } catch (...) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC("f729c", WARN, arangodb::Logger::FIXME) << "caught unknown exception in " << name();
#endif
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    Exception err(TRI_ERROR_INTERNAL, __FILE__, __LINE__);
    handleError(err);
  }

  RestHandler::CURRENT_HANDLER = nullptr;
  _state = HandlerState::FAILED;
}

void RestHandler::generateError(rest::ResponseCode code, int errorNumber,
                                std::string const& message) {
  resetResponse(code);

  VPackBuffer<uint8_t> buffer;
  VPackBuilder builder(buffer);
  try {
    builder.add(VPackValue(VPackValueType::Object));
    builder.add(StaticStrings::Code, VPackValue(static_cast<int>(code)));
    builder.add(StaticStrings::Error, VPackValue(true));
    builder.add(StaticStrings::ErrorMessage, VPackValue(message));
    builder.add(StaticStrings::ErrorNum, VPackValue(errorNumber));
    builder.close();

    VPackOptions options(VPackOptions::Defaults);
    options.escapeUnicode = true;

    TRI_ASSERT(options.escapeUnicode);
    if (_request != nullptr) {
      _response->setContentType(_request->contentTypeResponse());
    }
    _response->setPayload(std::move(buffer), true, options,
                          /*resolveExternals*/false);
  } catch (...) {
    // exception while generating error
  }
}

void RestHandler::compressResponse() {
  if (_response->isCompressionAllowed()) {

    switch (_request->acceptEncoding()) {
      case rest::EncodingType::DEFLATE:
        _response->deflate();
        _response->setHeader(StaticStrings::ContentEncoding, StaticStrings::EncodingDeflate);
        break;

      default:
        break;
    }

  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an error
////////////////////////////////////////////////////////////////////////////////

void RestHandler::generateError(rest::ResponseCode code, int errorCode) {
  char const* message = TRI_errno_string(errorCode);

  if (message != nullptr) {
    generateError(code, errorCode, std::string(message));
  } else {
    generateError(code, errorCode, std::string("unknown error"));
  }
}

// generates an error
void RestHandler::generateError(arangodb::Result const& r) {
  ResponseCode code = GeneralResponse::responseCode(r.errorNumber());
  generateError(code, r.errorNumber(), r.errorMessage());
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

void RestHandler::resetResponse(rest::ResponseCode code) {
  TRI_ASSERT(_response != nullptr);
  _response->reset(code);
}
