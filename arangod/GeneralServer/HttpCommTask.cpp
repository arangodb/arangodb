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

#include "HttpCommTask.h"

#include "Basics/tri-strings.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/RestHandler.h"
#include "GeneralServer/RestHandlerFactory.h"
#include "GeneralServer/VstCommTask.h"
#include "Meta/conversion.h"
#include "Rest/HttpRequest.h"
#include "Statistics/ConnectionStatistics.h"
#include "Utils/Events.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

size_t const HttpCommTask::MaximalHeaderSize = 2 * 1024 * 1024;       //    2 MB
size_t const HttpCommTask::MaximalBodySize = 1024 * 1024 * 1024;      // 1024 MB
size_t const HttpCommTask::MaximalPipelineSize = 1024 * 1024 * 1024;  // 1024 MB
size_t const HttpCommTask::RunCompactEvery = 500;

HttpCommTask::HttpCommTask(Scheduler* scheduler, GeneralServer* server,
                           std::unique_ptr<Socket> socket,
                           ConnectionInfo&& info, double timeout)
    : Task(scheduler, "HttpCommTask"),
      GeneralCommTask(scheduler, server, std::move(socket), std::move(info), timeout),
      _readPosition(0),
      _startPosition(0),
      _bodyPosition(0),
      _bodyLength(0),
      _readRequestBody(false),
      _allowMethodOverride(GeneralServerFeature::allowMethodOverride()),
      _denyCredentials(true),
      _newRequest(true),
      _requestType(rest::RequestType::ILLEGAL),
      _fullUrl(),
      _origin(),
      _sinceCompactification(0),
      _originalBodyLength(0) {
  _protocol = "http";

  ConnectionStatistics::SET_HTTP(_connectionStatistics);
}

// whether or not this task can mix sync and async I/O
bool HttpCommTask::canUseMixedIO() const {
  // in case SSL is used, we cannot use a combination of sync and async I/O
  // because that will make TLS fall apart
  return !_peer->isEncrypted();
}

/// @brief send error response including response body
void HttpCommTask::addSimpleResponse(rest::ResponseCode code,
                                     rest::ContentType respType, uint64_t /*messageId*/,
                                     velocypack::Buffer<uint8_t>&& buffer) {
  try {
    HttpResponse resp(code, leaseStringBuffer(buffer.size()));
    resp.setContentType(respType);
    if (!buffer.empty()) {
      resp.setPayload(std::move(buffer), true, VPackOptions::Defaults);
    }
    addResponse(resp, stealStatistics(1UL));
  } catch (std::exception const& ex) {
    LOG_TOPIC(WARN, Logger::COMMUNICATION)
        << "addSimpleResponse received an exception, closing connection:" << ex.what();
    _closeRequested = true;
  } catch (...) {
    LOG_TOPIC(WARN, Logger::COMMUNICATION)
        << "addSimpleResponse received an exception, closing connection";
    _closeRequested = true;
  }
}

