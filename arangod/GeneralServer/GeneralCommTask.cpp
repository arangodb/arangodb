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
////////////////////////////////////////////////////////////////////////////////

#include "GeneralCommTask.h"

#include "Basics/HybridLogicalClock.h"
#include "Basics/MutexLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/RestHandler.h"
#include "GeneralServer/RestHandlerFactory.h"
#include "Logger/Logger.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "VocBase/server.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

GeneralCommTask::GeneralCommTask(GeneralServer* server, TRI_socket_t socket,
                                 ConnectionInfo&& info, double keepAliveTimeout)
    : Task("GeneralCommTask"),
      SocketTask(socket, keepAliveTimeout),
      _server(server),
      _connectionInfo(std::move(info)) {
  LOG(TRACE) << "connection established, client "
             << TRI_get_fd_or_handle_of_socket(socket) << ", server ip "
             << _connectionInfo.serverAddress << ", server port "
             << _connectionInfo.serverPort << ", client ip "
             << _connectionInfo.clientAddress << ", client port "
             << _connectionInfo.clientPort;
}

GeneralCommTask::~GeneralCommTask() {
  LOG(TRACE) << "connection closed, client "
             << TRI_get_fd_or_handle_of_socket(_commSocket);

  for (auto& i : _writeBuffers) {
    delete i;
  }

  for (auto& i : _writeBuffersStats) {
    TRI_ReleaseRequestStatistics(i);
  }

  clearRequest();
}

void GeneralCommTask::signalTask(TaskData* data) {
  // data response
  if (data->_type == TaskData::TASK_DATA_RESPONSE) {
    data->RequestStatisticsAgent::transferTo(this);

    GeneralResponse* response = data->_response.get();

    if (response != nullptr) {
      processResponse(response);
      processRead();
    } else {
      handleSimpleError(GeneralResponse::ResponseCode::SERVER_ERROR);
    }
  }

  // buffer response
  else if (data->_type == TaskData::TASK_DATA_BUFFER) {
    data->RequestStatisticsAgent::transferTo(this);
    HttpResponse response(GeneralResponse::ResponseCode::OK);
    velocypack::Slice slice(data->_buffer->data());
    response.setPayload(_request, slice, true, VPackOptions::Defaults);
    processResponse(&response);
    processRead();
  }

  // data chunk
  else if (data->_type == TaskData::TASK_DATA_CHUNK) {
    handleChunk(data->_data.c_str(), data->_data.size());
  }

  // do not know, what to do - give up
  else {
    _scheduler->destroyTask(this);
  }
}

bool GeneralCommTask::handleRead() {
  bool res = true;

  if (!_closeRequested) {
    res = fillReadBuffer();

    // process as much data as we got; there might be more than one
    // request in the buffer
    while (processRead()) {
      if (_closeRequested) {
        break;
      }
    }
  } else {
    // if we don't close here, the scheduler thread may fall into a
    // busy wait state, consuming 100% CPU!
    _clientClosed = true;
  }

  if (_clientClosed) {
    res = false;
  } else if (!res) {
    _clientClosed = true;
  }

  return res;
}

void GeneralCommTask::executeRequest(GeneralRequest* request,
                                     GeneralResponse* response) {
  // create a handler for this request
  WorkItem::uptr<RestHandler> handler(
      GeneralServerFeature::HANDLER_FACTORY->createHandler(request, response));

  if (handler == nullptr) {
    LOG(TRACE) << "no handler is known, giving up";

    clearRequest();
    delete response;

    handleSimpleError(GeneralResponse::ResponseCode::NOT_FOUND);
    return;
  }

  handler->setTaskId(_taskId, _loop);

  // check for an async request
  bool found = false;
  std::string const& asyncExecution =
      request->header(StaticStrings::Async, found);

  // the responsibility for the request has been moved to the handler
  // TODO(fc) _request should
  _request = nullptr;

  // async execution
  bool ok = false;

  if (found && (asyncExecution == "true" || asyncExecution == "store")) {
    requestStatisticsAgentSetAsync();
    uint64_t jobId = 0;

    if (asyncExecution == "store") {
      // persist the responses
      ok = _server->handleRequestAsync(this, handler, &jobId);
    } else {
      // don't persist the responses
      ok = _server->handleRequestAsync(this, handler, nullptr);
    }

    if (ok) {
      HttpResponse response(GeneralResponse::ResponseCode::ACCEPTED);

      if (jobId > 0) {
        // return the job id we just created
        response.setHeaderNC(StaticStrings::AsyncId, StringUtils::itoa(jobId));
      }

      processResponse(&response);

      return;
    }
  }

  // synchronous request
  else {
    ok = _server->handleRequest(this, handler);
  }

  if (!ok) {
    handleSimpleError(GeneralResponse::ResponseCode::SERVER_ERROR);
  }
}

void GeneralCommTask::processResponse(GeneralResponse* response) {
  if (response == nullptr) {
    handleSimpleError(GeneralResponse::ResponseCode::SERVER_ERROR);
  } else {
    addResponse(response, false);
  }
}

void GeneralCommTask::handleSimpleError(GeneralResponse::ResponseCode code) {
  HttpResponse response(code);
  addResponse(&response, true);
}

void GeneralCommTask::handleSimpleError(
    GeneralResponse::ResponseCode responseCode, int errorNum,
    std::string const& errorMessage) {
  HttpResponse response(responseCode);

  VPackBuilder builder;
  builder.openObject();
  builder.add(StaticStrings::Error, VPackValue(true));
  builder.add(StaticStrings::ErrorNum, VPackValue(errorNum));
  builder.add(StaticStrings::ErrorMessage, VPackValue(errorMessage));
  builder.add(StaticStrings::Code, VPackValue((int)responseCode));
  builder.close();

  try {
    response.setPayload(_request, builder.slice(), true,
                        VPackOptions::Defaults);
    clearRequest();
    processResponse(&response);
  } catch (...) {
    addResponse(&response, true);
  }
}

// TODO(fc) MOVE TO SOCKET TASK
bool GeneralCommTask::handleEvent(EventToken token, EventType events) {
  bool result = SocketTask::handleEvent(token, events);
  if (_clientClosed) _scheduler->destroyTask(this);
  return result;
}
