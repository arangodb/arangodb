////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include <fuerte/jwt.h>
#include <velocypack/Exception.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/RecursiveLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/debugging.h"
#include "Basics/dtrace-wrapper.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerState.h"
#include "Futures/Utilities.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/LogMacros.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "Rest/GeneralRequest.h"
#include "Rest/HttpResponse.h"
#include "Scheduler/SchedulerFeature.h"
#include "Scheduler/SupervisedScheduler.h"
#include "Statistics/RequestStatistics.h"
#include "Utils/ExecContext.h"
#include "VocBase/ticks.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

RestHandler::RestHandler(application_features::ApplicationServer& server,
                         GeneralRequest* request, GeneralResponse* response)
    :
      _request(request),
      _response(response),
      _server(server),
      _statistics(),
      _handlerId(0),
      _state(HandlerState::PREPARE),
      _lane(RequestLane::UNDEFINED),
      _canceled(false) {}

RestHandler::~RestHandler() = default;

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
  
RequestLane RestHandler::determineRequestLane() {
  if (_lane == RequestLane::UNDEFINED) {
    bool found;
    _request->header(StaticStrings::XArangoFrontend, found);

    if (found) {
      _lane = RequestLane::CLIENT_UI;
    } else {
      _lane = lane();
    }
  }
  TRI_ASSERT(_lane != RequestLane::UNDEFINED);
  return _lane;
}

void RestHandler::trackQueueStart() noexcept {
  TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
  _statistics.SET_QUEUE_START(SchedulerFeature::SCHEDULER->queueStatistics()._queued);
}

void RestHandler::trackQueueEnd() noexcept {
  _statistics.SET_QUEUE_END();
}

void RestHandler::trackTaskStart() noexcept {
  if (PriorityRequestLane(determineRequestLane()) == RequestPriority::LOW) {
    TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
    SchedulerFeature::SCHEDULER->trackBeginOngoingLowPriorityTask();
  }
}

void RestHandler::trackTaskEnd() noexcept {
  if (PriorityRequestLane(determineRequestLane()) == RequestPriority::LOW) {
    TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
    SchedulerFeature::SCHEDULER->trackEndOngoingLowPriorityTask();
    
    // update the time the last low priority item spent waiting in the queue.
    
    // the queueing time is in ms
    uint64_t queueTimeMs = static_cast<uint64_t>(_statistics.ELAPSED_WHILE_QUEUED() * 1000.0);
    SchedulerFeature::SCHEDULER->setLastLowPriorityDequeueTime(queueTimeMs);
  }
}

RequestStatistics::Item&& RestHandler::stealStatistics() {
  return std::move(_statistics);
}

void RestHandler::setStatistics(RequestStatistics::Item&& stat) {
  _statistics = std::move(stat);
}

