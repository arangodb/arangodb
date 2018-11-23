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

#include "Basics/StringUtils.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/GeneralCommTask.h"
#include "Logger/Logger.h"
#include "Rest/GeneralRequest.h"
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

RestHandler::RestHandler(GeneralRequest* request, GeneralResponse* response)
    : _canceled(false),
      _request(request),
      _response(response),
      _statistics(nullptr),
      _handlerId(0),
      _state(HandlerState::PREPARE) {}

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
    LOG_TOPIC(WARN, Logger::COMMUNICATION)
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

bool RestHandler::forwardRequest() {
  if (!ServerState::instance()->isCoordinator()) {
    return false;
  }

  // TODO refactor into a more general/customizable method
  //
  // The below is mostly copied and only lightly modified from
  // RestReplicationHandler::handleTrampolineCoordinator; however, that method
  // needs some more specific checks regarding headers and param values, so we
  // can't just reuse this method there. Maybe we just need to implement some
  // virtual methods to handle param/header filtering?

  // TODO verify that vst -> http -> vst conversion works correctly

  uint32_t shortId = forwardingTarget();
  if (shortId == 0) {
    // no need to actually forward
    return false;
  }

  std::string serverId = ClusterInfo::instance()->getCoordinatorByShortID(shortId);

  if ("" == serverId) {
    // no mapping in agency, try to handle the request here
    return false;
  }

  LOG_TOPIC(DEBUG, Logger::REQUESTS) << "forwarding request " << _request->messageId() << " to " << serverId;

  bool useVst = false;
  if (_request->transportType() == Endpoint::TransportType::VST) {
    useVst = true;
  }
  std::string const& dbname = _request->databaseName();

  std::unordered_map<std::string, std::string> const& oldHeaders =
      _request->headers();
  std::unordered_map<std::string, std::string>::const_iterator it =
      oldHeaders.begin();
  std::unordered_map<std::string, std::string> headers;
  while (it != oldHeaders.end()) {
    std::string const& key = (*it).first;

    // ignore the following headers
    if (key != StaticStrings::Authorization) {
      headers.emplace(key, (*it).second);
    }
    ++it;
  }
  auto auth = AuthenticationFeature::instance();
  if (auth != nullptr && auth->isActive()) {

    // when in superuser mode, username is empty
    //  in this case ClusterComm will add the default superuser token
    std::string const& username = _request->user();
    if (!username.empty()) {

      VPackBuilder builder;
      {
        VPackObjectBuilder payload{&builder};
        payload->add("preferred_username", VPackValue(username));
      }
      VPackSlice slice = builder.slice();
      headers.emplace(StaticStrings::Authorization,
                      "bearer " + auth->tokenCache().generateJwt(slice));
    }
  }

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

  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    generateError(rest::ResponseCode::SERVICE_UNAVAILABLE,
                  TRI_ERROR_SHUTTING_DOWN, "shutting down server");
    return true;
  }

  std::unique_ptr<ClusterCommResult> res;
  if (!useVst) {
    HttpRequest* httpRequest = dynamic_cast<HttpRequest*>(_request.get());
    if (httpRequest == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid request type");
    }

    // Send a synchronous request to that shard using ClusterComm:
    res = cc->syncRequest(TRI_NewTickServer(), "server:" + serverId,
                          _request->requestType(),
                          "/_db/" + StringUtils::urlEncode(dbname) +
                              _request->requestPath() + params,
                          httpRequest->body(), headers, 300.0);
  } else {
    // do we need to handle multiple payloads here? - TODO
    // here we switch from vst to http
    res = cc->syncRequest(TRI_NewTickServer(), "server:" + serverId,
                          _request->requestType(),
                          "/_db/" + StringUtils::urlEncode(dbname) +
                              _request->requestPath() + params,
                          _request->payload().toJson(), headers, 300.0);
  }

  if (res->status == CL_COMM_TIMEOUT) {
    // No reply, we give up:
    generateError(rest::ResponseCode::BAD, TRI_ERROR_CLUSTER_TIMEOUT,
                  "timeout within cluster");
    return true;
  }

  if (res->status == CL_COMM_BACKEND_UNAVAILABLE) {
    // there is no result
    generateError(rest::ResponseCode::BAD, TRI_ERROR_CLUSTER_CONNECTION_LOST,
                  "lost connection within cluster");
    return true;
  }

  if (res->status == CL_COMM_ERROR) {
    // This could be a broken connection or an Http error:
    TRI_ASSERT(nullptr != res->result && res->result->isComplete());
    // In this case a proper HTTP error was reported by the DBserver,
    // we simply forward the result. Intentionally fall through here.
  }

  bool dummy;
  resetResponse(
      static_cast<rest::ResponseCode>(res->result->getHttpReturnCode()));

  _response->setContentType(
      res->result->getHeaderField(StaticStrings::ContentTypeHeader, dummy));

  if (!useVst) {
    HttpResponse* httpResponse = dynamic_cast<HttpResponse*>(_response.get());
    if (_response == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid response type");
    }
    httpResponse->body().swap(&(res->result->getBody()));
  } else {
    // need to switch back from http to vst
    std::shared_ptr<VPackBuilder> builder = res->result->getBodyVelocyPack();
    std::shared_ptr<VPackBuffer<uint8_t>> buf = builder->steal();
    _response->setPayload(std::move(*buf),
                          true);
  }

  auto const& resultHeaders = res->result->getHeaderFields();
  for (auto const& it : resultHeaders) {
    _response->setHeader(it.first, it.second);
  }
  _response->setHeader(StaticStrings::RequestForwardedTo, serverId);
  return true;
}