void HttpCommTask::addResponse(GeneralResponse& baseResponse, RequestStatistics* stat) {
  TRI_ASSERT(_peer->runningInThisThread());

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  HttpResponse& response = dynamic_cast<HttpResponse&>(baseResponse);
#else
  HttpResponse& response = static_cast<HttpResponse&>(baseResponse);
#endif

  finishExecution(baseResponse);

  // response has been queued, allow further requests
  _requestPending = false;
  
  resetKeepAlive();

  // CORS response handling
  if (!_origin.empty()) {
    // the request contained an Origin header. We have to send back the
    // access-control-allow-origin header now
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "handling CORS response";

    // send back original value of "Origin" header
    response.setHeaderNCIfNotSet(StaticStrings::AccessControlAllowOrigin, _origin);

    // send back "Access-Control-Allow-Credentials" header
    response.setHeaderNCIfNotSet(StaticStrings::AccessControlAllowCredentials,
                                 (_denyCredentials ? "false" : "true"));

    // use "IfNotSet" here because we should not override HTTP headers set
    // by Foxx applications
    response.setHeaderNCIfNotSet(StaticStrings::AccessControlExposeHeaders,
                                 StaticStrings::ExposedCorsHeaders);
  }

  if (!ServerState::instance()->isDBServer()) {
    // DB server is not user-facing, and does not need to set this header
    // use "IfNotSet" to not overwrite an existing response header
    response.setHeaderNCIfNotSet(StaticStrings::XContentTypeOptions, StaticStrings::NoSniff);
  }

  // set "connection" header, keep-alive is the default
  response.setConnectionType(_closeRequested ? rest::ConnectionType::C_CLOSE
                                             : rest::ConnectionType::C_KEEP_ALIVE);

  size_t const responseBodyLength = response.bodySize();

  if (_requestType == rest::RequestType::HEAD) {
    // clear body if this is an HTTP HEAD request
    // HEAD must not return a body
    response.headResponse(responseBodyLength);
  }

  // reserve a buffer with some spare capacity
  WriteBuffer buffer(leaseStringBuffer(responseBodyLength + 220), stat);

  // write header
  response.writeHeader(buffer._buffer);

  // write body
  if (_requestType != rest::RequestType::HEAD) {
    buffer._buffer->appendText(response.body());
  }

  buffer._buffer->ensureNullTerminated();

  if (!buffer._buffer->empty()) {
    LOG_TOPIC(TRACE, Logger::REQUESTS)
        << "\"http-request-response\",\"" << (void*)this << "\",\""
        << (Logger::logRequestParameters()
                ? _fullUrl
                : _fullUrl.substr(0, _fullUrl.find_first_of('?')))
        << "\",\""
        << (Logger::logRequestParameters()
                ? StringUtils::escapeUnicode(std::string(buffer._buffer->c_str(),
                                                         buffer._buffer->length()))
                : "--body--")
        << "\"";
  }

  // append write buffer and statistics
  double const totalTime = RequestStatistics::ELAPSED_SINCE_READ_START(stat);

  if (stat != nullptr &&
      arangodb::Logger::isEnabled(arangodb::LogLevel::TRACE, Logger::REQUESTS)) {
    LOG_TOPIC(TRACE, Logger::REQUESTS)
        << "\"http-request-statistics\",\"" << (void*)this << "\",\""
        << _connectionInfo.clientAddress << "\",\""
        << HttpRequest::translateMethod(_requestType) << "\",\""
        << HttpRequest::translateVersion(_protocolVersion) << "\","
        << static_cast<int>(response.responseCode()) << ","
        << _originalBodyLength << "," << responseBodyLength << ",\""
        << (Logger::logRequestParameters()
                ? _fullUrl
                : _fullUrl.substr(0, _fullUrl.find_first_of('?')))
        << "\"," << stat->timingsCsv();
  }
  addWriteBuffer(std::move(buffer));
  // read pipelined requests
  triggerProcessAll();

  // and give some request information
  LOG_TOPIC(INFO, Logger::REQUESTS)
      << "\"http-request-end\",\"" << (void*)this << "\",\"" << _connectionInfo.clientAddress
      << "\",\"" << HttpRequest::translateMethod(_requestType) << "\",\""
      << HttpRequest::translateVersion(_protocolVersion) << "\","
      << static_cast<int>(response.responseCode()) << "," << _originalBodyLength
      << "," << responseBodyLength << ",\""
      << (Logger::logRequestParameters()
              ? _fullUrl
              : _fullUrl.substr(0, _fullUrl.find_first_of('?')))
      << "\"," << Logger::FIXED(totalTime, 6);

  std::unique_ptr<basics::StringBuffer> body = response.stealBody();
  returnStringBuffer(body.release());  // takes care of deleting
}