futures::Future<Result> RestHandler::forwardRequest(bool& forwarded) {
  forwarded = false;
  if (!ServerState::instance()->isCoordinator()) {
    return futures::makeFuture(Result());
  }

  ResultT forwardResult = forwardingTarget();
  if (forwardResult.fail()) {
    return futures::makeFuture(forwardResult.result());
  }

  auto forwardContent = forwardResult.get();
  std::string serverId = std::get<0>(forwardContent);
  bool removeHeader = std::get<1>(forwardContent);

  if (removeHeader) {
    _request->removeHeader(StaticStrings::Authorization);
    _request->setUser("");
  }

  if (serverId.empty()) {
    // no need to actually forward
    return futures::makeFuture(Result());
  }

  NetworkFeature& nf = server().getFeature<NetworkFeature>();
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

  // always remove HTTP "Connection" header, so that we don't relay 
  // "Connection: Close" or "Connection: Keep-Alive" or such
  headers.erase(StaticStrings::Connection);

  if (headers.find(StaticStrings::Authorization) == headers.end()) {
    // No authorization header is set, this is in particular the case if this
    // request is coming in with VelocyStream, where the authentication happens
    // once at the beginning of the connection and not with every request.
    // In this case, we have to produce a proper JWT token as authorization:
    auto auth = AuthenticationFeature::instance();
    if (auth != nullptr && auth->isActive()) {
      // when in superuser mode, username is empty
      // in this case ClusterComm will add the default superuser token
      std::string const& username = _request->user();
      if (!username.empty()) {
        headers.emplace(StaticStrings::Authorization,
                        "bearer " + fuerte::jwt::generateUserToken(auth->tokenCache().jwtSecret(), username));
      }
    }
  }

  network::RequestOptions options;
  options.database = dbname;
  options.timeout = network::Timeout(900);
  
  if (useVst && _request->contentType() == rest::ContentType::UNSET) {
    // request is using VST, but doesn't have a Content-Type header set.
    // it is likely VelocyPack content, so let's assume that here.
    // should fix issue BTS-133.
    options.contentType = rest::contentTypeToString(rest::ContentType::VPACK);
  } else {
    // if the type is unset JSON is used
    options.contentType = rest::contentTypeToString(_request->contentType());
  }

  options.acceptType = rest::contentTypeToString(_request->contentTypeResponse());
    
  for (auto const& i : _request->values()) {
    options.param(i.first, i.second);
  }
  
  auto requestType =
      fuerte::from_string(GeneralRequest::translateMethod(_request->requestType()));

  VPackStringRef resPayload = _request->rawPayload();
  VPackBuffer<uint8_t> payload(resPayload.size());
  payload.append(resPayload.data(), resPayload.size());
  
  nf.trackForwardedRequest();
 
  auto future = network::sendRequest(pool, "server:" + serverId, requestType,
                                     _request->requestPath(),
                                     std::move(payload), options, std::move(headers));
  auto cb = [this, serverId, useVst,
             self = shared_from_this()](network::Response&& response) -> Result {
    auto res = network::fuerteToArangoErrorCode(response);
    if (res != TRI_ERROR_NO_ERROR) {
      generateError(res);
      return Result(res);
    }

    resetResponse(static_cast<rest::ResponseCode>(response.statusCode()));
    _response->setContentType(fuerte::v1::to_string(response.response().contentType()));
    
    if (!useVst) {
      HttpResponse* httpResponse = dynamic_cast<HttpResponse*>(_response.get());
      if (_response == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "invalid response type");
      }
      httpResponse->body() = response.response().payloadAsString();
    } else {
      _response->setPayload(std::move(*response.response().stealPayload()));
    }
    

    auto const& resultHeaders = response.response().messageHeader().meta();
    for (auto const& it : resultHeaders) {
      if (it.first == "http/1.1") {
        // never forward this header, as the HTTP response code was already set
        // via "resetResponse" above
        continue;
      }
      _response->setHeader(it.first, it.second);
    }
    _response->setHeaderNC(StaticStrings::RequestForwardedTo, serverId);
    
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
    << "maintainer mode: caught exception in " << name() << ": " << ex.what();
#endif
    handleError(ex);
  } catch (arangodb::velocypack::Exception const& ex) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC("fdcbc", WARN, arangodb::Logger::FIXME)
    << "maintainer mode: caught velocypack exception in " << name() << ": "
    << ex.what();
#endif
    bool const isParseError =
    (ex.errorCode() == arangodb::velocypack::Exception::ParseError ||
     ex.errorCode() == arangodb::velocypack::Exception::UnexpectedControlCharacter);
    Exception err(isParseError ? TRI_ERROR_HTTP_CORRUPTED_JSON : TRI_ERROR_INTERNAL,
                  std::string("VPack error: ") + ex.what(), __FILE__, __LINE__);
    handleError(err);
  } catch (std::bad_alloc const& ex) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC("5c9f6", WARN, arangodb::Logger::FIXME)
    << "maintainer mode: caught memory exception in " << name() << ": "
    << ex.what();
#endif
    Exception err(TRI_ERROR_OUT_OF_MEMORY, ex.what(), __FILE__, __LINE__);
    handleError(err);
  } catch (std::exception const& ex) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC("252ea", WARN, arangodb::Logger::FIXME)
    << "maintainer mode: caught exception in " << name() << ": " << ex.what();
#endif
    Exception err(TRI_ERROR_INTERNAL, ex.what(), __FILE__, __LINE__);
    handleError(err);
  } catch (...) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC("f729d", WARN, arangodb::Logger::FIXME) << "maintainer mode: caught unknown exception in " << name();
#endif
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
              << "Pausing rest handler execution " << this;
          return;  // stop state machine
        }
        break;
      }

      case HandlerState::CONTINUED: {
        executeEngine(/*isContinue*/true);
        if (_state == HandlerState::PAUSED) {
          shutdownExecute(/*isFinalized*/false);
          LOG_TOPIC("23727", DEBUG, Logger::COMMUNICATION)
              << "Pausing rest handler execution " << this;
          return;  // stop state machine
        }
        break;
      }

      case HandlerState::PAUSED:
        LOG_TOPIC("ae26f", DEBUG, Logger::COMMUNICATION)
            << "Resuming rest handler execution " << this;
        _state = HandlerState::CONTINUED;
        break;

      case HandlerState::FINALIZE:
        _statistics.SET_REQUEST_END();

        // shutdownExecute is noexcept
        shutdownExecute(true); // may not be moved down

        _state = HandlerState::DONE;
        
        // compress response if required
        compressResponse();
        // Callback may stealStatistics!
        _callback(this);
        break;

      case HandlerState::FAILED:
        _statistics.SET_REQUEST_END();
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
  _statistics.SET_REQUEST_START_END();

  if (_canceled) {
    _state = HandlerState::FAILED;

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
    handleError(ex);
  } catch (std::exception const& ex) {
    Exception err(TRI_ERROR_INTERNAL, ex.what(), __FILE__, __LINE__);
    handleError(err);
  } catch (...) {
    Exception err(TRI_ERROR_INTERNAL, __FILE__, __LINE__);
    handleError(err);
  }

  _state = HandlerState::FAILED;
}

