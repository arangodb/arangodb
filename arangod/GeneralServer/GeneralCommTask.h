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

#ifndef ARANGOD_HTTP_SERVER_HTTP_COMM_TASK_H
#define ARANGOD_HTTP_SERVER_HTTP_COMM_TASK_H 1

#include "Scheduler/SocketTask.h"

#include <openssl/ssl.h>

#include "Basics/Mutex.h"
#include "Basics/StringBuffer.h"
#include "Basics/WorkItem.h"

#include <deque>
namespace arangodb {
class GeneralRequest;
class GeneralResponse;

namespace rest {
class GeneralServer;

class GeneralCommTask : public SocketTask, public RequestStatisticsAgent {
  GeneralCommTask(GeneralCommTask const&) = delete;
  GeneralCommTask const& operator=(GeneralCommTask const&) = delete;

 public:
  GeneralCommTask(GeneralServer*, TRI_socket_t, ConnectionInfo&&,
                  double keepAliveTimeout);

  void handleResponse(GeneralResponse*);             // called by server

  void handleSimpleError(GeneralResponse::ResponseCode);
  void handleSimpleError(GeneralResponse::ResponseCode, int code,
                         std::string const& errorMessage);

 protected:
  virtual ~GeneralCommTask();

  virtual void addResponse(GeneralResponse*) = 0;
  virtual bool processRead() = 0;
  virtual void processRequest() = 0;
  virtual void resetState(bool) = 0;

  virtual bool handleEvent(EventToken token,
                           EventType events) override;  // called by TODO

  void cleanup() override final { SocketTask::cleanup(); }

  // clears the request object
  void clearRequest() {
    delete _request;
    _request = nullptr;
  }

 private:
  void handleTimeout() override final;

 protected:
  GeneralServer* const _server;
  GeneralRequest* _request;  // the request with possible incomplete body
  ConnectionInfo _connectionInfo;
  char const* _protocol;  // protocal to use http, vpp
  GeneralRequest::ProtocolVersion _protocolVersion;
  std::deque<basics::StringBuffer*> _writeBuffers;
  std::deque<TRI_request_statistics_t*>
      _writeBuffersStats;  // statistics buffers
  bool _isChunked;         // true if within a chunked response
  bool _requestPending;    // true if request is complete but not handled
};
}
}

#endif
