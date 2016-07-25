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

////////////////////////////////////////////////////////////////////////////////
/// @brief static initializers
////////////////////////////////////////////////////////////////////////////////

GeneralCommTask::GeneralCommTask(GeneralServer* server, TRI_socket_t socket,
                                 ConnectionInfo&& info, double keepAliveTimeout)
    : Task("GeneralCommTask"),
      SocketTask(socket, keepAliveTimeout),
      _server(server),
      _request(nullptr),
      _connectionInfo(std::move(info)),
      _protocol("unknown"),
      _protocolVersion(GeneralRequest::ProtocolVersion::UNKNOWN),
      _writeBuffers(),
      _writeBuffersStats(),
      _isChunked(false),
      _requestPending(false) {
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

  // free write buffers and statistics
  for (auto& i : _writeBuffers) delete i;
  for (auto& i : _writeBuffersStats) TRI_ReleaseRequestStatistics(i);

  // free request
  delete _request;
}

void GeneralCommTask::handleResponse(GeneralResponse* response) {
  _requestPending = false;
  _isChunked = false;

  if (response == nullptr) {
    handleSimpleError(GeneralResponse::ResponseCode::SERVER_ERROR);
  } else {
    addResponse(response);
  }
}

void GeneralCommTask::handleSimpleError(GeneralResponse::ResponseCode code) {
  HttpResponse response(code);
  resetState(true);
  addResponse(&response);
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
    handleResponse(&response);
  } catch (...) {
    resetState(true);
    addResponse(&response);
  }
}

bool GeneralCommTask::handleEvent(EventToken token, EventType events) {
  bool result = SocketTask::handleEvent(token, events);
  if (_clientClosed) _scheduler->destroyTask(this);
  return result;
}

void GeneralCommTask::handleTimeout() { _clientClosed = true; }
