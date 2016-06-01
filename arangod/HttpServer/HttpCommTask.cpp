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

#include "HttpCommTask.h"

#include "Basics/MutexLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Logger/Logger.h"
#include "HttpServer/HttpHandler.h"
#include "HttpServer/HttpHandlerFactory.h"
#include "HttpServer/HttpServer.h"
#include "Scheduler/Scheduler.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief static initializers
////////////////////////////////////////////////////////////////////////////////

size_t const HttpCommTask::MaximalHeaderSize = 1 * 1024 * 1024;       //   1 MB
size_t const HttpCommTask::MaximalBodySize = 512 * 1024 * 1024;       // 512 MB
size_t const HttpCommTask::MaximalPipelineSize = 512 * 1024 * 1024;   // 512 MB
size_t const HttpCommTask::RunCompactEvery = 500;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new task
////////////////////////////////////////////////////////////////////////////////

HttpCommTask::HttpCommTask(HttpServer* server, TRI_socket_t socket,
                           ConnectionInfo&& info, double keepAliveTimeout)
    : Task("HttpCommTask"),
      SocketTask(socket, keepAliveTimeout),
      _connectionInfo(std::move(info)),
      _server(server),
      _writeBuffers(),
      _writeBuffersStats(),
      _readPosition(0),
      _bodyPosition(0),
      _bodyLength(0),
      _requestPending(false),
      _closeRequested(false),
      _readRequestBody(false),
      _denyCredentials(true),
      _acceptDeflate(false),
      _newRequest(true),
      _isChunked(false),
      _startThread(false),
      _request(nullptr),
      _httpVersion(GeneralRequest::ProtocolVersion::UNKNOWN),
      _requestType(GeneralRequest::RequestType::ILLEGAL),
      _fullUrl(),
      _origin(),
      _startPosition(0),
      _sinceCompactification(0),
      _originalBodyLength(0),
      _setupDone(false) {
  LOG(TRACE) << "connection established, client "
             << TRI_get_fd_or_handle_of_socket(socket) << ", server ip "
             << _connectionInfo.serverAddress << ", server port "
             << _connectionInfo.serverPort << ", client ip "
             << _connectionInfo.clientAddress << ", client port "
             << _connectionInfo.clientPort;
  
  connectionStatisticsAgentSetHttp();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a task
////////////////////////////////////////////////////////////////////////////////

HttpCommTask::~HttpCommTask() {
  LOG(TRACE) << "connection closed, client "
             << TRI_get_fd_or_handle_of_socket(_commSocket);

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

void HttpCommTask::handleResponse(HttpResponse* response) {
  _requestPending = false;
  _isChunked = false;
  _startThread = false;

  addResponse(response);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads data from the socket
////////////////////////////////////////////////////////////////////////////////

bool HttpCommTask::processRead() {
  if (_requestPending || _readBuffer->c_str() == nullptr) {
    return false;
  }

  bool handleRequest = false;

  // still trying to read the header fields
  if (!_readRequestBody) {
    char const* ptr = _readBuffer->c_str() + _readPosition;
    char const* etr = _readBuffer->end();

    if (ptr == etr) {
      return false;
    }

    // starting a new request
    if (_newRequest) {
      // acquire a new statistics entry for the request
      RequestStatisticsAgent::acquire();

#if USE_DEV_TIMERS
      if (RequestStatisticsAgent::_statistics != nullptr) {
        RequestStatisticsAgent::_statistics->_id = (void*) this;
      }
#endif

      _newRequest = false;
      _startPosition = _readPosition;
      _httpVersion = GeneralRequest::ProtocolVersion::UNKNOWN;
      _requestType = GeneralRequest::RequestType::ILLEGAL;
      _fullUrl = "";
      _denyCredentials = true;
      _acceptDeflate = false;

      _sinceCompactification++;
    }

    char const* end = etr - 3;

    // read buffer contents are way to small. we can exit here directly
    if (ptr >= end) {
      return false;
    }

    // request started
    requestStatisticsAgentSetReadStart();

    // check for the end of the request
    for (; ptr < end; ptr++) {
      if (ptr[0] == '\r' && ptr[1] == '\n' && ptr[2] == '\r' &&
          ptr[3] == '\n') {
        break;
      }
    }

    // check if header is too large
    size_t headerLength = ptr - (_readBuffer->c_str() + _startPosition);

    if (headerLength > MaximalHeaderSize) {
      LOG(WARN) << "maximal header size is " << MaximalHeaderSize
                << ", request header size is " << headerLength;

      // header is too large
      HttpResponse response(
          GeneralResponse::ResponseCode::REQUEST_HEADER_FIELDS_TOO_LARGE);

      // we need to close the connection, because there is no way we
      // know what to remove and then continue
      resetState(true);
      handleResponse(&response);

      return false;
    }

    // header is complete
    if (ptr < end) {
      _readPosition = ptr - _readBuffer->c_str() + 4;

      LOG(TRACE) << "HTTP READ FOR " << (void*)this << ": "
                 << std::string(_readBuffer->c_str() + _startPosition,
                                _readPosition - _startPosition);

      // check that we know, how to serve this request and update the connection
      // information, i. e. client and server addresses and ports and create a
      // request context for that request
      _request = _server->handlerFactory()->createRequest(
          _connectionInfo, _readBuffer->c_str() + _startPosition,
          _readPosition - _startPosition);

      if (_request == nullptr) {
        LOG(ERR) << "cannot generate request";

        // internal server error
        HttpResponse response(GeneralResponse::ResponseCode::SERVER_ERROR);

        // we need to close the connection, because there is no way we
        // know how to remove the body and then continue
        resetState(true);
        handleResponse(&response);

        return false;
      }

      _request->setClientTaskId(_taskId);

      // check HTTP protocol version
      _httpVersion = _request->protocolVersion();

      if (_httpVersion != GeneralRequest::ProtocolVersion::HTTP_1_0 &&
          _httpVersion != GeneralRequest::ProtocolVersion::HTTP_1_1) {
        HttpResponse response(
            GeneralResponse::ResponseCode::HTTP_VERSION_NOT_SUPPORTED);

        // we need to close the connection, because there is no way we
        // know what to remove and then continue
        resetState(true);
        handleResponse(&response);

        return false;
      }

      // check max URL length
      _fullUrl = _request->fullUrl();

      if (_fullUrl.size() > 16384) {
        HttpResponse response(
            GeneralResponse::ResponseCode::REQUEST_URI_TOO_LONG);

        // we need to close the connection, because there is no way we
        // know what to remove and then continue
        resetState(true);
        handleResponse(&response);

        return false;
      }

      // update the connection information, i. e. client and server addresses
      // and ports
      _request->setProtocol(_server->protocol());

      LOG(TRACE) << "server port " << _connectionInfo.serverPort
                 << ", client port " << _connectionInfo.clientPort;

      // set body start to current position
      _bodyPosition = _readPosition;
      _bodyLength = 0;

      // keep track of the original value of the "origin" request header (if
      // any), we need this value to handle CORS requests
      _origin = _request->header(StaticStrings::Origin);

      if (!_origin.empty()) {
        // check for Access-Control-Allow-Credentials header
        bool found;
        std::string const& allowCredentials =
            _request->header(StaticStrings::AccessControlAllowCredentials, found);

        if (found) {
          // default is to allow nothing
          _denyCredentials = true;
         
          // if the request asks to allow credentials, we'll check against the
          // configured whitelist of origins
          std::vector<std::string> const& accessControlAllowOrigins = _server->trustedOrigins();
          
          if (StringUtils::boolean(allowCredentials) &&
              !accessControlAllowOrigins.empty())  {
            if (accessControlAllowOrigins[0] == "*") {
              // special case: allow everything
              _denyCredentials = false;
            } else if (!_origin.empty()) {
              // copy origin string
              if (_origin[_origin.size() - 1] == '/') {
                // strip trailing slash
                auto result = std::find(accessControlAllowOrigins.begin(), accessControlAllowOrigins.end(), _origin.substr(0, _origin.size() - 1));
                _denyCredentials = (result == accessControlAllowOrigins.end());
              } else {
                auto result = std::find(accessControlAllowOrigins.begin(), accessControlAllowOrigins.end(), _origin);
                _denyCredentials = (result == accessControlAllowOrigins.end());
              }
            } else {
              TRI_ASSERT(_denyCredentials);
            }
          }
        }
      }

      // store the original request's type. we need it later when responding
      // (original request object gets deleted before responding)
      _requestType = _request->requestType();

      requestStatisticsAgentSetRequestType(_requestType);

      // handle different HTTP methods
      switch (_requestType) {
        case GeneralRequest::RequestType::GET:
        case GeneralRequest::RequestType::DELETE_REQ:
        case GeneralRequest::RequestType::HEAD:
        case GeneralRequest::RequestType::OPTIONS:
        case GeneralRequest::RequestType::POST:
        case GeneralRequest::RequestType::PUT:
        case GeneralRequest::RequestType::PATCH: {
          // technically, sending a body for an HTTP DELETE request is not
          // forbidden, but it is not explicitly supported
          bool const expectContentLength =
              (_requestType == GeneralRequest::RequestType::POST ||
               _requestType == GeneralRequest::RequestType::PUT ||
               _requestType == GeneralRequest::RequestType::PATCH ||
               _requestType == GeneralRequest::RequestType::OPTIONS ||
               _requestType == GeneralRequest::RequestType::DELETE_REQ);

          if (!checkContentLength(expectContentLength)) {
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

          LOG(WARN) << "got corrupted HTTP request '"
                    << std::string(_readBuffer->c_str() + _startPosition, l)
                    << "'";

          // bad request, method not allowed
          HttpResponse response(
              GeneralResponse::ResponseCode::METHOD_NOT_ALLOWED);

          // we need to close the connection, because there is no way we
          // know what to remove and then continue
          resetState(true);

          // force a socket close, response will be ignored!
          TRI_CLOSE_SOCKET(_commSocket);
          TRI_invalidatesocket(&_commSocket);

          // might delete this
          // note that as we closed the socket above, the response will not make
          // it to
          // the client! will result in a "Empty reply from server" error in
          // curl etc.
          handleResponse(&response);

          return false;
        }
      }

      // .............................................................................
      // check if server is active
      // .............................................................................

      Scheduler const* scheduler =
          _server->scheduler();  // TODO(fc) this is singleton now

      if (scheduler != nullptr && !scheduler->isActive()) {
        // server is inactive and will intentionally respond with HTTP 503
        LOG(TRACE) << "cannot serve request - server is inactive";

        HttpResponse response(
            GeneralResponse::ResponseCode::SERVICE_UNAVAILABLE);

        // we need to close the connection, because there is no way we
        // know what to remove and then continue
        resetState(true);
        handleResponse(&response);

        return false;
      }

      // check for a 100-continue
      if (_readRequestBody) {
        bool found;
        std::string const& expect = _request->header(StaticStrings::Expect, found);

        if (found && StringUtils::trim(expect) == "100-continue") {
          LOG(TRACE) << "received a 100-continue request";

          auto buffer = std::make_unique<StringBuffer>(TRI_UNKNOWN_MEM_ZONE);
          buffer->appendText(
              TRI_CHAR_LENGTH_PAIR("HTTP/1.1 100 (Continue)\r\n\r\n"));
          buffer->ensureNullTerminated();

          _writeBuffers.push_back(buffer.get());
          buffer.release();

          _writeBuffersStats.push_back(nullptr);

          fillWriteBuffer();
        }
      }
    } else {
      size_t l = (_readBuffer->end() - _readBuffer->c_str());

      if (_startPosition + 4 <= l) {
        _readPosition = l - 4;
      }
    }
  }

  // readRequestBody might have changed, so cannot use else
  if (_readRequestBody) {
    if (_readBuffer->length() - _bodyPosition < _bodyLength) {
      setKeepAliveTimeout(_keepAliveTimeout);

      // let client send more
      return false;
    }

    // read "bodyLength" from read buffer and add this body to "httpRequest"
    _request->setBody(_readBuffer->c_str() + _bodyPosition, _bodyLength);

    LOG(TRACE) << "" << std::string(_readBuffer->c_str() + _bodyPosition,
                                    _bodyLength);

    // remove body from read buffer and reset read position
    _readRequestBody = false;
    handleRequest = true;
  }

  // .............................................................................
  // request complete
  //
  // we have to delete request in here or pass it to a handler, which will
  // delete
  // it
  // .............................................................................

  if (!handleRequest) {
    return false;
  }

  requestStatisticsAgentSetReadEnd();
  requestStatisticsAgentAddReceivedBytes(_bodyPosition - _startPosition +
                                         _bodyLength);

  bool const isOptionsRequest =
      (_requestType == GeneralRequest::RequestType::OPTIONS);
  resetState(false);

  // .............................................................................
  // keep-alive handling
  // .............................................................................

  std::string connectionType =
      StringUtils::tolower(_request->header(StaticStrings::Connection));

  if (connectionType == "close") {
    // client has sent an explicit "Connection: Close" header. we should close
    // the connection
    LOG(DEBUG) << "connection close requested by client";
    _closeRequested = true;
  } else if (_request->isHttp10() && connectionType != "keep-alive") {
    // HTTP 1.0 request, and no "Connection: Keep-Alive" header sent
    // we should close the connection
    LOG(DEBUG) << "no keep-alive, connection close requested by client";
    _closeRequested = true;
  } else if (_keepAliveTimeout <= 0.0) {
    // if keepAliveTimeout was set to 0.0, we'll close even keep-alive
    // connections immediately
    LOG(DEBUG) << "keep-alive disabled by admin";
    _closeRequested = true;
  }

  // we keep the connection open in all other cases (HTTP 1.1 or Keep-Alive
  // header sent)

  // .............................................................................
  // authenticate
  // .............................................................................

  GeneralResponse::ResponseCode authResult =
      _server->handlerFactory()->authenticateRequest(_request);

  // authenticated or an OPTIONS request. OPTIONS requests currently go
  // unauthenticated
  if (authResult == GeneralResponse::ResponseCode::OK || isOptionsRequest) {
    // handle HTTP OPTIONS requests directly
    if (isOptionsRequest) {
      processCorsOptions();
    } else {
      processRequest();
    }
  }

  // not found
  else if (authResult == GeneralResponse::ResponseCode::NOT_FOUND) {
    HttpResponse response(authResult);
    response.setContentType(HttpResponse::CONTENT_TYPE_JSON);

    response.body()
        .appendText(TRI_CHAR_LENGTH_PAIR("{\"error\":true,\"errorMessage\":\""))
        .appendText(TRI_errno_string(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND))
        .appendText(TRI_CHAR_LENGTH_PAIR("\",\"code\":"))
        .appendInteger((int)authResult)
        .appendText(TRI_CHAR_LENGTH_PAIR(",\"errorNum\":"))
        .appendInteger(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND)
        .appendText(TRI_CHAR_LENGTH_PAIR("}"));

    clearRequest();
    handleResponse(&response);
  }

  // forbidden
  else if (authResult == GeneralResponse::ResponseCode::FORBIDDEN) {
    HttpResponse response(authResult);
    response.setContentType(HttpResponse::CONTENT_TYPE_JSON);

    response.body()
        .appendText(TRI_CHAR_LENGTH_PAIR(
            "{\"error\":true,\"errorMessage\":\"change password\",\"code\":"))
        .appendInteger((int)authResult)
        .appendText(TRI_CHAR_LENGTH_PAIR(",\"errorNum\":"))
        .appendInteger(TRI_ERROR_USER_CHANGE_PASSWORD)
        .appendText(TRI_CHAR_LENGTH_PAIR("}"));

    clearRequest();
    handleResponse(&response);
  }

  // not authenticated
  else {
    HttpResponse response(GeneralResponse::ResponseCode::UNAUTHORIZED);
    std::string realm =
      "Bearer token_type=\"JWT\", realm=\"ArangoDB\"";

    response.setHeaderNC(StaticStrings::WwwAuthenticate, std::move(realm));

    clearRequest();
    handleResponse(&response);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sends more chunked data
////////////////////////////////////////////////////////////////////////////////

void HttpCommTask::sendChunk(StringBuffer* buffer) {
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

void HttpCommTask::finishedChunked() {
  auto buffer = std::make_unique<StringBuffer>(TRI_UNKNOWN_MEM_ZONE, 6, true);
  buffer->appendText(TRI_CHAR_LENGTH_PAIR("0\r\n\r\n"));
  buffer->ensureNullTerminated();

  _writeBuffers.push_back(buffer.get());
  buffer.release();
  _writeBuffersStats.push_back(nullptr);

  _isChunked = false;
  _startThread = false;
  _requestPending = false;

  fillWriteBuffer();
  processRead();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief task set up complete
////////////////////////////////////////////////////////////////////////////////

void HttpCommTask::setupDone() {
  _setupDone.store(true, std::memory_order_relaxed);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads data from the socket
////////////////////////////////////////////////////////////////////////////////

void HttpCommTask::addResponse(HttpResponse* response) {
  // CORS response handling
  if (!_origin.empty()) {
    // the request contained an Origin header. We have to send back the
    // access-control-allow-origin header now
    LOG(TRACE) << "handling CORS response";

    response->setHeaderNC(StaticStrings::AccessControlExposeHeaders,
                          StaticStrings::ExposedCorsHeaders);

    // send back original value of "Origin" header
    response->setHeaderNC(StaticStrings::AccessControlAllowOrigin, _origin);

    // send back "Access-Control-Allow-Credentials" header
    response->setHeaderNC(StaticStrings::AccessControlAllowCredentials,
                          (_denyCredentials ? "false" : "true"));
  }
  // CORS request handling EOF

  // set "connection" header
  // keep-alive is the default
  response->setConnectionType(_closeRequested ? HttpResponse::CONNECTION_CLOSE : HttpResponse::CONNECTION_KEEP_ALIVE);

  size_t const responseBodyLength = response->bodySize();

  if (_requestType == GeneralRequest::RequestType::HEAD) {
    // clear body if this is an HTTP HEAD request
    // HEAD must not return a body
    response->headResponse(responseBodyLength);
  }
  // else {
  //   // to enable automatic deflating of responses, activate this.
  //   // deflate takes a lot of CPU time so it should only be enabled for
  //   // dedicated purposes and not generally
  //   if (responseBodyLength > 16384  && _acceptDeflate) {
  //     response->deflate();
  //     responseBodyLength = response->bodySize();
  //   }
  // }

  // reserve a buffer with some spare capacity
  auto buffer = std::make_unique<StringBuffer>(TRI_UNKNOWN_MEM_ZONE,
                                               responseBodyLength + 128, false);

  // write header
  response->writeHeader(buffer.get());

  // write body
  if (_requestType != GeneralRequest::RequestType::HEAD) {
    if (_isChunked) {
      if (0 != responseBodyLength) {
        buffer->appendHex(response->body().length());
        buffer->appendText(TRI_CHAR_LENGTH_PAIR("\r\n"));
        buffer->appendText(response->body());
        buffer->appendText(TRI_CHAR_LENGTH_PAIR("\r\n"));
      }
    } else {
      buffer->appendText(response->body());
    }
  }

  buffer->ensureNullTerminated();

  _writeBuffers.push_back(buffer.get());
  auto b = buffer.release();

  if (!b->empty()) {
    LOG_TOPIC(TRACE, Logger::REQUESTS)
        << "\"http-request-response\",\"" << (void*)this << "\",\""
        << StringUtils::escapeUnicode(std::string(b->c_str(), b->length()))
        << "\"";
  }

  // clear body
  response->body().clear();

  double const totalTime = RequestStatisticsAgent::elapsedSinceReadStart();

  _writeBuffersStats.push_back(RequestStatisticsAgent::steal());

  LOG_TOPIC(INFO, Logger::REQUESTS)
      << "\"http-request-end\",\"" << (void*)this << "\",\""
      << _connectionInfo.clientAddress << "\",\""
      << HttpRequest::translateMethod(_requestType) << "\",\""
      << HttpRequest::translateVersion(_httpVersion) << "\","
      << static_cast<int>(response->responseCode()) << ","
      << _originalBodyLength << "," << responseBodyLength << ",\"" << _fullUrl
      << "\"," << Logger::FIXED(totalTime, 6);

  // start output
  fillWriteBuffer();
}

////////////////////////////////////////////////////////////////////////////////
/// check the content-length header of a request and fail it is broken
////////////////////////////////////////////////////////////////////////////////

bool HttpCommTask::checkContentLength(bool expectContentLength) {
  int64_t const bodyLength = _request->contentLength();

  if (bodyLength < 0) {
    // bad request, body length is < 0. this is a client error
    HttpResponse response(GeneralResponse::ResponseCode::LENGTH_REQUIRED);

    resetState(true);
    handleResponse(&response);

    return false;
  }

  if (!expectContentLength && bodyLength > 0) {
    // content-length header was sent but the request method does not support
    // that
    // we'll warn but read the body anyway
    LOG(WARN) << "received HTTP GET/HEAD request with content-length, this "
                 "should not happen";
  }

  if ((size_t)bodyLength > MaximalBodySize) {
    LOG(WARN) << "maximal body size is " << MaximalBodySize
              << ", request body size is " << bodyLength;

    // request entity too large
    HttpResponse response(
        GeneralResponse::ResponseCode::REQUEST_ENTITY_TOO_LARGE);

    resetState(true);
    handleResponse(&response);

    return false;
  }

  // set instance variable to content-length value
  _bodyLength = (size_t)bodyLength;
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

void HttpCommTask::fillWriteBuffer() {
  if (!hasWriteBuffer() && !_writeBuffers.empty()) {
    StringBuffer* buffer = _writeBuffers.front();
    _writeBuffers.pop_front();

    TRI_ASSERT(buffer != nullptr);

    TRI_request_statistics_t* statistics = _writeBuffersStats.front();
    _writeBuffersStats.pop_front();

    setWriteBuffer(buffer, statistics);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles CORS options
////////////////////////////////////////////////////////////////////////////////

void HttpCommTask::processCorsOptions() {
  HttpResponse response(GeneralResponse::ResponseCode::OK);

  response.setHeaderNC(StaticStrings::Allow, StaticStrings::CorsMethods);

  if (!_origin.empty()) {
    LOG(TRACE) << "got CORS preflight request";
    std::string const allowHeaders = StringUtils::trim(
        _request->header(StaticStrings::AccessControlRequestHeaders));

    // send back which HTTP methods are allowed for the resource
    // we'll allow all
    response.setHeaderNC(StaticStrings::AccessControlAllowMethods, StaticStrings::CorsMethods);

    if (!allowHeaders.empty()) {
      // allow all extra headers the client requested
      // we don't verify them here. the worst that can happen is that the client
      // sends some broken headers and then later cannot access the data on the
      // server. that's a client problem.
      response.setHeaderNC(StaticStrings::AccessControlAllowHeaders,
                           allowHeaders);

      LOG(TRACE) << "client requested validation of the following headers: "
                 << allowHeaders;
    }

    // set caching time (hard-coded value)
    response.setHeaderNC(StaticStrings::AccessControlMaxAge, StaticStrings::N1800);
  }

  clearRequest();
  handleResponse(&response);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief processes a request
////////////////////////////////////////////////////////////////////////////////

void HttpCommTask::processRequest() {
  // check for deflate
  bool found;
  std::string const& acceptEncoding =
      _request->header(StaticStrings::AcceptEncoding, found);

  if (found) {
    if (acceptEncoding.find("deflate") != std::string::npos) {
      _acceptDeflate = true;
    }
  }

  LOG_TOPIC(DEBUG, Logger::REQUESTS)
      << "\"http-request-begin\",\"" << (void*)this << "\",\""
      << _connectionInfo.clientAddress << "\",\""
      << HttpRequest::translateMethod(_requestType) << "\",\""
      << HttpRequest::translateVersion(_httpVersion) << "\"," << _fullUrl
      << "\"";

  // check for an async request
  std::string const& asyncExecution = _request->header(StaticStrings::Async, found);

  // create handler, this will take over the request
  WorkItem::uptr<HttpHandler> handler(
      _server->handlerFactory()->createHandler(_request));

  if (handler == nullptr) {
    LOG(TRACE) << "no handler is known, giving up";

    HttpResponse response(GeneralResponse::ResponseCode::NOT_FOUND);

    clearRequest();
    handleResponse(&response);

    return;
  }

  if (_request != nullptr) {
    std::string const& body = _request->body();

    if (!body.empty()) {
      LOG_TOPIC(DEBUG, Logger::REQUESTS)
          << "\"http-request-body\",\"" << (void*)this << "\",\""
          << (StringUtils::escapeUnicode(body)) << "\"";
    }
   
    bool found; 
    std::string const& startThread = _request->header(StaticStrings::StartThread, found);

    if (found) {
      _startThread = StringUtils::boolean(startThread);
    }
  }
  
  handler->setTaskId(_taskId, _loop);

  // clear request object
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

      handleResponse(&response);

      return;
    }
  }

  // synchronous request
  else {
    ok = _server->handleRequest(this, handler);
  }

  if (!ok) {
    HttpResponse response(GeneralResponse::ResponseCode::SERVER_ERROR);
    handleResponse(&response);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clears the request object
////////////////////////////////////////////////////////////////////////////////

void HttpCommTask::clearRequest() {
  delete _request;
  _request = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resets the internal state
///
/// this method can be called to clean up when the request handling aborts
/// prematurely
////////////////////////////////////////////////////////////////////////////////

void HttpCommTask::resetState(bool close) {
  if (close) {
    clearRequest();

    _requestPending = false;
    _closeRequested = true;

    _readPosition = 0;
    _bodyPosition = 0;
    _bodyLength = 0;
  } else {
    _requestPending = true;

    bool compact = false;

    if (_sinceCompactification > RunCompactEvery) {
      compact = true;
    } else if (_readBuffer->length() > MaximalPipelineSize) {
      compact = true;
    }

    if (compact) {
      _readBuffer->erase_front(_bodyPosition + _bodyLength);

      _sinceCompactification = 0;
      _readPosition = 0;
    } else {
      _readPosition = _bodyPosition + _bodyLength;

      if (_readPosition == _readBuffer->length()) {
        _sinceCompactification = 0;
        _readPosition = 0;
        _readBuffer->reset();
      }
    }

    _bodyPosition = 0;
    _bodyLength = 0;
  }

  _newRequest = true;
  _readRequestBody = false;
  _startThread = false;
}

bool HttpCommTask::setup(Scheduler* scheduler, EventLoop loop) {
  bool ok = SocketTask::setup(scheduler, loop);

  if (!ok) {
    return false;
  }

  _scheduler = scheduler;
  _loop = loop;

  setupDone();

  return true;
}

void HttpCommTask::cleanup() { SocketTask::cleanup(); }

bool HttpCommTask::handleEvent(EventToken token, EventType events) {
  bool result = SocketTask::handleEvent(token, events);

  if (_clientClosed) {
    _scheduler->destroyTask(this);
  }

  return result;
}

void HttpCommTask::signalTask(TaskData* data) {
  // data response
  if (data->_type == TaskData::TASK_DATA_RESPONSE) {
    data->RequestStatisticsAgent::transferTo(this);
    handleResponse(data->_response.get());
    processRead();
  }

  // data chunk
  else if (data->_type == TaskData::TASK_DATA_CHUNK) {
    size_t len = data->_data.size();

    if (0 == len) {
      finishedChunked();
    } else {
      StringBuffer* buffer = new StringBuffer(TRI_UNKNOWN_MEM_ZONE, len);

      buffer->appendHex(len);
      buffer->appendText(TRI_CHAR_LENGTH_PAIR("\r\n"));
      buffer->appendText(data->_data.c_str(), len);
      buffer->appendText(TRI_CHAR_LENGTH_PAIR("\r\n"));

      sendChunk(buffer);
    }
  }

  // do not know, what to do - give up
  else {
    _scheduler->destroyTask(this);
  }
}

bool HttpCommTask::handleRead() {
  bool res = true;

  if (!_setupDone.load(std::memory_order_relaxed)) {
    return res;
  }

  if (!_closeRequested) {
    res = fillReadBuffer();

    // process as much data as we got
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
    _server->handleCommunicationClosed(this);
  } else if (!res) {
    _clientClosed = true;
    _server->handleCommunicationFailure(this);
  }

  return res;
}

void HttpCommTask::completedWriteBuffer() {
  _writeBuffer = nullptr;
  _writeLength = 0;

  if (_writeBufferStatistics != nullptr) {
    _writeBufferStatistics->_writeEnd = TRI_StatisticsTime();

    TRI_ReleaseRequestStatistics(_writeBufferStatistics);
    _writeBufferStatistics = nullptr;
  }

  fillWriteBuffer();

  if (!_clientClosed && _closeRequested && !hasWriteBuffer() &&
      _writeBuffers.empty() && !_isChunked) {
    _clientClosed = true;
    _server->handleCommunicationClosed(this);
  }
}

void HttpCommTask::handleTimeout() {
  _clientClosed = true;
  _server->handleCommunicationClosed(this);
}
