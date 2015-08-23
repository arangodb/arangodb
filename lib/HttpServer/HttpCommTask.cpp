////////////////////////////////////////////////////////////////////////////////
/// @brief task for communications
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "HttpCommTask.h"

#include "Basics/StringBuffer.h"
#include "Basics/logging.h"
#include "Basics/MutexLocker.h"
#include "HttpServer/HttpHandler.h"
#include "HttpServer/HttpHandlerFactory.h"
#include "HttpServer/HttpServer.h"
#include "Scheduler/Scheduler.h"

using namespace triagens::basics;
using namespace triagens::rest;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                            class AsyncChunkedTask
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new async chunked task
////////////////////////////////////////////////////////////////////////////////

AsyncChunkedTask::AsyncChunkedTask (HttpCommTask* output) 
  : Task("AsyncChunkedTask"),
    _output(output),
    _done(false),
    _data(nullptr),
    _dataLock() {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs an async chunked task
////////////////////////////////////////////////////////////////////////////////

AsyncChunkedTask::~AsyncChunkedTask () {
  if (_data != nullptr) {
    delete _data;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a chunk and signal next part
////////////////////////////////////////////////////////////////////////////////

int AsyncChunkedTask::signalChunk (const string& data) {
  {
    MUTEX_LOCKER(_dataLock);

    if (data.empty()) {
      _done = true;
    }
    else {
      if (_data == nullptr) {
        _data = new StringBuffer(TRI_UNKNOWN_MEM_ZONE, data.size());
      }

      if (_data == nullptr) {
        return TRI_ERROR_OUT_OF_MEMORY;
      }

      _data->appendHex(data.size());
      _data->appendText("\r\n");
      _data->appendText(data.c_str(), data.size());
      _data->appendText("\r\n");
    }
  }

  signal();

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

bool AsyncChunkedTask::setup (Scheduler* scheduler, EventLoop loop) {
  return AsyncTask::setup(scheduler, loop);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cleanup
////////////////////////////////////////////////////////////////////////////////

void AsyncChunkedTask::cleanup () {
  return AsyncTask::cleanup();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles signal if a new chunk has arrived
////////////////////////////////////////////////////////////////////////////////

bool AsyncChunkedTask::handleAsync () {
  MUTEX_LOCKER(_dataLock);

  if (_data != nullptr) {
    _output->sendChunk(_data);
    _data = nullptr;
  }
    
  if (_done) {
    _output->finishedChunked();
    _done = false;
  }

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                class HttpCommTask
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new task
////////////////////////////////////////////////////////////////////////////////

HttpCommTask::HttpCommTask (HttpServer* server,
                            TRI_socket_t socket,
                            ConnectionInfo const& info,
                            double keepAliveTimeout)
  : Task("HttpCommTask"),
    SocketTask(socket, keepAliveTimeout),
    _connectionInfo(info),
    _server(server),
    _writeBuffers(),
#ifdef TRI_ENABLE_FIGURES
    _writeBuffersStats(),
#endif
    _readPosition(0),
    _bodyPosition(0),
    _bodyLength(0),
    _requestPending(false),
    _closeRequested(false),
    _readRequestBody(false),
    _request(nullptr),
    _maximalHeaderSize(0),
    _maximalBodySize(0),
    _maximalPipelineSize(0),
    _httpVersion(HttpRequest::HTTP_UNKNOWN),
    _requestType(HttpRequest::HTTP_REQUEST_ILLEGAL),
    _fullUrl(),
    _origin(),
    _denyCredentials(false),
    _acceptDeflate(false),
    _newRequest(true),
    _startPosition(0),
    _sinceCompactification(0),
    _originalBodyLength(0),
    _isChunked(false),
    _chunkedTask(this),
    _setupDone(false) {
  LOG_TRACE(
    "connection established, client %d, server ip %s, server port %d, client ip %s, client port %d",
    (int) TRI_get_fd_or_handle_of_socket(socket),
    _connectionInfo.serverAddress.c_str(),
    (int) _connectionInfo.serverPort,
    _connectionInfo.clientAddress.c_str(),
    (int) _connectionInfo.clientPort);

  const auto p = server->handlerFactory()->sizeRestrictions();

  _maximalHeaderSize = p.maximalHeaderSize;
  _maximalBodySize = p.maximalBodySize;
  _maximalPipelineSize = p.maximalPipelineSize;

  ConnectionStatisticsAgentSetHttp(this);
  ConnectionStatisticsAgent::release();

  ConnectionStatisticsAgent::acquire();
  ConnectionStatisticsAgentSetStart(this);
  ConnectionStatisticsAgentSetHttp(this);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a task
////////////////////////////////////////////////////////////////////////////////

HttpCommTask::~HttpCommTask () {
  LOG_TRACE("connection closed, client %d",
            (int) TRI_get_fd_or_handle_of_socket(_commSocket));

  // free write buffers
  for (auto i : _writeBuffers) {
    delete i;
  }

#ifdef TRI_ENABLE_FIGURES

  for (auto i : _writeBuffersStats) {
    TRI_ReleaseRequestStatistics(i);
  }

#endif

  // free request
  if (_request != nullptr) {
    delete _request;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief signals a new chunk
////////////////////////////////////////////////////////////////////////////////

int HttpCommTask::signalChunk (const string& data) {
  return _chunkedTask.signalChunk(data);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief begins shutdown 
////////////////////////////////////////////////////////////////////////////////

void HttpCommTask::beginShutdown ()  {
#ifdef _WIN32
  // Cannot close socket descriptors here. Probably should not close
  // these for linux as well.
  return;
#else
  if (TRI_isvalidsocket(_commSocket)) {
    TRI_CLOSE_SOCKET(_commSocket);
    TRI_invalidatesocket(&_commSocket);
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles response
////////////////////////////////////////////////////////////////////////////////

void HttpCommTask::handleResponse (HttpResponse * response)  {
  if (response->isChunked()) {
    _requestPending = true;
    _isChunked = true;
  }
  else {
    _requestPending = false;
    _isChunked = false;
  }

  addResponse(response);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads data from the socket
////////////////////////////////////////////////////////////////////////////////

bool HttpCommTask::processRead () {
  if (_requestPending || _readBuffer->c_str() == nullptr) {
    return false;
  }

  bool handleRequest = false;

  // still trying to read the header fields
  if (! _readRequestBody) {

    // starting a new request
    if (_newRequest) {
#ifdef TRI_ENABLE_FIGURES
      RequestStatisticsAgent::acquire();
      RequestStatisticsAgentSetReadStart(this);
#endif

      _newRequest      = false;
      _startPosition   = _readPosition;
      _httpVersion     = HttpRequest::HTTP_UNKNOWN;
      _requestType     = HttpRequest::HTTP_REQUEST_ILLEGAL;
      _fullUrl         = "";
      _denyCredentials = false;
      _acceptDeflate   = false;

      _sinceCompactification++;
    }

    const char * ptr = _readBuffer->c_str() + _readPosition;
    const char * end = _readBuffer->end() - 3;

    for (;  ptr < end;  ptr++) {
      if (ptr[0] == '\r' && ptr[1] == '\n' && ptr[2] == '\r' && ptr[3] == '\n') {
        break;
      }
    }

    // check if header is too large
    size_t headerLength = ptr - (_readBuffer->c_str() + _startPosition);

    if (headerLength > _maximalHeaderSize) {
      LOG_WARNING("maximal header size is %d, request header size is %d",
                  (int) _maximalHeaderSize,
                  (int) headerLength);

      // header is too large
      HttpResponse response(HttpResponse::REQUEST_HEADER_FIELDS_TOO_LARGE, getCompatibility());

      // we need to close the connection, because there is no way we 
      // know what to remove and then continue
      resetState(true);
      handleResponse(&response);

      return false;
    }

    // header is complete
    if (ptr < end) {
      _readPosition = ptr - _readBuffer->c_str() + 4;

      LOG_TRACE("HTTP READ FOR %p: %s", (void*) this,
                string(_readBuffer->c_str() + _startPosition,
                            _readPosition - _startPosition).c_str());

      // check that we know, how to serve this request
      // and update the connection information, i. e. client and server addresses and ports
      // and create a request context for that request
      _request = _server->handlerFactory()->createRequest(
        _connectionInfo,
        _readBuffer->c_str() + _startPosition,
        _readPosition - _startPosition);

      if (_request == nullptr) {
        LOG_ERROR("cannot generate request");

        // internal server error
        HttpResponse response(HttpResponse::SERVER_ERROR, getCompatibility());

        // we need to close the connection, because there is no way we 
        // know how to remove the body and then continue
        resetState(true);
        handleResponse(&response);

        return false;
      }

      _request->setClientTaskId(_taskId);

      // check HTTP protocol version
      _httpVersion = _request->httpVersion();

      if (_httpVersion != HttpRequest::HTTP_1_0 &&
          _httpVersion != HttpRequest::HTTP_1_1) {

        HttpResponse response(HttpResponse::HTTP_VERSION_NOT_SUPPORTED, getCompatibility());

        // we need to close the connection, because there is no way we 
        // know what to remove and then continue
        resetState(true);
        handleResponse(&response);

        return false;
      }

      // check max URL length
      _fullUrl = _request->fullUrl();

      if (_fullUrl.size() > 16384) {
        HttpResponse response(HttpResponse::REQUEST_URI_TOO_LONG, getCompatibility());

        // we need to close the connection, because there is no way we 
        // know what to remove and then continue
        resetState(true);
        handleResponse(&response);

        return false;
      }

      // update the connection information, i. e. client and server addresses and ports
      _request->setProtocol(_server->protocol());

      LOG_TRACE("server port %d, client port %d",
                (int) _connectionInfo.serverPort,
                (int) _connectionInfo.clientPort);

      // set body start to current position
      _bodyPosition = _readPosition;
      _bodyLength = 0;

      // keep track of the original value of the "origin" request header (if any)
      // we need this value to handle CORS requests
      _origin = _request->header("origin");

      if (! _origin.empty()) {

        // check for Access-Control-Allow-Credentials header
        bool found;
        string const& allowCredentials = _request->header("access-control-allow-credentials", found);

        if (found) {
          _denyCredentials = ! StringUtils::boolean(allowCredentials);
        }
      }

      // store the original request's type. we need it later when responding
      // (original request object gets deleted before responding)
      _requestType = _request->requestType();

#ifdef TRI_ENABLE_FIGURES
      RequestStatisticsAgentSetRequestType(this, _requestType);
#endif

      // handle different HTTP methods
      switch (_requestType) {
        case HttpRequest::HTTP_REQUEST_GET:
        case HttpRequest::HTTP_REQUEST_DELETE:
        case HttpRequest::HTTP_REQUEST_HEAD:
        case HttpRequest::HTTP_REQUEST_OPTIONS:
        case HttpRequest::HTTP_REQUEST_POST:
        case HttpRequest::HTTP_REQUEST_PUT:
        case HttpRequest::HTTP_REQUEST_PATCH: {
          // technically, sending a body for an HTTP DELETE request is not forbidden, but it is not explicitly supported
          const bool expectContentLength = (_requestType == HttpRequest::HTTP_REQUEST_POST
                                            || _requestType == HttpRequest::HTTP_REQUEST_PUT
                                            || _requestType == HttpRequest::HTTP_REQUEST_PATCH
                                            || _requestType == HttpRequest::HTTP_REQUEST_OPTIONS
                                            || _requestType == HttpRequest::HTTP_REQUEST_DELETE);

          if (! checkContentLength(expectContentLength)) {
            return false;
          }

          if (_bodyLength == 0) {
            handleRequest = true;
          }

          break;
        }

        default: {
          size_t l = _readPosition - _startPosition;

          if (6 < l) {
            l = 6;
          }

          LOG_WARNING("got corrupted HTTP request '%s'",
                      string(_readBuffer->c_str() + _startPosition, l).c_str());

          // bad request, method not allowed
          HttpResponse response(HttpResponse::METHOD_NOT_ALLOWED, getCompatibility());

          // we need to close the connection, because there is no way we 
          // know what to remove and then continue
          resetState(true);

          // force a socket close, response will be ignored!
          TRI_CLOSE_SOCKET(_commSocket);
          TRI_invalidatesocket(&_commSocket);

          // might delete this
          handleResponse(&response);

          return false;
        }
      }

      // .............................................................................
      // check if server is active
      // .............................................................................

      Scheduler const* scheduler = _server->scheduler();

      if (scheduler != nullptr && ! scheduler->isActive()) {
        // server is inactive and will intentionally respond with HTTP 503
        LOG_TRACE("cannot serve request - server is inactive");

        HttpResponse response(HttpResponse::SERVICE_UNAVAILABLE, getCompatibility());

        // we need to close the connection, because there is no way we 
        // know what to remove and then continue
        resetState(true);
        handleResponse(&response);

        return false;
      }

      // check for a 100-continue
      if (_readRequestBody) {
        bool found;
        string const& expect = _request->header("expect", found);

        if (found && StringUtils::trim(expect) == "100-continue") {
          LOG_TRACE("received a 100-continue request");

          StringBuffer* buffer = new StringBuffer(TRI_UNKNOWN_MEM_ZONE);
          buffer->appendText("HTTP/1.1 100 (Continue)\r\n\r\n");

          _writeBuffers.push_back(buffer);

#ifdef TRI_ENABLE_FIGURES
          _writeBuffersStats.push_back(0);
#endif

          fillWriteBuffer();
        }
      }
    }
    else {
      size_t l = (_readBuffer->end() - _readBuffer->c_str());

      if (_startPosition + 4 <= l) {
        _readPosition = l - 4;
      }
    }
  }

  // readRequestBody might have changed, so cannot use else
  if (_readRequestBody) {
    if (_readBuffer->length() - _bodyPosition < _bodyLength) {
      setKeepAliveTimeout(60.0);

      // let client send more
      return false;
    }

    // read "bodyLength" from read buffer and add this body to "httpRequest"
    _request->setBody(_readBuffer->c_str() + _bodyPosition, _bodyLength);

    LOG_TRACE("%s", string(_readBuffer->c_str() + _bodyPosition, _bodyLength).c_str());

    // remove body from read buffer and reset read position
    _readRequestBody = false;
    handleRequest = true;
  }

  // .............................................................................
  // request complete
  //
  // we have to delete request in here or pass it to a handler, which will delete
  // it
  // .............................................................................

  if (! handleRequest) {
    return false;
  }

  RequestStatisticsAgentSetReadEnd(this);
  RequestStatisticsAgentAddReceivedBytes(this, _bodyPosition - _startPosition + _bodyLength);

  bool isOptions = (_requestType == HttpRequest::HTTP_REQUEST_OPTIONS);
  resetState(false);

  // .............................................................................
  // keep-alive handling
  // .............................................................................

  string connectionType = StringUtils::tolower(_request->header("connection"));

  if (connectionType == "close") {
    // client has sent an explicit "Connection: Close" header. we should close the connection
    LOG_DEBUG("connection close requested by client");
    _closeRequested = true;
  }
  else if (_request->isHttp10() && connectionType != "keep-alive") {
    // HTTP 1.0 request, and no "Connection: Keep-Alive" header sent
    // we should close the connection
    LOG_DEBUG("no keep-alive, connection close requested by client");
    _closeRequested = true;
  }
  else if (_keepAliveTimeout <= 0.0) {
    // if keepAliveTimeout was set to 0.0, we'll close even keep-alive connections immediately
    LOG_DEBUG("keep-alive disabled by admin");
    _closeRequested = true;
  }

  // we keep the connection open in all other cases (HTTP 1.1 or Keep-Alive header sent)

  // .............................................................................
  // authenticate
  // .............................................................................

  auto const compatibility = _request->compatibility();

  HttpResponse::HttpResponseCode authResult = _server->handlerFactory()->authenticateRequest(_request);

  // authenticated or an OPTIONS request. OPTIONS requests currently go unauthenticated
  if (authResult == HttpResponse::OK || isOptions) {

    // handle HTTP OPTIONS requests directly
    if (isOptions) {
      processCorsOptions(compatibility);
    }
    else {
      processRequest(compatibility);
    }
  }

  // not found
  else if (authResult == HttpResponse::NOT_FOUND) {
    HttpResponse response(authResult, compatibility);
    response.setContentType("application/json; charset=utf-8");

    response.body()
    .appendText("{\"error\":true,\"errorMessage\":\"")
    .appendText(TRI_errno_string(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND))
    .appendText("\",\"code\":")
    .appendInteger((int) authResult)
    .appendText(",\"errorNum\":")
    .appendInteger(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND)
    .appendText("}");

    clearRequest();
    handleResponse(&response);
  }

  // forbidden
  else if (authResult == HttpResponse::FORBIDDEN) {
    HttpResponse response(authResult, compatibility);
    response.setContentType("application/json; charset=utf-8");

    response.body()
    .appendText("{\"error\":true,\"errorMessage\":\"change password\",\"code\":")
    .appendInteger((int) authResult)
    .appendText(",\"errorNum\":")
    .appendInteger(TRI_ERROR_USER_CHANGE_PASSWORD)
    .appendText("}");

    clearRequest();
    handleResponse(&response);
  }

  // not authenticated
  else {
    HttpResponse response(HttpResponse::UNAUTHORIZED, compatibility);
    const string realm = "basic realm=\"" + _server->handlerFactory()->authenticationRealm(_request) + "\"";

    if (sendWwwAuthenticateHeader()) {
      response.setHeader("www-authenticate", strlen("www-authenticate"), realm.c_str());
    }

    clearRequest();
    handleResponse(&response);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sends more chunked data
////////////////////////////////////////////////////////////////////////////////

void HttpCommTask::sendChunk (StringBuffer* buffer) {
  if (_isChunked) {
    _writeBuffers.push_back(buffer);

#ifdef TRI_ENABLE_FIGURES
    _writeBuffersStats.push_back(0);
#endif

    fillWriteBuffer();
  }
  else {
    delete buffer;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief chunking is finished
////////////////////////////////////////////////////////////////////////////////

void HttpCommTask::finishedChunked () {
  StringBuffer* buffer = new StringBuffer(TRI_UNKNOWN_MEM_ZONE, 6);
  buffer->appendText("0\r\n\r\n");

  _writeBuffers.push_back(buffer);

#ifdef TRI_ENABLE_FIGURES
  _writeBuffersStats.push_back(0);
#endif

  _isChunked = false;
  _requestPending = false;

  fillWriteBuffer();
  processRead();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief task set up complete
////////////////////////////////////////////////////////////////////////////////

void HttpCommTask::setupDone () {
  _setupDone.store(true, std::memory_order_relaxed);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief reads data from the socket
////////////////////////////////////////////////////////////////////////////////

void HttpCommTask::addResponse (HttpResponse* response) {

  // CORS response handling
  if (! _origin.empty()) {

    // the request contained an Origin header. We have to send back the
    // access-control-allow-origin header now
    LOG_TRACE("handling CORS response");

    response->setHeader("access-control-expose-headers",
                        strlen("access-control-expose-headers"),
                        "etag, content-encoding, content-length, location, server, x-arango-errors, x-arango-async-id");

    // TODO: check whether anyone actually needs these headers in the browser:
    // x-arango-replication-checkmore, x-arango-replication-lastincluded,
    // x-arango-replication-lasttick, x-arango-replication-active");

    // send back original value of "Origin" header
    response->setHeader("access-control-allow-origin", strlen("access-control-allow-origin"), _origin);

    // send back "Access-Control-Allow-Credentials" header
    if (_denyCredentials) {
      response->setHeader("access-control-allow-credentials", "false");
    }
    else {
      response->setHeader("access-control-allow-credentials", "true");
    }
  }
  // CORS request handling EOF

  // set "connection" header
  if (_closeRequested) {
    response->setHeader("connection", strlen("connection"), "Close");
  }
  else {
    // keep-alive is the default
    response->setHeader("connection", strlen("connection"), "Keep-Alive");
  }

  size_t responseBodyLength = response->bodySize();

  if (_requestType == HttpRequest::HTTP_REQUEST_HEAD) {
    // clear body if this is an HTTP HEAD request
    // HEAD must not return a body
    response->headResponse(responseBodyLength);
  }
  // else {
  //    // to enable automatic deflating of responses, active this.
  //   // deflate takes a lot of CPU time so it should only be enabled for
  //   // dedicated purposes and not generally
  //   if (responseBodyLength > 16384  && _acceptDeflate) {
  //     response->deflate();
  //     responseBodyLength = response->bodySize();
  //   }
  // }

  // reserve some outbuffer size
  StringBuffer* buffer
    = new StringBuffer(TRI_UNKNOWN_MEM_ZONE, responseBodyLength + 128);

  // write header
  response->writeHeader(buffer);

  // write body
  if (_requestType != HttpRequest::HTTP_REQUEST_HEAD) {
    if (_isChunked) {
      if (0 != responseBodyLength) {
        buffer->appendHex(response->body().length());
        buffer->appendText("\r\n");
        buffer->appendText(response->body());
        buffer->appendText("\r\n");
      }
    }
    else {
      buffer->appendText(response->body());
    }
  }

  _writeBuffers.push_back(buffer);
          
  LOG_TRACE("HTTP WRITE FOR %p: %s", (void*) this, buffer->c_str());
          
  // clear body
  response->body().clear();
          
  double totalTime;

#ifdef TRI_ENABLE_FIGURES
  _writeBuffersStats.push_back(RequestStatisticsAgent::transfer());
  totalTime = RequestStatisticsAgent::elapsedSinceReadStart();
#else
  totalTime = 0.0;
#endif

  // disable the following statement to prevent excessive logging of incoming requests
  LOG_USAGE(",\"http-request\",\"%s\",\"%s\",\"%s\",%d,%llu,%llu,\"%s\",%.6f",
            _connectionInfo.clientAddress.c_str(),
            HttpRequest::translateMethod(_requestType).c_str(),
            HttpRequest::translateVersion(_httpVersion).c_str(),
            (int) response->responseCode(),
            (unsigned long long) _originalBodyLength,
            (unsigned long long) responseBodyLength,
            _fullUrl.c_str(),
            totalTime);
          
  // start output
  fillWriteBuffer();
}

////////////////////////////////////////////////////////////////////////////////
/// check the content-length header of a request and fail it is broken
////////////////////////////////////////////////////////////////////////////////

bool HttpCommTask::checkContentLength (bool expectContentLength) {
  const int64_t bodyLength = _request->contentLength();

  if (bodyLength < 0) {

    // bad request, body length is < 0. this is a client error
    HttpResponse response(HttpResponse::LENGTH_REQUIRED, getCompatibility());

    resetState(true);
    handleResponse(&response);

    return false;
  }

  if (! expectContentLength && bodyLength > 0) {
    // content-length header was sent but the request method does not support that
    // we'll warn but read the body anyway
    LOG_WARNING("received HTTP GET/HEAD request with content-length, this should not happen");
  }

  if ((size_t) bodyLength > _maximalBodySize) {
    LOG_WARNING("maximal body size is %d, request body size is %d", (int) _maximalBodySize, (int) bodyLength);

    // request entity too large
    HttpResponse response(HttpResponse::REQUEST_ENTITY_TOO_LARGE, getCompatibility());

    resetState(true);
    handleResponse(&response);

    return false;
  }

  // set instance variable to content-length value
  _bodyLength = (size_t) bodyLength;
  _originalBodyLength = _bodyLength;

  if (_bodyLength > 0) {
    // we'll read the body
    _readRequestBody = true;
  }

  // everything's fine
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the write buffer
////////////////////////////////////////////////////////////////////////////////

void HttpCommTask::fillWriteBuffer () {
  if (! hasWriteBuffer() && ! _writeBuffers.empty()) {
    StringBuffer * buffer = _writeBuffers.front();
    _writeBuffers.pop_front();

#ifdef TRI_ENABLE_FIGURES
    TRI_request_statistics_t* statistics = _writeBuffersStats.front();
    _writeBuffersStats.pop_front();
#else
    TRI_request_statistics_t* statistics = nullptr;
#endif

    setWriteBuffer(buffer, statistics);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles CORS options
////////////////////////////////////////////////////////////////////////////////

void HttpCommTask::processCorsOptions (uint32_t compatibility) {
  const string allowedMethods = "DELETE, GET, HEAD, PATCH, POST, PUT";

  HttpResponse response(HttpResponse::OK, compatibility);

  response.setHeader("allow", strlen("allow"), allowedMethods);

  if (! _origin.empty()) {
    LOG_TRACE("got CORS preflight request");
    const string allowHeaders = StringUtils::trim(_request->header("access-control-request-headers"));

    // send back which HTTP methods are allowed for the resource
    // we'll allow all
    response.setHeader("access-control-allow-methods", strlen("access-control-allow-methods"), allowedMethods);

    if (! allowHeaders.empty()) {
      // allow all extra headers the client requested
      // we don't verify them here. the worst that can happen is that the client
      // sends some broken headers and then later cannot access the data on the
      // server. that's a client problem.
      response.setHeader("access-control-allow-headers", strlen("access-control-allow-headers"), allowHeaders);
      LOG_TRACE("client requested validation of the following headers: %s", allowHeaders.c_str());
    }

    // set caching time (hard-coded value)
    response.setHeader("access-control-max-age", strlen("access-control-max-age"), "1800");
  }

  clearRequest();
  handleResponse(&response);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief processes a request
////////////////////////////////////////////////////////////////////////////////

void HttpCommTask::processRequest (uint32_t compatibility) {
  HttpHandler* handler = _server->handlerFactory()->createHandler(_request);

  if (handler == nullptr) {
    LOG_TRACE("no handler is known, giving up");

    HttpResponse response(HttpResponse::NOT_FOUND, compatibility);

    clearRequest();
    handleResponse(&response);

    return;
  }

  bool found;
  string const& acceptEncoding = _request->header("accept-encoding", found);

  if (found) {
    if (acceptEncoding.find("deflate") != string::npos) {
      _acceptDeflate = true;
    }
  }

  // check for an async request
  string const& asyncExecution = _request->header("x-arango-async", found);

  // clear request object
  _request = nullptr;
  RequestStatisticsAgent::transfer(handler);
  
  int res = TRI_ERROR_INTERNAL;

  // async execution
  if (found && (asyncExecution == "true" || asyncExecution == "store")) {

#ifdef TRI_ENABLE_FIGURES
    RequestStatisticsAgentSetAsync(this);
#endif

    uint64_t jobId = 0;

    if (asyncExecution == "store") {
      // persist the responses
      res = _server->handleRequestAsync(handler, &jobId);
    }
    else {
      // don't persist the responses
      res = _server->handleRequestAsync(handler, 0);
    }

    if (res == TRI_ERROR_NO_ERROR) {
      HttpResponse response(HttpResponse::ACCEPTED, compatibility);

      if (jobId > 0) {
        // return the job id we just created
        response.setHeader("x-arango-async-id",
                           strlen("x-arango-async-id"),
                           StringUtils::itoa(jobId));
      }

      handleResponse(&response);

      return;
    }
  }

  // synchronous request
  else {
    res = _server->handleRequest(this, handler);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    HttpResponse response(HttpResponse::SERVER_ERROR, compatibility);

    response.body()
    .appendText("{\"error\":true,\"errorMessage\":\"")
    .appendText(TRI_errno_string(res))
    .appendText("\",\"code\":")
    .appendInteger((int) HttpResponse::SERVER_ERROR)
    .appendText(",\"errorNum\":")
    .appendInteger(res)
    .appendText("}");

    handleResponse(&response);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clears the request object
////////////////////////////////////////////////////////////////////////////////

void HttpCommTask::clearRequest () {
  if (_request != nullptr) {
    delete _request;
    _request = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resets the internal state
///
/// this method can be called to clean up when the request handling aborts
/// prematurely
////////////////////////////////////////////////////////////////////////////////

void HttpCommTask::resetState (bool close) {
  const size_t COMPACT_EVERY = 500;

  if (close) {
    clearRequest();

    _requestPending = false;
    _closeRequested = true;

    _readPosition    = 0;
    _bodyPosition    = 0;
    _bodyLength      = 0;
  }
  else {
    _requestPending = true;

    bool compact = false;

    if (_sinceCompactification > COMPACT_EVERY) {
      compact = true;
    }
    else if (_readBuffer->length() > _maximalPipelineSize) {
      compact = true;
    }

    if (compact) {
      _readBuffer->erase_front(_bodyPosition + _bodyLength);

      _sinceCompactification = 0;
      _readPosition = 0;
    }
    else {
      _readPosition = _bodyPosition + _bodyLength;
    }

    _bodyPosition    = 0;
    _bodyLength      = 0;
  }

  _newRequest      = true;
  _readRequestBody = false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief decides whether or not we should send back a www-authenticate header
////////////////////////////////////////////////////////////////////////////////

bool HttpCommTask::sendWwwAuthenticateHeader () const {
  bool found;
  _request->header("x-omit-www-authenticate", found);

  return ! found;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get request compatibility
////////////////////////////////////////////////////////////////////////////////

int32_t HttpCommTask::getCompatibility () const {
  if (_request != nullptr) {
    return _request->compatibility();
  }

  return HttpRequest::MinCompatibility;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                      Task methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool HttpCommTask::setup (Scheduler* scheduler, EventLoop loop) {
  bool ok = SocketTask::setup(scheduler, loop);

  if (! ok) {
    return false;
  }

  ok = AsyncTask::setup(scheduler, loop);

  if (! ok) {
    return false;
  }

  ok = _chunkedTask.setup(scheduler, loop);

  if (! ok) {
    return false;
  }

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void HttpCommTask::cleanup () {
  SocketTask::cleanup();
  AsyncTask::cleanup();
  _chunkedTask.cleanup();
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool HttpCommTask::handleEvent (EventToken token, EventType events) {
  bool result = SocketTask::handleEvent(token, events);

  if (result) {
    result = AsyncTask::handleEvent(token, events);
  }

  if (_clientClosed) {
    _scheduler->destroyTask(this);
  }

  return result;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 AsyncTask methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool HttpCommTask::handleAsync () {
  _server->handleAsync(this);
  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                SocketTask methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool HttpCommTask::handleRead ()  {
  bool res = true;

  if (! _setupDone.load(std::memory_order_relaxed)) {
    return res;
  }

  if (! _closeRequested) {
    res = fillReadBuffer();

    // process as much data as we got
    while (processRead()) {
      if (_closeRequested) {
        break;
      }
    }
  }
  else {
    // if we don't close here, the scheduler thread may fall into a 
    // busy wait state, consuming 100% CPU!
    _clientClosed = true;
  }

  if (_clientClosed) {
    res = false;
    _server->handleCommunicationClosed(this);
  }
  else if (! res) {
    _clientClosed = true;
    _server->handleCommunicationFailure(this);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void HttpCommTask::completedWriteBuffer () {
  _writeBuffer = nullptr;

#ifdef TRI_ENABLE_FIGURES
  if (_writeBufferStatistics != nullptr) {
    _writeBufferStatistics->_writeEnd = TRI_StatisticsTime();

    TRI_ReleaseRequestStatistics(_writeBufferStatistics);
    _writeBufferStatistics = nullptr;
  }
#endif

  fillWriteBuffer();

  if (! _clientClosed && _closeRequested && ! hasWriteBuffer() && _writeBuffers.empty() && ! _isChunked) {
    _clientClosed = true;
    _server->handleCommunicationClosed(this);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void HttpCommTask::handleTimeout () {
  _server->handleCommunicationClosed(this);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