// reads data from the socket
// caller must hold the _lock
bool HttpCommTask::processRead(double startTime) {
  TRI_ASSERT(_peer->runningInThisThread());

  TRI_ASSERT(_readBuffer.c_str() != nullptr);

  if (_requestPending) {
    return false;
  }
  
  // starts and extends the keep-alive timeout
  resetKeepAlive(); // will extend the Keep-Alive timeout

  RequestStatistics* stat = nullptr;
  bool handleRequest = false;

  // still trying to read the header fields
  if (!_readRequestBody) {
    char const* ptr = _readBuffer.c_str() + _readPosition;
    char const* etr = _readBuffer.end();

    if (ptr == etr) {
      return false;
    }

    // starting a new request
    if (_newRequest) {
      // acquire a new statistics entry for the request
      stat = acquireStatistics(1UL);
      RequestStatistics::SET_READ_START(stat, startTime);

      _newRequest = false;
      _startPosition = _readPosition;
      _protocolVersion = rest::ProtocolVersion::UNKNOWN;
      _requestType = rest::RequestType::ILLEGAL;
      _fullUrl = "";
      _denyCredentials = true;

      _sinceCompactification++;
    }

    char const* end = etr - 3;

    // read buffer contents are way too small. we can exit here directly
    if (ptr >= end) {
      return false;
    }

    // check for the end of the request
    for (; ptr < end; ptr++) {
      if (ptr[0] == '\r' && ptr[1] == '\n' && ptr[2] == '\r' && ptr[3] == '\n') {
        break;
      }
    }

    // check if header is too large
    size_t headerLength = ptr - (_readBuffer.c_str() + _startPosition);

    if (headerLength > MaximalHeaderSize) {
      // header is too large
      addSimpleResponse(rest::ResponseCode::REQUEST_HEADER_FIELDS_TOO_LARGE,
                        rest::ContentType::UNSET, 1, VPackBuffer<uint8_t>());
      _closeRequested = true;
      return false;
    }

    if (_readBuffer.length() >= 11 &&
        (std::memcmp(_readBuffer.c_str(), "VST/1.0\r\n\r\n", 11) == 0 ||
         std::memcmp(_readBuffer.c_str(), "VST/1.1\r\n\r\n", 11) == 0)) {
      LOG_TOPIC(TRACE, Logger::COMMUNICATION) << "switching from HTTP to VST";
      ProtocolVersion protocolVersion = _readBuffer.c_str()[6] == '0'
                                            ? ProtocolVersion::VST_1_0
                                            : ProtocolVersion::VST_1_1;

      // mark task as abandoned, no more reads will happen on _peer
      if (!abandon()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "task is already abandoned");
      }

      std::shared_ptr<GeneralCommTask> commTask =
          std::make_shared<VstCommTask>(_scheduler, _server, std::move(_peer),
                                        std::move(_connectionInfo),
                                        GeneralServerFeature::keepAliveTimeout(),
                                        protocolVersion, /*skipSocketInit*/ true);
      commTask->addToReadBuffer(_readBuffer.c_str() + 11, _readBuffer.length() - 11);
      commTask->processAll();
      commTask->start();
      return false;
    }

    // header is complete
    if (ptr < end) {
      _readPosition = ptr - _readBuffer.c_str() + 4;

      char const* sptr = _readBuffer.c_str() + _startPosition;
      size_t slen = _readPosition - _startPosition;

      if (slen == 11 && std::memcmp(sptr, "VST/1.1\r\n\r\n", 11) == 0) {
        LOG_TOPIC(WARN, arangodb::Logger::FIXME)
            << "got VST request on HTTP port";
        _closeRequested = true;
        return false;
      }

      LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
          << "HTTP READ FOR " << (void*)this << ": " << std::string(sptr, slen);

      // check that we know, how to serve this request and update the connection
      // information, i. e. client and server addresses and ports and create a
      // request context for that request
      _incompleteRequest.reset(new HttpRequest(_connectionInfo, sptr, slen, _allowMethodOverride));
      _incompleteRequest->setClientTaskId(_taskId);

      // check HTTP protocol version
      _protocolVersion = _incompleteRequest->protocolVersion();

      if (_protocolVersion != rest::ProtocolVersion::HTTP_1_0 &&
          _protocolVersion != rest::ProtocolVersion::HTTP_1_1) {
        addSimpleResponse(rest::ResponseCode::HTTP_VERSION_NOT_SUPPORTED,
                          rest::ContentType::UNSET, 1, VPackBuffer<uint8_t>());
        _closeRequested = true;
        return false;
      }

      // check max URL length
      _fullUrl = _incompleteRequest->fullUrl();

      if (_fullUrl.size() > 16384) {
        addSimpleResponse(rest::ResponseCode::REQUEST_URI_TOO_LONG,
                          rest::ContentType::UNSET, 1, VPackBuffer<uint8_t>());
        _closeRequested = true;
        return false;
      }

      // update the connection information, i. e. client and server addresses
      // and ports
      _incompleteRequest->setProtocol(_protocol);

      // set body start to current position
      _bodyPosition = _readPosition;
      _bodyLength = 0;

      // keep track of the original value of the "origin" request header (if
      // any), we need this value to handle CORS requests
      _origin = _incompleteRequest->header(StaticStrings::Origin);

      if (!_origin.empty()) {
        // default is to allow nothing
        _denyCredentials = true;

        // if the request asks to allow credentials, we'll check against the
        // configured whitelist of origins
        std::vector<std::string> const& accessControlAllowOrigins =
            GeneralServerFeature::accessControlAllowOrigins();

        if (!accessControlAllowOrigins.empty()) {
          if (accessControlAllowOrigins[0] == "*") {
            // special case: allow everything
            _denyCredentials = false;
          } else if (!_origin.empty()) {
            // copy origin string
            if (_origin[_origin.size() - 1] == '/') {
              // strip trailing slash
              auto result = std::find(accessControlAllowOrigins.begin(),
                                      accessControlAllowOrigins.end(),
                                      _origin.substr(0, _origin.size() - 1));
              _denyCredentials = (result == accessControlAllowOrigins.end());
            } else {
              auto result = std::find(accessControlAllowOrigins.begin(),
                                      accessControlAllowOrigins.end(), _origin);
              _denyCredentials = (result == accessControlAllowOrigins.end());
            }
          } else {
            TRI_ASSERT(_denyCredentials);
          }
        }
      }

      // store the original request's type. we need it later when responding
      // (original request object gets deleted before responding)
      _requestType = _incompleteRequest->requestType();

      stat = statistics(1UL);
      RequestStatistics::SET_REQUEST_TYPE(stat, _requestType);

      // handle different HTTP methods
      switch (_requestType) {
        case rest::RequestType::GET:
        case rest::RequestType::DELETE_REQ:
        case rest::RequestType::HEAD:
        case rest::RequestType::OPTIONS:
        case rest::RequestType::POST:
        case rest::RequestType::PUT:
        case rest::RequestType::PATCH: {
          // technically, sending a body for an HTTP DELETE request is not
          // forbidden, but it is not explicitly supported
          bool const expectContentLength =
              (_requestType == rest::RequestType::POST ||
               _requestType == rest::RequestType::PUT ||
               _requestType == rest::RequestType::PATCH ||
               _requestType == rest::RequestType::OPTIONS ||
               _requestType == rest::RequestType::DELETE_REQ);

          if (!checkContentLength(_incompleteRequest.get(), expectContentLength)) {
            _closeRequested = true;
            return false;
          }

          if (_bodyLength == 0) {
            handleRequest = true;
          }

          break;
        }

        default: {
          // bad request, method not allowed
          addSimpleResponse(rest::ResponseCode::METHOD_NOT_ALLOWED,
                            rest::ContentType::UNSET, 1, VPackBuffer<uint8_t>());

          _closeRequested = true;
          return false;
        }
      }

      // check for a 100-continue
      if (_readRequestBody) {
        bool found;
        std::string const& expect =
            _incompleteRequest->header(StaticStrings::Expect, found);

        if (found && StringUtils::trim(expect) == "100-continue") {
          LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
              << "received a 100-continue request";

          WriteBuffer buffer(new StringBuffer(false), nullptr);
          buffer._buffer->appendText(
              TRI_CHAR_LENGTH_PAIR("HTTP/1.1 100 (Continue)\r\n\r\n"));
          buffer._buffer->ensureNullTerminated();
          addWriteBuffer(std::move(buffer));
          triggerProcessAll();  // read pipelined requests
        }
      }
    } else {
      size_t l = (_readBuffer.end() - _readBuffer.c_str());

      if (_startPosition + 4 <= l) {
        _readPosition = l - 4;
      }
    }
  }

  // readRequestBody might have changed, so cannot use else
  if (_readRequestBody) {
    if (_readBuffer.length() - _bodyPosition < _bodyLength) {
      // let client send more
      return false;
    }

    bool handled = false;
    std::string const& encoding = _incompleteRequest->header(StaticStrings::ContentEncoding);
    if (!encoding.empty()) {
      if (encoding == "gzip") {
        std::string uncompressed;
        if (!StringUtils::gzipUncompress(_readBuffer.c_str() + _bodyPosition,
                                         _bodyLength, uncompressed)) {
          addErrorResponse(rest::ResponseCode::BAD,
                           _incompleteRequest->contentTypeResponse(), 1,
                           TRI_ERROR_BAD_PARAMETER, "gzip decoding error");
          return false;
        }
        _incompleteRequest->setBody(uncompressed.c_str(), uncompressed.size());
        handled = true;
      } else if (encoding == "deflate") {
        std::string uncompressed;
        if (!StringUtils::gzipDeflate(_readBuffer.c_str() + _bodyPosition,
                                      _bodyLength, uncompressed)) {
          addErrorResponse(rest::ResponseCode::BAD,
                           _incompleteRequest->contentTypeResponse(), 1,
                           TRI_ERROR_BAD_PARAMETER, "gzip deflate error");
          return false;
        }
        _incompleteRequest->setBody(uncompressed.c_str(), uncompressed.size());
        handled = true;
      }
    }

    if (!handled) {
      // read "bodyLength" from read buffer and add this body to "httpRequest"
      _incompleteRequest->setBody(_readBuffer.c_str() + _bodyPosition, _bodyLength);
    }

    LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
        << std::string(_readBuffer.c_str() + _bodyPosition, _bodyLength);

    // remove body from read buffer and reset read position
    _readRequestBody = false;
    handleRequest = true;
  }

  if (!handleRequest) {
    return false;
  }

  auto bytes = _bodyPosition - _startPosition + _bodyLength;

  stat = statistics(1UL);
  RequestStatistics::SET_READ_END(stat);
  RequestStatistics::ADD_RECEIVED_BYTES(stat, bytes);

  bool const isOptionsRequest = (_requestType == rest::RequestType::OPTIONS);
  resetState();

  // .............................................................................
  // keep-alive handling
  // .............................................................................

  // header value can have any case. we'll lower-case it now
  std::string connectionType =
      StringUtils::tolower(_incompleteRequest->header(StaticStrings::Connection));

  if (connectionType == "close") {
    // client has sent an explicit "Connection: Close" header. we should close
    // the connection
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME)
        << "connection close requested by client";
    _closeRequested = true;
  } else if (_incompleteRequest->isHttp10() && connectionType != "keep-alive") {
    // HTTP 1.0 request, and no "Connection: Keep-Alive" header sent
    // we should close the connection
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME)
        << "no keep-alive, connection close requested by client";
    _closeRequested = true;
  } else if (!_useKeepAliveTimer) {
    // if keepAliveTimeout was set to 0.0, we'll close even keep-alive
    // connections immediately
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "keep-alive disabled by admin";
    _closeRequested = true;
  }

  // we keep the connection open in all other cases (HTTP 1.1 or Keep-Alive
  // header sent)

  // .............................................................................
  // CORS
  // .............................................................................

  // OPTIONS requests currently go unauthenticated
  if (isOptionsRequest) {
    // handle HTTP OPTIONS requests directly
    processCorsOptions(std::move(_incompleteRequest));
    _incompleteRequest.reset(nullptr);
    return true;
  }

  // .............................................................................
  // authenticate
  // .............................................................................

  // first scrape the auth headers and try to determine and authenticate the user
  rest::ResponseCode authResult = handleAuthHeader(_incompleteRequest.get());

  // authenticated
  if (authResult != rest::ResponseCode::SERVER_ERROR) {
    // prepare execution will send an error message
    RequestFlow cont = prepareExecution(*_incompleteRequest.get());
    if (cont == RequestFlow::Continue) {
      processRequest(std::move(_incompleteRequest));
    }

  } else {
    std::string realm = "Bearer token_type=\"JWT\", realm=\"ArangoDB\"";
    HttpResponse resp(rest::ResponseCode::UNAUTHORIZED, leaseStringBuffer(0));
    resp.setHeaderNC(StaticStrings::WwwAuthenticate, std::move(realm));
    addResponse(resp, nullptr);
  }

  _incompleteRequest.reset(nullptr);
  return true;
}

