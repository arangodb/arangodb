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
      _statistics(nullptr) {
  bool found;
  std::string const& startThread =
      _request->header(StaticStrings::StartThread, found);

  if (found) {
    _needsOwnThread = StringUtils::boolean(startThread);
  }

  _context =
      std::make_shared<WorkContext>(_request->user(), _request->databaseName());
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

int RestHandler::prepareEngine() {
  RequestStatistics::SET_REQUEST_START(_statistics);

  // set end immediately so we do not get netative statistics
  RequestStatistics::SET_REQUEST_END(_statistics);

  ExecContext::CURRENT_EXECCONTEXT = _request->execContext();

  if (ExecContext::CURRENT_EXECCONTEXT == nullptr) {
    std::cout << "ExecContext::CURRENT_EXECCONTEXT is nullptr :S \n";
  } else {
    std::cout << "CURRENT_EXECCONTEXT != nullptr\n";
  }

  if (_canceled) {
    _engine.setState(RestEngine::State::DONE);
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);

    Exception err(TRI_ERROR_REQUEST_CANCELED,
                  "request has been canceled by user", __FILE__, __LINE__);
    handleError(err);

    _storeResult(this);
    return TRI_ERROR_REQUEST_CANCELED;
  }

  try {
    prepareExecute();
    _engine.setState(RestEngine::State::EXECUTE);
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

  _engine.setState(RestEngine::State::FAILED);
  _storeResult(this);
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
    _engine.setState(RestEngine::State::DONE);
  } else {
    _engine.setState(RestEngine::State::FAILED);
    _storeResult(this);
  }

  ExecContext::CURRENT_EXECCONTEXT = nullptr;

  return res;
}

int RestHandler::executeEngine() {
  try {
    RestStatus result = execute();

    if (result.isLeaf()) {
      if (_response == nullptr) {
        Exception err(TRI_ERROR_INTERNAL, "no response received from handler",
                      __FILE__, __LINE__);

        handleError(err);
      }

      _engine.setState(RestEngine::State::FINALIZE);
      _storeResult(this);
      return TRI_ERROR_NO_ERROR;
    }

    _engine.setState(RestEngine::State::RUN);
    _engine.appendRestStatus(result.element());

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

  _engine.setState(RestEngine::State::FAILED);
  _storeResult(this);
  return TRI_ERROR_INTERNAL;
}

int RestHandler::runEngine(bool synchron) {
  try {
    while (_engine.hasSteps()) {
      std::shared_ptr<RestStatusElement> result = _engine.popStep();

      switch (result->state()) {
        case RestStatusElement::State::DONE:
          _engine.setState(RestEngine::State::FINALIZE);
          _storeResult(this);
          break;

        case RestStatusElement::State::FAIL:
          _engine.setState(RestEngine::State::FINALIZE);
          _storeResult(this);
          return TRI_ERROR_NO_ERROR;

        case RestStatusElement::State::WAIT_FOR:
          if (!synchron) {
            _engine.setState(RestEngine::State::WAITING);

            std::shared_ptr<RestHandler> self = shared_from_this();
            result->callWaitFor([self, this]() {
              _engine.setState(RestEngine::State::RUN);
              _engine.asyncRun(self);
            });

            return TRI_ERROR_NO_ERROR;
          }

          return TRI_ERROR_INTERNAL;

        case RestStatusElement::State::QUEUED:
          if (!synchron) {
            std::shared_ptr<RestHandler> self = shared_from_this();
            _engine.queue([self, this]() { _engine.asyncRun(self); });
            return TRI_ERROR_NO_ERROR;
          }
          break;

        case RestStatusElement::State::THEN: {
          auto status = result->callThen();

          if (status != nullptr) {
            _engine.appendRestStatus(status->element());
          }

          break;
        }
      }
    }

    return TRI_ERROR_NO_ERROR;
  } catch (Exception const& ex) {
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    handleError(ex);
  } catch (arangodb::velocypack::Exception const& ex) {
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    Exception err(TRI_ERROR_INTERNAL, std::string("VPack error: ") + ex.what(),
                  __FILE__, __LINE__);
    handleError(err);
  } catch (std::bad_alloc const& ex) {
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    Exception err(TRI_ERROR_OUT_OF_MEMORY, ex.what(), __FILE__, __LINE__);
    handleError(err);
  } catch (std::exception const& ex) {
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    Exception err(TRI_ERROR_INTERNAL, ex.what(), __FILE__, __LINE__);
    handleError(err);
  } catch (...) {
    RequestStatistics::SET_EXECUTE_ERROR(_statistics);
    Exception err(TRI_ERROR_INTERNAL, __FILE__, __LINE__);
    handleError(err);
  }

  _engine.setState(RestEngine::State::FAILED);
  _storeResult(this);
  return TRI_ERROR_INTERNAL;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

void RestHandler::resetResponse(rest::ResponseCode code) {
  TRI_ASSERT(_response != nullptr);
  _response->reset(code);
}
