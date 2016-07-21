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

#include "Basics/StringUtils.h"
#include "Dispatcher/Dispatcher.h"
#include "Logger/Logger.h"
#include "Rest/GeneralRequest.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

namespace {
std::atomic_uint_fast64_t NEXT_HANDLER_ID(
    static_cast<uint64_t>(TRI_microtime() * 100000.0));
}

RestHandler::RestHandler(GeneralRequest* request, GeneralResponse* response)
    : _handlerId(NEXT_HANDLER_ID.fetch_add(1, std::memory_order_seq_cst)),
      _taskId(0),
      _request(request),
      _response(response) {}

RestHandler::~RestHandler() {
  delete _request;
  delete _response;
}

void RestHandler::setTaskId(uint64_t id, EventLoop loop) {
  _taskId = id;
  _loop = loop;
}

RestHandler::status RestHandler::executeFull() {
  RestHandler::status result = status::FAILED;

  requestStatisticsAgentSetRequestStart();

#ifdef USE_DEV_TIMERS
  TRI_request_statistics_t::STATS = _statistics;
#endif

  try {
    prepareExecute();

    try {
      result = execute();
    } catch (Exception const& ex) {
      requestStatisticsAgentSetExecuteError();
      handleError(ex);
    } catch (std::bad_alloc const& ex) {
      requestStatisticsAgentSetExecuteError();
      Exception err(TRI_ERROR_OUT_OF_MEMORY, ex.what(), __FILE__, __LINE__);
      handleError(err);
    } catch (std::exception const& ex) {
      requestStatisticsAgentSetExecuteError();
      Exception err(TRI_ERROR_INTERNAL, ex.what(), __FILE__, __LINE__);
      handleError(err);
    } catch (...) {
      requestStatisticsAgentSetExecuteError();
      Exception err(TRI_ERROR_INTERNAL, __FILE__, __LINE__);
      handleError(err);
    }

    finalizeExecute();

    if (result != status::ASYNC && _response == nullptr) {
      Exception err(TRI_ERROR_INTERNAL, "no response received from handler",
                    __FILE__, __LINE__);

      handleError(err);
    }
  } catch (Exception const& ex) {
    result = status::FAILED;
    requestStatisticsAgentSetExecuteError();
    LOG(ERR) << "caught exception: " << DIAGNOSTIC_INFORMATION(ex);
  } catch (std::exception const& ex) {
    result = status::FAILED;
    requestStatisticsAgentSetExecuteError();
    LOG(ERR) << "caught exception: " << ex.what();
  } catch (...) {
    result = status::FAILED;
    requestStatisticsAgentSetExecuteError();
    LOG(ERR) << "caught exception";
  }

  requestStatisticsAgentSetRequestEnd();

#ifdef USE_DEV_TIMERS
  TRI_request_statistics_t::STATS = nullptr;
#endif

  return result;
}

GeneralRequest* RestHandler::stealRequest() {
  GeneralRequest* tmp = _request;
  _request = nullptr;
  return tmp;
}

GeneralResponse* RestHandler::stealResponse() {
  GeneralResponse* tmp = _response;
  _response = nullptr;
  return tmp;
}

void RestHandler::setResponseCode(GeneralResponse::ResponseCode code) {
  TRI_ASSERT(_response != nullptr);
  _response->reset(code);
}