void HttpCommTask::processRequest(std::unique_ptr<HttpRequest> request) {
  TRI_ASSERT(_peer->runningInThisThread());
  cancelKeepAlive(); // timeout will be restarted

  {
    LOG_TOPIC(DEBUG, Logger::REQUESTS)
        << "\"http-request-begin\",\"" << (void*)this << "\",\""
        << _connectionInfo.clientAddress << "\",\""
        << HttpRequest::translateMethod(_requestType) << "\",\""
        << HttpRequest::translateVersion(_protocolVersion) << "\",\""
        << (Logger::logRequestParameters()
                ? _fullUrl
                : _fullUrl.substr(0, _fullUrl.find_first_of('?')))
        << "\"";

    std::string const& body = request->body();

    if (!body.empty() && Logger::isEnabled(LogLevel::TRACE, Logger::REQUESTS) &&
        Logger::logRequestParameters()) {
      LOG_TOPIC(TRACE, Logger::REQUESTS)
          << "\"http-request-body\",\"" << (void*)this << "\",\""
          << (StringUtils::escapeUnicode(body)) << "\"";
    }
  }

  // create a handler and execute
  auto resp = std::make_unique<HttpResponse>(rest::ResponseCode::SERVER_ERROR,
                                             leaseStringBuffer(1024));
  resp->setContentType(request->contentTypeResponse());
  resp->setContentTypeRequested(request->contentTypeResponse());

  executeRequest(std::move(request), std::move(resp));
}