void RestHandler::runHandlerStateMachine() {
  TRI_ASSERT(_callback);
  MUTEX_LOCKER(locker, _executionMutex);

  while (true) {
    switch (_state) {
      case HandlerState::PREPARE:
        prepareEngine();
        break;

      case HandlerState::EXECUTE: {
        executeEngine(false);
        if (_state == HandlerState::PAUSED) {
          shutdownExecute(false);
          LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "Pausing rest handler execution";
          return; // stop state machine
        }
        break;
      }

      case HandlerState::CONTINUED: {
        executeEngine(true);
        if (_state == HandlerState::PAUSED) {
          shutdownExecute(false);
          LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
              << "Pausing rest handler execution";
          return;  // stop state machine
        }
        break;
      }

      case HandlerState::PAUSED:
        LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
            << "Resuming rest handler execution";
        _state = HandlerState::CONTINUED;
        break;

      case HandlerState::FINALIZE:
        RequestStatistics::SET_REQUEST_END(_statistics);
        // Callback may stealStatistics!
        _callback(this);
        // Schedule callback BEFORE! finalize
        shutdownEngine();
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

RequestPriority RestHandler::priority(RequestLane l) const {
  RequestPriority p = RequestPriority::LOW;

  switch (l) {
    case RequestLane::AGENCY_INTERNAL:
    case RequestLane::CLIENT_FAST:
    case RequestLane::CLUSTER_INTERNAL:
    case RequestLane::SERVER_REPLICATION:
      p = RequestPriority::HIGH;
      break;

    case RequestLane::CLIENT_AQL:
    case RequestLane::CLIENT_SLOW:
    case RequestLane::AGENCY_CLUSTER:
    case RequestLane::CLUSTER_ADMIN:
    case RequestLane::CLIENT_V8:
    case RequestLane::CLUSTER_V8:
    case RequestLane::TASK_V8:
      p = RequestPriority::LOW;
      break;
  }

  if (p == RequestPriority::HIGH) {
    return p;
  }

  bool found;
  _request->header(StaticStrings::XArangoFrontend, found);

  if (!found) {
    return p;
  }

  return RequestPriority::MED;
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
    MUTEX_LOCKER(locker, _executionMutex);
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
  TRI_ASSERT(ExecContext::CURRENT == nullptr);
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
    LOG_TOPIC(WARN, arangodb::Logger::FIXME)
        << "caught exception in " << name() << ": "
        << DIAGNOSTIC_INFORMATION(ex);
#endif
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    handleError(ex);
  } catch (arangodb::velocypack::Exception const& ex) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC(WARN, arangodb::Logger::FIXME)
        << "caught velocypack exception in " << name() << ": "
        << DIAGNOSTIC_INFORMATION(ex);
#endif
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    bool const isParseError = (ex.errorCode() == arangodb::velocypack::Exception::ParseError);
    Exception err(isParseError ? TRI_ERROR_HTTP_CORRUPTED_JSON : TRI_ERROR_INTERNAL, std::string("VPack error: ") + ex.what(),
                  __FILE__, __LINE__);
    handleError(err);
  } catch (std::bad_alloc const& ex) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC(WARN, arangodb::Logger::FIXME)
        << "caught memory exception in " << name() << ": "
        << DIAGNOSTIC_INFORMATION(ex);
#endif
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    Exception err(TRI_ERROR_OUT_OF_MEMORY, ex.what(), __FILE__, __LINE__);
    handleError(err);
  } catch (std::exception const& ex) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC(WARN, arangodb::Logger::FIXME)
        << "caught exception in " << name() << ": "
        << DIAGNOSTIC_INFORMATION(ex);
#endif
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    Exception err(TRI_ERROR_INTERNAL, ex.what(), __FILE__, __LINE__);
    handleError(err);
  } catch (...) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC(WARN, arangodb::Logger::FIXME)
        << "caught unknown exception in " << name();
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
    builder.add(StaticStrings::Error, VPackValue(true));
    builder.add(StaticStrings::ErrorMessage, VPackValue(message));
    builder.add(StaticStrings::Code,
                VPackValue(static_cast<int>(code)));
    builder.add(StaticStrings::ErrorNum,
                VPackValue(errorNumber));
    builder.close();

    VPackOptions options(VPackOptions::Defaults);
    options.escapeUnicode = true;

    TRI_ASSERT(options.escapeUnicode);
    if (_request != nullptr) {
      _response->setContentType(_request->contentTypeResponse());
    }
    _response->setPayload(std::move(buffer), true, options);
  } catch (...) {
    // exception while generating error
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