/// Execute the rest handler state machine. Retry the wakeup,
/// returns true if _state == PAUSED, false otherwise
bool RestHandler::wakeupHandler() {
  RECURSIVE_MUTEX_LOCKER(_executionMutex, _executionMutexOwner);
  if (_state == HandlerState::PAUSED) {
    runHandlerStateMachine(); // may change _state
    return _state == HandlerState::PAUSED;
  }
  return false;
}

void RestHandler::executeEngine(bool isContinue) {
  DTRACE_PROBE1(arangod, RestHandlerExecuteEngine, this);
  ExecContext* exec = static_cast<ExecContext*>(_request->requestContext());
  ExecContextScope scope(exec);

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
        << "maintainer mode: caught exception in " << name() << ": " << ex.what();
#endif
    handleError(ex);
  } catch (arangodb::velocypack::Exception const& ex) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC("fdcbb", WARN, arangodb::Logger::FIXME)
        << "maintainer mode: caught velocypack exception in " << name() << ": "
        << ex.what();
#endif
    bool const isParseError =
        (ex.errorCode() == arangodb::velocypack::Exception::ParseError ||
         ex.errorCode() == arangodb::velocypack::Exception::UnexpectedControlCharacter);
    Exception err(isParseError ? TRI_ERROR_HTTP_CORRUPTED_JSON : TRI_ERROR_INTERNAL,
                  std::string("VPack error: ") + ex.what(), __FILE__, __LINE__);
    handleError(err);
  } catch (std::bad_alloc const& ex) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC("5c9f5", WARN, arangodb::Logger::FIXME)
        << "maintainer mode: caught memory exception in " << name() << ": "
        << ex.what();
#endif
    Exception err(TRI_ERROR_OUT_OF_MEMORY, ex.what(), __FILE__, __LINE__);
    handleError(err);
  } catch (std::exception const& ex) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC("252e9", WARN, arangodb::Logger::FIXME)
        << "maintainer mode: caught exception in " << name() << ": " << ex.what();
#endif
    Exception err(TRI_ERROR_INTERNAL, ex.what(), __FILE__, __LINE__);
    handleError(err);
  } catch (...) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC("f729c", WARN, arangodb::Logger::FIXME) << "maintainer mode: caught unknown exception in " << name();
#endif
    Exception err(TRI_ERROR_INTERNAL, __FILE__, __LINE__);
    handleError(err);
  }

  _state = HandlerState::FAILED;
}

void RestHandler::generateError(rest::ResponseCode code, ErrorCode errorNumber,
                                std::string_view const errorMessage) {
  resetResponse(code);

  if (_request->requestType() != rest::RequestType::HEAD) {
    VPackBuffer<uint8_t> buffer;
    VPackBuilder builder(buffer);
    try {
      builder.add(VPackValue(VPackValueType::Object));
      builder.add(StaticStrings::Code, VPackValue(static_cast<int>(code)));
      builder.add(StaticStrings::Error, VPackValue(true));
      builder.add(StaticStrings::ErrorMessage, VPackValue(errorMessage));
      builder.add(StaticStrings::ErrorNum, VPackValue(errorNumber));
      builder.close();

      if (_request != nullptr) {
        _response->setContentType(_request->contentTypeResponse());
      }
      _response->setPayload(std::move(buffer), VPackOptions::Defaults,
                            /*resolveExternals*/ false);
    } catch (...) {
      // exception while generating error
    }
  }
}

void RestHandler::compressResponse() {
  if (_response->isCompressionAllowed()) {

    switch (_request->acceptEncoding()) {
      case rest::EncodingType::DEFLATE:
        _response->deflate();
        _response->setHeaderNC(StaticStrings::ContentEncoding, StaticStrings::EncodingDeflate);
        break;

      default:
        break;
    }

  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an error
////////////////////////////////////////////////////////////////////////////////

void RestHandler::generateError(rest::ResponseCode code, ErrorCode errorNumber) {
  auto const message = TRI_errno_string(errorNumber);

  if (message.data() != nullptr) {
    generateError(code, errorNumber, message);
  } else {
    generateError(code, errorNumber, "unknown error");
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
