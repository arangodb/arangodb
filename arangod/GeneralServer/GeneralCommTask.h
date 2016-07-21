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

#include "Basics/Mutex.h"
#include "Basics/StringBuffer.h"
#include "Basics/WorkItem.h"

#include <deque>

namespace arangodb {
class HttpRequest;
class HttpResponse;

namespace rest {
class GeneralServer;

class GeneralCommTask : public SocketTask, public RequestStatisticsAgent {
  GeneralCommTask(GeneralCommTask const&) = delete;
  GeneralCommTask const& operator=(GeneralCommTask const&) = delete;

 public:
  static size_t const MaximalHeaderSize;
  static size_t const MaximalBodySize;
  static size_t const MaximalPipelineSize;
  static size_t const RunCompactEvery;

 public:
  GeneralCommTask(GeneralServer*, TRI_socket_t, ConnectionInfo&&,
                  double keepAliveTimeout);

 protected:
  ~GeneralCommTask();

 public:
  // return whether or not the task desires to start a dispatcher thread
  bool startThread() const { return _startThread; }

  // handles response
  void handleResponse(HttpResponse*);

  // handles simple errors
  void handleSimpleError(GeneralResponse::ResponseCode);
  void handleSimpleError(GeneralResponse::ResponseCode, int code,
                         std::string const& errorMessage);

  // reads data from the socket
  bool processRead();

  // sends more chunked data
  void sendChunk(basics::StringBuffer*);

  // chunking is finished
  void finishedChunked();

  // task set up complete
  void setupDone();

 private:
  // returns the authentication realm
  std::string authenticationRealm() const;

  // checks the authentication
  GeneralResponse::ResponseCode authenticateRequest();

  // reads data from the socket
  void addResponse(HttpResponse*);

  // check the content-length header of a request and fail it is broken
  bool checkContentLength(bool expectContentLength);

  // fills the write buffer
  void fillWriteBuffer();

  // handles CORS options
  void processCorsOptions();

  // processes a request
  void processRequest();

  // clears the request object
  void clearRequest();

  // resets the internal state
  //
  // this method can be called to clean up when the request handling aborts
  // prematurely
  void resetState(bool close);

 protected:
  bool setup(Scheduler* scheduler, EventLoop loop) override;
  void cleanup() override;
  bool handleEvent(EventToken token, EventType events) override;
  void signalTask(TaskData*) override;

 protected:
  bool handleRead() override;
  void completedWriteBuffer() override;
  void handleTimeout() override;

 protected:
  // connection info
  ConnectionInfo _connectionInfo;

  // the underlying server
  GeneralServer* const _server;

  // allow method override
  bool _allowMethodOverride;

  char const* _protocol;

 private:
  // write buffers
  std::deque<basics::StringBuffer*> _writeBuffers;

  // statistics buffers
  std::deque<TRI_request_statistics_t*> _writeBuffersStats;

  // current read position
  size_t _readPosition;

  // start of the body position
  size_t _bodyPosition;

  // body length
  size_t _bodyLength;

  // true if request is complete but not handled
  bool _requestPending;

  // true if a close has been requested by the client
  bool _closeRequested;

  // true if reading the request body
  bool _readRequestBody;

  // whether or not to allow credentialed requests (only CORS)
  bool _denyCredentials;

  // whether the client accepts deflate algorithm
  bool _acceptDeflate;

  // new request started
  bool _newRequest;

  // true if within a chunked response
  bool _isChunked;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief start a separate thread if the task is added to the dispatcher?
  //////////////////////////////////////////////////////////////////////////////

  bool _startThread;

  // the request with possible incomplete body
  HttpRequest* _request;

  // http version number used
  GeneralRequest::ProtocolVersion _httpVersion;

  // type of request (GET, POST, ...)
  GeneralRequest::RequestType _requestType;

  // value of requested URL
  std::string _fullUrl;

  // value of the HTTP origin header the client sent (if any, CORS only)
  std::string _origin;

  // start position of current request
  size_t _startPosition;

  // number of requests since last compactification
  size_t _sinceCompactification;

  // original body length
  size_t _originalBodyLength;

  // task ready
  std::atomic<bool> _setupDone;

  // authentication real
  std::string const _authenticationRealm;

};  // Commontask
}  // rest
}  // arango

#endif
