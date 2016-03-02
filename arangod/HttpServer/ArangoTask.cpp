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
////////////////////////////////////////////////////////////////////////////////

#include "ArangoTask.h"

#include "Basics/MutexLocker.h"
#include "Basics/StringBuffer.h"
#include "Basics/logging.h"
#include "HttpServer/GeneralHandler.h"
#include "HttpServer/GeneralHandlerFactory.h"
#include "HttpServer/GeneralServer.h"
#include "Scheduler/Scheduler.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief static initializers
////////////////////////////////////////////////////////////////////////////////

size_t const ArangoTask::MaximalHeaderSize = 1 * 1024 * 1024;       //   1 MB
size_t const ArangoTask::MaximalBodySize = 512 * 1024 * 1024;       // 512 MB
size_t const ArangoTask::MaximalPipelineSize = 1024 * 1024 * 1024;  //   1 GB

ArangoTask::ArangoTask(GeneralServer* server, TRI_socket_t socket,
                           ConnectionInfo const& info, double keepAliveTimeout, std::string task, 
                           	GeneralRequest::ProtocolVersion	version, GeneralRequest::RequestType request)
		: Task(task),
		  SocketTask(socket, keepAliveTimeout),
	      _connectionInfo(info),
	      _server(server),
	      _writeBuffers(),
	      _writeBuffersStats(),
	      _bodyLength(0),
	      _requestPending(false),
	      _closeRequested(false),
	      _readRequestBody(false),
	      _denyCredentials(false),
	      _acceptDeflate(false),
	      _newRequest(true),
	      _isChunked(false),
	      _request(nullptr),
	      _httpVersion(GeneralRequest::HTTP_UNKNOWN),
	      _requestType(GeneralRequest::HTTP_REQUEST_ILLEGAL),
	      _fullUrl(),
	      _origin(),
	      _sinceCompactification(0),
	      _originalBodyLength(0),
	      _setupDone(false) {
  LOG_TRACE(
      "connection established, client %d, server ip %s, server port %d, client "
      "ip %s, client port %d",
      (int)TRI_get_fd_or_handle_of_socket(socket),
      _connectionInfo.serverAddress.c_str(), (int)_connectionInfo.serverPort,
      _connectionInfo.clientAddress.c_str(), (int)_connectionInfo.clientPort);

  // acquire a statistics entry and set the type to HTTP/VStream
  ConnectionStatisticsAgent::acquire();
  connectionStatisticsAgentSetStart();
  // connectionStatisticsAgentSetHttp(); @TODO, Implement this in HttpCommTask
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a task
////////////////////////////////////////////////////////////////////////////////

ArangoTask::~ArangoTask() {
  LOG_TRACE("connection closed, client %d",
            (int)TRI_get_fd_or_handle_of_socket(_commSocket));

  // free write buffers and statistics
  for (auto& i : _writeBuffers) {
    delete i;
  }

  for (auto& i : _writeBuffersStats) {
    TRI_ReleaseRequestStatistics(i);
  }

  // free request
  delete _request;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles response
////////////////////////////////////////////////////////////////////////////////

void ArangoTask::handleResponse(GeneralResponse* response) {
  if (response->isChunked()) {
    _requestPending = true;
    _isChunked = true;
  } else {
    _requestPending = false;
    _isChunked = false;
  }

  addResponse(response);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sends more chunked data
////////////////////////////////////////////////////////////////////////////////

void ArangoTask::sendChunk(StringBuffer* buffer) {
  if (_isChunked) {
    TRI_ASSERT(buffer != nullptr);

    _writeBuffers.push_back(buffer);
    _writeBuffersStats.push_back(nullptr);

    fillWriteBuffer();
  } else {
    delete buffer;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief chunking is finished
////////////////////////////////////////////////////////////////////////////////

void ArangoTask::finishedChunked() {
  auto buffer = std::make_unique<StringBuffer>(TRI_UNKNOWN_MEM_ZONE, 6);
  buffer->appendText(TRI_CHAR_LENGTH_PAIR("0\r\n\r\n"));

  _writeBuffers.push_back(buffer.get());
  buffer.release();
  _writeBuffersStats.push_back(nullptr);

  _isChunked = false;
  _requestPending = false;

  fillWriteBuffer();
  processRead();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief task set up complete
////////////////////////////////////////////////////////////////////////////////

void ArangoTask::setupDone() {
  _setupDone.store(true, std::memory_order_relaxed);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clears the request object
////////////////////////////////////////////////////////////////////////////////

void ArangoTask::clearRequest() {
  delete _request;
  _request = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief decides whether or not we should send back a www-authenticate header
////////////////////////////////////////////////////////////////////////////////

bool ArangoTask::sendWwwAuthenticateHeader() const {
  bool found;
  _request->header("x-omit-www-authenticate", found);

  return !found;
}


bool ArangoTask::setup(Scheduler* scheduler, EventLoop loop) {
  bool ok = SocketTask::setup(scheduler, loop);

  if (!ok) {
    return false;
  }

  _scheduler = scheduler;
  _loop = loop;
  
  setupDone();

  return true;
}

void ArangoTask::cleanup() { SocketTask::cleanup(); }

bool ArangoTask::handleEvent(EventToken token, EventType events) {
  bool result = SocketTask::handleEvent(token, events);

  if (_clientClosed) {
    _scheduler->destroyTask(this);
  }

  return result;
}

void ArangoTask::handleTimeout() {
  _clientClosed = true;
  _server->handleCommunicationClosed(this);
}