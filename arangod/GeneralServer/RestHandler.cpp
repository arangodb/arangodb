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
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/GeneralCommTask.h"
#include "Logger/Logger.h"
#include "Rest/GeneralRequest.h"
#include "Statistics/RequestStatistics.h"
#include "Utils/ExecContext.h"

#include <iostream>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

namespace {
std::atomic_uint_fast64_t NEXT_HANDLER_ID(
    static_cast<uint64_t>(TRI_microtime() * 100000.0));
}

thread_local RestHandler const* RestHandler::CURRENT_HANDLER = nullptr;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

RestHandler::RestHandler(GeneralRequest* request, GeneralResponse* response)
    : _handlerId(NEXT_HANDLER_ID.fetch_add(1, std::memory_order_seq_cst)),
      _canceled(false),
      _request(request),
      _response(response),
      _statistics(nullptr),
      _state(HandlerState::PREPARE) {
  bool found;
  std::string const& startThread =
      _request->header(StaticStrings::StartThread, found);

  if (found) {
    _needsOwnThread = StringUtils::boolean(startThread);
  }
}

RestHandler::~RestHandler() {
  RequestStatistics* stat = _statistics.exchange(nullptr);

  if (stat != nullptr) {
    stat->release();
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

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
  // TODO refactor into a more general/customizable method
  //
  // The below is mostly copied and only lightly modified from
  // RestReplicationHandler::handleTrampolineCoordinator; however, that method
  // needs some more specific checks regarding headers and param values, so we
  // can't just reuse this method there. Maybe we just need to implement some
  // virtual methods to handle param/header filtering?

  // TODO verify that vst -> http -> vst conversion works correctly

  // TODO verify that async requests work correctly

  // TODO verify that user-JWT works correctly

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
    VPackBuilder builder;
    {
      VPackObjectBuilder payload{&builder};
      payload->add("preferred_username", VPackValue(_request->user()));
    }
    VPackSlice slice = builder.slice();
    headers.emplace(StaticStrings::Authorization,
                    "bearer " + auth->tokenCache()->generateJwt(slice));
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
    res = cc->syncRequest("", TRI_NewTickServer(), "server:" + serverId,
                          _request->requestType(),
                          "/_db/" + StringUtils::urlEncode(dbname) +
                              _request->requestPath() + params,
                          httpRequest->body(), headers, 300.0);
  } else {
    // do we need to handle multiple payloads here? - TODO
    // here we switch from vst to http
    res = cc->syncRequest("", TRI_NewTickServer(), "server:" + serverId,
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
  return true;
}

void RestHandler::runHandlerStateMachine() {
  TRI_ASSERT(_callback);

  while (true) {
    switch (_state) {
      case HandlerState::PREPARE:
        this->prepareEngine();
        break;

      case HandlerState::EXECUTE: {
        int res = this->executeEngine();
        if (res != TRI_ERROR_NO_ERROR) {
          this->finalizeEngine();
        }
        if (_state == HandlerState::PAUSED) {
          LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
              << "Pausing rest handler execution";
          return;  // stop state machine
        }
        break;
      }

      case HandlerState::PAUSED:
        LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
            << "Resuming rest handler execution";
        TRI_ASSERT(_response != nullptr);
        _callback(this);
        _state = HandlerState::FINALIZE;
        break;

      case HandlerState::FINALIZE:
        this->finalizeEngine();
        break;

      case HandlerState::DONE:
      case HandlerState::FAILED:
        return;
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

int RestHandler::prepareEngine() {
  // set end immediately so we do not get netative statistics
  RequestStatistics::SET_REQUEST_START_END(_statistics);

  if (_canceled) {
    _state = HandlerState::DONE;
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);

    Exception err(TRI_ERROR_REQUEST_CANCELED,
                  "request has been canceled by user", __FILE__, __LINE__);
    handleError(err);

    _callback(this);
    return TRI_ERROR_REQUEST_CANCELED;
  }

  try {
    prepareExecute();
    _state = HandlerState::EXECUTE;
    return TRI_ERROR_NO_ERROR;
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
  _callback(this);
  return TRI_ERROR_INTERNAL;
}

int RestHandler::finalizeEngine() {
  int res = TRI_ERROR_NO_ERROR;

  try {
    finalizeExecute();
  } catch (Exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "caught exception in " << name() << ": "
        << DIAGNOSTIC_INFORMATION(ex);
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    handleError(ex);
    res = ex.code();
  } catch (std::bad_alloc const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "caught memory exception in " << name() << ": " << ex.what();
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    Exception err(TRI_ERROR_OUT_OF_MEMORY, ex.what(), __FILE__, __LINE__);
    handleError(err);
    res = TRI_ERROR_OUT_OF_MEMORY;
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "caught exception in " << name() << ": " << ex.what();
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    Exception err(TRI_ERROR_INTERNAL, ex.what(), __FILE__, __LINE__);
    handleError(err);
    res = TRI_ERROR_INTERNAL;
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "caught unknown exception in " << name();
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    Exception err(TRI_ERROR_INTERNAL, __FILE__, __LINE__);
    handleError(err);
    res = TRI_ERROR_INTERNAL;
  }

  RequestStatistics::SET_REQUEST_END(_statistics);

  if (res == TRI_ERROR_NO_ERROR) {
    _state = HandlerState::DONE;
  } else {
    _state = HandlerState::FAILED;
    _callback(this);
  }

  return res;
}

int RestHandler::executeEngine() {
  TRI_ASSERT(ExecContext::CURRENT == nullptr);
  ExecContext* exec = static_cast<ExecContext*>(_request->requestContext());
  ExecContextScope scope(exec);
  try {
    RestStatus result = execute();

    if (result == RestStatus::FAIL) {
      LOG_TOPIC(WARN, Logger::REQUESTS) << "Rest handler reported fail";
    } else if (result == RestStatus::WAITING) {
      _state = HandlerState::PAUSED;  // wait for someone to continue the state
                                      // machine
      return TRI_ERROR_NO_ERROR;
    } else if (_response == nullptr) {
      Exception err(TRI_ERROR_INTERNAL, "no response received from handler",
                    __FILE__, __LINE__);

      handleError(err);
    }

    _state = HandlerState::FINALIZE;
    _callback(this);
    return TRI_ERROR_NO_ERROR;
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
    Exception err(TRI_ERROR_INTERNAL, std::string("VPack error: ") + ex.what(),
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

  _state = HandlerState::FAILED;
  _callback(this);
  return TRI_ERROR_INTERNAL;
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

    try {
      TRI_ASSERT(options.escapeUnicode);
      if (_request != nullptr) {
        _response->setContentType(_request->contentTypeResponse());
      }
      _response->setPayload(std::move(buffer), true, options);
    } catch (...) {
      // exception while generating error
    }
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
