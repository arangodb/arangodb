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
/// @author Achim Brandt
/// @author Dr. Frank Celler
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "GeneralCommTask.h"

#include "Basics/HybridLogicalClock.h"
#include "Basics/MutexLocker.h"
#include "Basics/StaticStrings.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/RestHandler.h"
#include "GeneralServer/RestHandlerFactory.h"
#include "Logger/Logger.h"
#include "Meta/conversion.h"
#include "Rest/VppResponse.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

GeneralCommTask::GeneralCommTask(GeneralServer* server, TRI_socket_t socket,
                                 ConnectionInfo&& info, double keepAliveTimeout)
    : Task("GeneralCommTask"),
      SocketTask(socket, std::move(info), keepAliveTimeout),
      _server(server),
      _agents(){};

void GeneralCommTask::signalTask(TaskData* data) {
  // data response
  if (data->_type == TaskData::TASK_DATA_RESPONSE) {
    data->RequestStatisticsAgent::transferTo(
        getAgent(data->_response->messageId()));

    processResponse(data->_response.get());
  }

  // data chunk
  else if (data->_type == TaskData::TASK_DATA_CHUNK) {
    handleChunk(data->_data.c_str(), data->_data.size());
  }

  // do not know, what to do - give up
  else {
    _clientClosed = true;
  }

  while (processRead()) {
    if (_closeRequested) {
      break;
    }
  }

}

void GeneralCommTask::executeRequest(
    std::unique_ptr<GeneralRequest>&& request,
    std::unique_ptr<GeneralResponse>&& response) {
  // check for an async request (before the handler steals the request)
  bool found = false;
  std::string const& asyncExecution =
      request->header(StaticStrings::Async, found);

  // store the message id for error handling
  uint64_t messageId = 0UL;
  if (request) {
    messageId = request->messageId();
  } else if (response) {
    messageId = response->messageId();
  } else {
    LOG_TOPIC(WARN, Logger::COMMUNICATION)
        << "could not find corresponding request/response";
  }

  // create a handler, this takes ownership of request and response
  WorkItem::uptr<RestHandler> handler(
      GeneralServerFeature::HANDLER_FACTORY->createHandler(
          std::move(request), std::move(response)));

  if (handler == nullptr) {
    LOG(TRACE) << "no handler is known, giving up";
    handleSimpleError(rest::ResponseCode::NOT_FOUND, messageId);
    return;
  }

  handler->setTaskId(_taskId, _loop);

  // asynchronous request
  bool ok = false;

  if (found && (asyncExecution == "true" || asyncExecution == "store")) {
    getAgent(messageId)->requestStatisticsAgentSetAsync();
    uint64_t jobId = 0;

    if (asyncExecution == "store") {
      // persist the responses
      ok = _server->handleRequestAsync(this, std::move(handler), &jobId);
    } else {
      // don't persist the responses
      ok = _server->handleRequestAsync(this, std::move(handler));
    }

    if (ok) {
      std::unique_ptr<GeneralResponse> response =
          createResponse(rest::ResponseCode::ACCEPTED, messageId);

      if (jobId > 0) {
        // return the job id we just created
        response->setHeaderNC(StaticStrings::AsyncId, StringUtils::itoa(jobId));
      }

      processResponse(response.get());
      return;
    }
  }

  // synchronous request
  else {
    ok = _server->handleRequest(this, std::move(handler));
  }

  if (!ok) {
    handleSimpleError(rest::ResponseCode::SERVER_ERROR, messageId);
  }
}

void GeneralCommTask::processResponse(GeneralResponse* response) {
  if (response == nullptr) {
    LOG_TOPIC(WARN, Logger::COMMUNICATION)
        << "processResponse received a nullptr, closing connection";
    _clientClosed = true;
  } else {
    addResponse(response);
  }
}
