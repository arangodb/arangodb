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
          LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "Pausing rest handler execution";
          return; // stop state machine
        }
        break;
      }
        
      case HandlerState::PAUSED:
        LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "Resuming rest handler execution";
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
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "caught exception in " << name() << ": "
             << DIAGNOSTIC_INFORMATION(ex);
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    handleError(ex);
    res = ex.code();
  } catch (std::bad_alloc const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "caught memory exception in " << name() << ": " << ex.what();
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    Exception err(TRI_ERROR_OUT_OF_MEMORY, ex.what(), __FILE__, __LINE__);
    handleError(err);
    res = TRI_ERROR_OUT_OF_MEMORY;
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "caught exception in " << name() << ": " << ex.what();
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    Exception err(TRI_ERROR_INTERNAL, ex.what(), __FILE__, __LINE__);
    handleError(err);
    res = TRI_ERROR_INTERNAL;
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "caught unknown exception in " << name();
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
      _state = HandlerState::PAUSED; // wait for someone to continue the state machine
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
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "caught exception in " << name() << ": "
              << DIAGNOSTIC_INFORMATION(ex);
#endif
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    handleError(ex);
  } catch (arangodb::velocypack::Exception const& ex) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "caught velocypack exception in " << name() << ": "
              << DIAGNOSTIC_INFORMATION(ex);
#endif
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    Exception err(TRI_ERROR_INTERNAL, std::string("VPack error: ") + ex.what(),
                  __FILE__, __LINE__);
    handleError(err);
  } catch (std::bad_alloc const& ex) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "caught memory exception in " << name() << ": "
              << DIAGNOSTIC_INFORMATION(ex);
#endif
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    Exception err(TRI_ERROR_OUT_OF_MEMORY, ex.what(), __FILE__, __LINE__);
    handleError(err);
  } catch (std::exception const& ex) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "caught exception in " << name() << ": "
              << DIAGNOSTIC_INFORMATION(ex);
#endif
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    Exception err(TRI_ERROR_INTERNAL, ex.what(), __FILE__, __LINE__);
    handleError(err);
  } catch (...) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "caught unknown exception in " << name();
#endif
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    Exception err(TRI_ERROR_INTERNAL, __FILE__, __LINE__);
    handleError(err);
  }

  _state = HandlerState::FAILED;
  _callback(this);
  return TRI_ERROR_INTERNAL;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

void RestHandler::resetResponse(rest::ResponseCode code) {
  TRI_ASSERT(_response != nullptr);
  _response->reset(code);
}