////////////////////////////////////////////////////////////////////////////////
/// check the content-length header of a request and fail it is broken
////////////////////////////////////////////////////////////////////////////////

bool HttpCommTask::checkContentLength(HttpRequest* request, bool expectContentLength) {
  int64_t const bodyLength = request->contentLength();

  if (bodyLength < 0) {
    // bad request, body length is < 0. this is a client error
    addSimpleResponse(rest::ResponseCode::LENGTH_REQUIRED,
                      rest::ContentType::UNSET, 1, VPackBuffer<uint8_t>());
    return false;
  }

  if (!expectContentLength && bodyLength > 0) {
    // content-length header was sent but the request method does not support
    // that we'll warn but read the body anyway
    LOG_TOPIC(WARN, arangodb::Logger::FIXME)
        << "received HTTP GET/HEAD request with content-length, this "
           "should not happen";
  }

  if ((size_t)bodyLength > MaximalBodySize) {
    // request entity too large
    addSimpleResponse(rest::ResponseCode::REQUEST_ENTITY_TOO_LARGE,
                      rest::ContentType::UNSET, 1, VPackBuffer<uint8_t>());
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

void HttpCommTask::processCorsOptions(std::unique_ptr<HttpRequest> request) {
  HttpResponse resp(rest::ResponseCode::OK, leaseStringBuffer(0));

  resp.setHeaderNCIfNotSet(StaticStrings::Allow, StaticStrings::CorsMethods);

  if (!_origin.empty()) {
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "got CORS preflight request";
    std::string const allowHeaders =
        StringUtils::trim(request->header(StaticStrings::AccessControlRequestHeaders));

    // send back which HTTP methods are allowed for the resource
    // we'll allow all
    resp.setHeaderNCIfNotSet(StaticStrings::AccessControlAllowMethods,
                             StaticStrings::CorsMethods);

    if (!allowHeaders.empty()) {
      // allow all extra headers the client requested
      // we don't verify them here. the worst that can happen is that the
      // client sends some broken headers and then later cannot access the data
      // on
      // the server. that's a client problem.
      resp.setHeaderNCIfNotSet(StaticStrings::AccessControlAllowHeaders, allowHeaders);

      LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
          << "client requested validation of the following headers: " << allowHeaders;
    }

    // set caching time (hard-coded value)
    resp.setHeaderNCIfNotSet(StaticStrings::AccessControlMaxAge, StaticStrings::N1800);
  }

  addResponse(resp, nullptr);
}

std::unique_ptr<GeneralResponse> HttpCommTask::createResponse(rest::ResponseCode responseCode,
                                                              uint64_t /* messageId */) {
  return std::make_unique<HttpResponse>(responseCode, leaseStringBuffer(0));
}

void HttpCommTask::compactify() {
  if (!_newRequest) {
    return;
  }

  bool compact = false;

  if (_sinceCompactification > RunCompactEvery) {
    compact = true;
  } else if (_readBuffer.length() > MaximalPipelineSize) {
    compact = true;
  }

  if (compact) {
    _readBuffer.erase_front(_readPosition);
  } else if (_readPosition == _readBuffer.length()) {
    _readBuffer.reset();
    compact = true;
  }

  if (compact) {
    _sinceCompactification = 0;

    if (_startPosition > 0) {
      TRI_ASSERT(_startPosition >= _readPosition);
      _startPosition -= _readPosition;
    }

    if (_bodyPosition > 0) {
      TRI_ASSERT(_bodyPosition >= _readPosition);
      _bodyPosition -= _readPosition;
    }

    _readPosition = 0;
  }
}

void HttpCommTask::resetState() {
  _requestPending = true;

  _readPosition = _bodyPosition + _bodyLength;

  _bodyPosition = 0;
  _bodyLength = 0;
  _startPosition = 0;

  _newRequest = true;
  _readRequestBody = false;
}

ResponseCode HttpCommTask::handleAuthHeader(HttpRequest* req) const {
  bool found;
  std::string const& authStr = req->header(StaticStrings::Authorization, found);
  if (!found) {
    if (_auth->isActive()) {
      events::CredentialsMissing(req);
      return rest::ResponseCode::UNAUTHORIZED;
    }
    return rest::ResponseCode::OK;
  }

  size_t methodPos = authStr.find_first_of(' ');
  if (methodPos != std::string::npos) {
    // skip over authentication method
    char const* auth = authStr.c_str() + methodPos;
    while (*auth == ' ') {
      ++auth;
    }

    if (Logger::logRequestParameters()) {
      LOG_TOPIC(DEBUG, arangodb::Logger::REQUESTS)
          << "\"authorization-header\",\"" << (void*)this << "\",\"" << authStr << "\"";
    }

    try {
      // note that these methods may throw in case of an error
      AuthenticationMethod authMethod = AuthenticationMethod::NONE;
      if (TRI_CaseEqualString(authStr.c_str(), "basic ", 6)) {
        authMethod = AuthenticationMethod::BASIC;
      } else if (TRI_CaseEqualString(authStr.c_str(), "bearer ", 7)) {
        authMethod = AuthenticationMethod::JWT;
      }

      req->setAuthenticationMethod(authMethod);
      if (authMethod != AuthenticationMethod::NONE) {
        auto entry = _auth->tokenCache().checkAuthentication(authMethod, auth);
        req->setAuthenticated(entry.authenticated());
        req->setUser(std::move(entry._username));
      }

      if (req->authenticated() || !_auth->isActive()) {
        events::Authenticated(req, authMethod);
        return rest::ResponseCode::OK;
      } else if (_auth->isActive()) {
        events::CredentialsBad(req, authMethod);
        return rest::ResponseCode::UNAUTHORIZED;
      }

      // intentionally falls through
    } catch (arangodb::basics::Exception const& ex) {
      // translate error
      if (ex.code() == TRI_ERROR_USER_NOT_FOUND) {
        return rest::ResponseCode::UNAUTHORIZED;
      }
      return GeneralResponse::responseCode(ex.what());
    } catch (...) {
      return rest::ResponseCode::SERVER_ERROR;
    }
  }

  events::UnknownAuthenticationMethod(req);
  return rest::ResponseCode::UNAUTHORIZED;
}
