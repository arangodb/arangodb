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
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GENERAL_SERVER_GENERAL_COMM_TASK_H
#define ARANGOD_GENERAL_SERVER_GENERAL_COMM_TASK_H 1

#include "Scheduler/SocketTask.h"

#include <openssl/ssl.h>

#include "Basics/Mutex.h"
#include "Basics/StringBuffer.h"
#include "Basics/WorkItem.h"

namespace arangodb {
class GeneralRequest;
class GeneralResponse;

namespace rest {
class GeneralServer;

//
// The flow of events is as follows:
//
// (1) As soon as new data is available from the client, then `handleRead()` is
//     called. This will read new data from the client using
//     `SocketTask::fillReadBuffer()`.
//
// (2) After reading data from the client, `processRead()` is called. Each
//     sub-class of `GeneralCommTask` must implement this method. While the
//     function returns true it is called in a loop. Returning false signals
//     that new data has to be received in order to continue and that all
//     available data has been processed
//
// (3) As soon as `processRead()` detects that it has read a complete request,
//     it must create an instance of a sub-class of `GeneralRequest` and
//     `GeneralResponse`. Then it must call `executeRequest(...)` to start the
//     execution of the request.
//
// (4) `executeRequest(...)` will create a handler. A handler is responsible for
//     executing the request. It will take the `request` instance and executes a
//     plan to generate a response. It is possible, that one request generates a
//     response and still does some work afterwards. It is even possible, that a
//     request generates a push stream.
//
//     As soon as a response is available, `handleResponse()` will be called.
//     This in turn calls `addResponse()` which must be implemented in the
//     sub-class. It will be called with an response object and an indicator
//     if an error has occurred.
//
//     It is the responsibility of the sub-class to govern what is supported.
//     For example, HTTP will only support one active request executing at a
//     time until the final response has been send out.
//
//     VelocyPack on the other hand, allows multiple active requests. Partial
//     responses are identified by a request id.
//
// (5) Error handling: In case of an error `handleSimpleError()` will be
//     called. This will call `addResponse()` with an error indicator, which in
//     turn will end the responding request.
//
// External Interface (called from Scheduler):
//
// (1) handleRead
//
//     Will be called when new data can be read from the client. It will
//     use `SocketTask::fillReadBuffer()` to actually read the data into the
//     read buffer (defined in `SocketTask`).
//
// (2) signalTask
//
//     `signalTask` will be called when data becomes available from an
//     asynchronous execution.
//
// (3) addResponse
//
//     see below.
//
// Internal Interface (must be implemented by sub-classes):
//
// (1) processRead
//
//     Will be called as soon as new data has been read from the client.  The
//     method must split the read buffer into requests and call `executeRequest`
//     for each request found. It is the responsiblity of the `processRead` to
//     cleanup the read buffer periodically.
//
// (2) addResponse
//
//     Will be called when a new response is available.
//
// Ownership and life-time:
//
// (1) The Task will live as long as there is at least one active handler.
//     TODO(fc)
//
// (2) The Task owns the handlers and is responsible for destroying them.
//     TODO(fc)
//

// handleEvent (defined in SocketTask and arumented in this class) is called
// when new data is available. handleEvent calls in turn handleWrite and
// handleRead (virtual function required by SocketTask) that calls processRead
// (which has to be implemented in derived) as long as new input is available.

class GeneralCommTask : public SocketTask {
  friend class GeneralServer;

  GeneralCommTask(GeneralCommTask const&) = delete;
  GeneralCommTask const& operator=(GeneralCommTask const&) = delete;

 public:
  GeneralCommTask(GeneralServer*, TRI_socket_t, ConnectionInfo&&,
                  double keepAliveTimeout);

  virtual void addResponse(GeneralResponse*) = 0;
  virtual arangodb::Endpoint::TransportType transportType() = 0;

  RequestStatisticsAgent* getAgent(uint64_t id) {
    auto agentIt = _agents.find(id);
    if (agentIt != _agents.end()) {
      return &(agentIt->second);
    } else {
      throw std::logic_error("there should be an agent for every request");
      //LOG(DEBUG) << "accessing already removed id";
      //return &_agents[id];
    }
  }

 protected:
  virtual void handleChunk(char const*, size_t) = 0;

  virtual std::unique_ptr<GeneralResponse> createResponse(
      rest::ResponseCode, uint64_t messageId) = 0;

 protected:
  void executeRequest(std::unique_ptr<GeneralRequest>&&,
                      std::unique_ptr<GeneralResponse>&&);

  void processResponse(GeneralResponse*);

 public:
  virtual void handleSimpleError(rest::ResponseCode, uint64_t messagid) = 0;
  virtual void handleSimpleError(rest::ResponseCode, int code,
                                 std::string const& errorMessage,
                                 uint64_t messageId) = 0;

 private:
  void handleTimeout() override final { _clientClosed = true; }
  void signalTask(TaskData*) override;

 protected:
  // for asynchronous requests
  GeneralServer* const _server;

  // protocol to use http, vpp
  char const* _protocol = "unknown";

  rest::ProtocolVersion _protocolVersion = rest::ProtocolVersion::UNKNOWN;

  std::unordered_map<uint64_t, RequestStatisticsAgent> _agents;
};
}
}

#endif
