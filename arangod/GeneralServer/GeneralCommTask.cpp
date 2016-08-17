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
#include "Meta/conversion.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/RestHandler.h"
#include "GeneralServer/RestHandlerFactory.h"
#include "Logger/Logger.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Rest/VppResponse.h"

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
}

void GeneralCommTask::signalTask(TaskData* data) {
  // used to output text

  // data response
  if (data->_type == TaskData::TASK_DATA_RESPONSE) {
    data->RequestStatisticsAgent::transferTo(this);

    GeneralResponse* response = data->_response.get();

    if (response != nullptr) {
      processResponse(response);
      processRead();
    } else {
      handleSimpleError(GeneralResponse::ResponseCode::SERVER_ERROR, 0);
    }
  }

  // buffer response
  else if (data->_type == TaskData::TASK_DATA_BUFFER) {
    std::unique_ptr<GeneralResponse> response;
    if (transportType() == Endpoint::TransportType::VPP) {
      response = std::unique_ptr<VppResponse>(new VppResponse(
          GeneralResponse::ResponseCode::OK, 0 /*id unset FIXME?*/));
    } else {
      response = std::unique_ptr<HttpResponse>(
          new HttpResponse(GeneralResponse::ResponseCode::OK));
    }

    data->RequestStatisticsAgent::transferTo(this);
    velocypack::Slice slice(data->_buffer->data());

    // FIXME (obi) contentType - text set header/meta information?
    response->setPayload(slice, true, VPackOptions::Defaults);
    processResponse(response.get());
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

void GeneralCommTask::fillWriteBuffer() {
  if (!hasWriteBuffer() && !_writeBuffers.empty()) {
    StringBuffer* buffer = _writeBuffers.front();
    _writeBuffers.pop_front();

    TRI_ASSERT(buffer != nullptr);

    // REVIEW (fc)
    TRI_request_statistics_t* statistics = nullptr;
    if (!_writeBuffersStats.empty()) {
      statistics = _writeBuffersStats.front();
      _writeBuffersStats.pop_front();
    }

    setWriteBuffer(buffer, statistics);
  }
}

void GeneralCommTask::executeRequest(GeneralRequest* request,
                                     GeneralResponse* response) {
  WorkItem::uptr<RestHandler> handler(
      GeneralServerFeature::HANDLER_FACTORY->createHandler(request, response));

  if (handler == nullptr) {
    LOG(TRACE) << "no handler is known, giving up";

    httpClearRequest();
    delete response;

    handleSimpleError(GeneralResponse::ResponseCode::NOT_FOUND,
                      request->messageId());
    return;
  }

  handler->setTaskId(_taskId, _loop);

  // check for an async request
  bool found = false;
  std::string const& asyncExecution =
      request->header(StaticStrings::Async, found);

  // TODO(fc)
  // the responsibility for the request has been moved to the handler
  // so we give up ownage here by setting _request = nullptr
  httpNullRequest();  // http specific - should be removed FIXME

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
    handleSimpleError(GeneralResponse::ResponseCode::SERVER_ERROR,
                      request->messageId());
  }
}

void GeneralCommTask::processResponse(GeneralResponse* response) {
  if (response == nullptr) {
    handleSimpleError(GeneralResponse::ResponseCode::SERVER_ERROR,
                      response->messageId());
  } else {
    addResponse(response, false);
  }
}

// TODO(fc) MOVE TO SOCKET TASK
bool GeneralCommTask::handleEvent(EventToken token, EventType events) {
  // destroy this task if client is closed
  bool result = SocketTask::handleEvent(token, events);
  if (_clientClosed) {
    _scheduler->destroyTask(this);
  }
  return result;
}
