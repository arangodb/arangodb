////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "HttpCommTask.h"

#include "Basics/tri-strings.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/RestHandler.h"
#include "GeneralServer/RestHandlerFactory.h"
//#include "GeneralServer/VstCommTask.h"
#include "Meta/conversion.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "Statistics/ConnectionStatistics.h"
#include "Statistics/RequestStatistics.h"
#include "Utils/Events.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

namespace {
  using namespace arangodb;
  using namespace arangodb::rest;
  
  constexpr static size_t MaximalBodySize = 1024 * 1024 * 1024; // 1024 MB
  
  rest::RequestType llhttpToRequestType(llhttp_t* p) {
    switch (p->method) {
      case HTTP_DELETE:
        return RequestType::DELETE_REQ;
      case HTTP_GET:
        return RequestType::GET;
      case HTTP_HEAD:
        return RequestType::HEAD;
      case HTTP_POST:
        return RequestType::POST;
      case HTTP_PUT:
        return RequestType::PUT;
      case HTTP_OPTIONS:
        return RequestType::OPTIONS;
      default:
        return RequestType::ILLEGAL;
    }
  }
}  // namespace

template <SocketType T>
int HttpCommTask<T>::on_message_began(llhttp_t* p) {
  HttpCommTask<T>* self = static_cast<HttpCommTask<T>*>(p->data);
  self->_lastHeaderValue.clear();
  self->_lastHeaderValue.clear();
  self->_origin.clear();
  self->_request = std::make_unique<HttpRequest>(self->_connectionInfo, /*header*/nullptr, 0,
                                                /*allowMethodOverride*/true);
  self->_last_header_was_a_value = false;
  self->_should_keep_alive = false;
  self->_message_complete = false;
  self->_denyCredentials = false;
  return HPE_OK;
}

template <SocketType T>
int HttpCommTask<T>::on_url(llhttp_t* p, const char* at, size_t len) {
  HttpCommTask<T>* self = static_cast<HttpCommTask<T>*>(p->data);
  self->_request->parseUrl(at, len);
  self->_request->setRequestType(llhttpToRequestType(p));
  if (self->_request->requestType() == RequestType::ILLEGAL) {
    self->addSimpleResponse(rest::ResponseCode::METHOD_NOT_ALLOWED,
                            rest::ContentType::UNSET, 1, VPackBuffer<uint8_t>());
    return HPE_USER;
  }
  return HPE_OK;
}

template <SocketType T>
int HttpCommTask<T>::on_status(llhttp_t* p, const char* at, size_t len) {
//    HttpParserState* data = static_cast<HttpParserState*>(parser->data);
//    data->request->a
//
//    data->_response->header.meta.emplace(std::string("http/") +
//                                         std::to_string(parser->http_major) + '.' +
//                                         std::to_string(parser->http_minor),
//                                         std::string(at, len));
  return HPE_OK;
}

template <SocketType T>
int HttpCommTask<T>::on_header_field(llhttp_t* p, const char* at, size_t len) {
  HttpCommTask<T>* self = static_cast<HttpCommTask<T>*>(p->data);
  if (self->_last_header_was_a_value) {
    StringUtils::tolowerInPlace(&self->_lastHeaderField);
    self->_request->setHeader(self->_lastHeaderField,
                              self->_lastHeaderValue);
    self->_lastHeaderField.assign(at, len);
  } else {
    self->_lastHeaderField.append(at, len);
  }
  self->_last_header_was_a_value = false;
  return HPE_OK;
}

template <SocketType T>
int HttpCommTask<T>::on_header_value(llhttp_t* p, const char* at, size_t len) {
  HttpCommTask<T>* self = static_cast<HttpCommTask<T>*>(p->data);
  if (self->_last_header_was_a_value) {
    self->_lastHeaderValue.append(at, len);
  } else {
    self->_lastHeaderValue.assign(at, len);
  }
  self->_last_header_was_a_value = true;
  return HPE_OK;
}

template <SocketType T>
int HttpCommTask<T>::on_header_complete(llhttp_t* p) {
  HttpCommTask<T>* self = static_cast<HttpCommTask<T>*>(p->data);
  if (!self->_lastHeaderField.empty()) {
    StringUtils::tolowerInPlace(&self->_lastHeaderField);
    self->_request->setHeader(self->_lastHeaderField, self->_lastHeaderValue);
  }
  
  if ((p->http_major != 1 && p->http_minor != 0) &&
      (p->http_major != 1 && p->http_minor != 1)) {
    self->addSimpleResponse(rest::ResponseCode::HTTP_VERSION_NOT_SUPPORTED,
                            rest::ContentType::UNSET, 1, VPackBuffer<uint8_t>());
    return HPE_USER;
  }
  if (p->content_length > MaximalBodySize) {
    self->addSimpleResponse(rest::ResponseCode::REQUEST_ENTITY_TOO_LARGE,
                            rest::ContentType::UNSET, 1, VPackBuffer<uint8_t>());
    return HPE_USER;
  }
  if (p->content_length > 0) {
    uint64_t maxReserve = std::min<uint64_t>(2 << 25, p->content_length);
    self->_request->body().reserve(maxReserve + 1);
  }
  self->_should_keep_alive = llhttp_should_keep_alive(p);
  
  bool found;
  std::string const& expect = self->_request->header(StaticStrings::Expect, found);
  if (found && StringUtils::trim(expect) == "100-continue") {
    LOG_TOPIC("2b604", TRACE, arangodb::Logger::FIXME)
        << "received a 100-continue request";
    char const* response = "HTTP/1.1 100 Continue\r\n\r\n";
    auto buff = asio_ns::buffer(response, strlen(response));
    asio_ns::async_write(self->_protocol->socket, buff,
                         [self](asio_ns::error_code const& ec,
                                std::size_t transferred) {
                           llhttp_resume(&self->_parser);
                           self->asyncReadSome();
                         });
    return HPE_PAUSED;
  }
  if (self->_request->requestType() == RequestType::HEAD) {
    self->_message_complete = true;
    return 1; // tells the parser not to expect a body
  }
  return HPE_OK;
}

template <SocketType T>
int HttpCommTask<T>::on_body(llhttp_t* p, const char* at, size_t len) {
  HttpCommTask<T>* self = static_cast<HttpCommTask<T>*>(p->data);
  self->_request->body().append(at, len);

//    static_cast<RequestItem*>(parser->data)->_responseBuffer.append(at, len);
  return HPE_OK;
}

template <SocketType T>
int HttpCommTask<T>::on_message_complete(llhttp_t* p) {
  static_cast<HttpCommTask<T>*>(p->data)->_message_complete = true;
  return HPE_PAUSED;
}

template <SocketType T>
HttpCommTask<T>::HttpCommTask(GeneralServer& server,
                              std::unique_ptr<AsioSocket<T>> proto,
                              ConnectionInfo info)
  : GeneralCommTask(server, "HttpCommTask", std::move(info)),
    _protocol(std::move(proto)),
    _keepAliveTimer(_protocol->context.io_context) {
  ConnectionStatistics::SET_HTTP(this->_connectionStatistics);
  
  // initialize http parsing code
  llhttp_settings_init(&_parserSettings);
  _parserSettings.on_message_begin = HttpCommTask<T>::on_message_began;
  _parserSettings.on_url = HttpCommTask<T>::on_url;
  _parserSettings.on_status = HttpCommTask<T>::on_status;
  _parserSettings.on_header_field = HttpCommTask<T>::on_header_field;
  _parserSettings.on_header_value = HttpCommTask<T>::on_header_value;
  _parserSettings.on_headers_complete = HttpCommTask<T>::on_header_complete;
  _parserSettings.on_body = HttpCommTask<T>::on_body;
  _parserSettings.on_message_complete = HttpCommTask<T>::on_message_complete;
  llhttp_init(&_parser, HTTP_REQUEST, &_parserSettings);
  _parser.data = this;
}

template <SocketType T>
HttpCommTask<T>::~HttpCommTask() {}

/// @brief send error response including response body
template <SocketType T>
void HttpCommTask<T>::addSimpleResponse(rest::ResponseCode code,
                                        rest::ContentType respType, uint64_t /*messageId*/,
                                        velocypack::Buffer<uint8_t>&& buffer) {
  try {
    HttpResponse resp(code, nullptr);
    resp.setContentType(respType);
    if (!buffer.empty()) {
      resp.setPayload(std::move(buffer), true, VPackOptions::Defaults);
    }
    addResponse(resp, stealStatistics(1UL));
  } catch (std::exception const& ex) {
    LOG_TOPIC("233e6", WARN, Logger::COMMUNICATION)
        << "addSimpleResponse received an exception, closing connection:" << ex.what();
    this->close();
  } catch (...) {
    LOG_TOPIC("fc831", WARN, Logger::COMMUNICATION)
        << "addSimpleResponse received an exception, closing connection";
   this->close();
  }
}

template <SocketType T>
void HttpCommTask<T>::start() {
  _protocol->setNonBlocking(true);
  this->asyncReadSome();
}

template <SocketType T>
void HttpCommTask<T>::close() {
  if (_protocol) {
    asio_ns::error_code ec;
    _protocol->shutdown(ec);
    if (ec) {
      LOG_DEVEL << ec.message();
    }
  }
}

template <SocketType T>
void HttpCommTask<T>::asyncReadSome() {
  TRI_ASSERT(llhttp_get_errno(&_parser) == HPE_OK);
  // first read pipelined requests
  if (_receiveBuffer.size() > 5 &&
      !readCallback(asio_ns::error_code(), 0)) {
    LOG_DEVEL << "reading pipelined request";
    return;
  }
  
#warning TODO sync read first
  auto cb = [this](asio_ns::error_code const& ec, size_t transferred) {
    // received data is "committed" from output sequence to input sequence
    _receiveBuffer.commit(transferred);
    if (readCallback(ec, transferred)) {
      asyncReadSome();
    }
  };
  
  // reserve 32kB in output buffer
  auto mutableBuff = _receiveBuffer.prepare(READ_BLOCK_SIZE);
  _protocol->socket.async_read_some(mutableBuff, std::move(cb));
}

template <SocketType T>
bool HttpCommTask<T>::readCallback(asio_ns::error_code ec, size_t transferred) {
  
  llhttp_errno_t err = HPE_OK;
  if (ec) {
    if (ec == asio_ns::error::misc_errors::eof) {
      err = llhttp_finish(&_parser);
    } else {
      LOG_TOPIC("395fe", DEBUG, Logger::REQUESTS)
      << "Error while reading from socket: '" << ec.message() << "'";
      err = HPE_CLOSED_CONNECTION;
    }
  } else {
    LOG_TOPIC("595fe", TRACE, Logger::REQUESTS)
    << "received " << transferred << " bytes\n";
    
    size_t parsedBytes = 0;
    // Inspect the data we've received so far.
    auto buffers = _receiveBuffer.data(); // no copy
    for (auto const& buffer : buffers) {
      const char* data = reinterpret_cast<const char *>(buffer.data());
      err = llhttp_execute(&_parser, data, buffer.size());
      if (err != HPE_OK) {
        parsedBytes += llhttp_get_error_pos(&_parser) - data;
        break;
      }
      parsedBytes += buffer.size();
    }
    // Remove consumed data from receive buffer.
    _receiveBuffer.consume(parsedBytes);
  }
  
  if (_message_complete) {
    _message_complete = false;
    TRI_ASSERT(err == HPE_PAUSED);
    processRequest(std::move(_request));
  } else if (err != HPE_OK ) {
    if (err == HPE_PAUSED_UPGRADE) {
      //    llhttp_resume_after_upgrade(&parser_);
      LOG_DEVEL << "received upgrade header";
      close();
    } else if (err != HPE_USER && err != HPE_PAUSED) {
      LOG_DEVEL << "HTTP parse failure: "
      << "(" << llhttp_errno_name(err) << ") "
      << llhttp_get_error_reason(&_parser);
      close();
    }
  }
  
  return err == HPE_OK;
}

template <SocketType T>
void HttpCommTask<T>::processRequest(std::unique_ptr<HttpRequest> request) {
  TRI_ASSERT(request);
  // ensure there is a null byte in there somewhere. Some RestHandlers
  // use C functions like strchr that except a C string as input
  request->body().push_back('\0');
  request->body().resetTo(request->body().size() - 1);
  
  {
    LOG_TOPIC("6e770", DEBUG, Logger::REQUESTS)
    << "\"http-request-begin\",\"" << (void*)this << "\",\""
    << _connectionInfo.clientAddress << "\",\""
    << HttpRequest::translateMethod(request->requestType()) << "\",\""
    << (Logger::logRequestParameters() ? request->fullUrl() : request->requestPath())
    << "\"";
    
    VPackStringRef body = request->rawPayload();
    if (!body.empty() && Logger::isEnabled(LogLevel::TRACE, Logger::REQUESTS) &&
        Logger::logRequestParameters()) {
      LOG_TOPIC("b9e76", TRACE, Logger::REQUESTS)
      << "\"http-request-body\",\"" << (void*)this << "\",\""
      << StringUtils::escapeUnicode(body.toString()) << "\"";
    }
  }
  
  // handle origin headers
  _origin = request->header(StaticStrings::Origin);
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
  // OPTIONS requests currently go unauthenticated
  if (request->requestType() == rest::RequestType::OPTIONS) {
    processCorsOptions(std::move(request));
    return;
  }
  
  // .............................................................................
  // authenticate
  // .............................................................................
  
  // first scrape the auth headers and try to determine and authenticate the
  // user
  rest::ResponseCode authResult = handleAuthHeader(request.get());
  
  // authenticated
  if (authResult == rest::ResponseCode::SERVER_ERROR) {
    std::string realm = "Bearer token_type=\"JWT\", realm=\"ArangoDB\"";
    HttpResponse resp(rest::ResponseCode::UNAUTHORIZED, nullptr);
    resp.setHeaderNC(StaticStrings::WwwAuthenticate, std::move(realm));
    addResponse(resp, nullptr);
    return;
  }
  
  RequestFlow cont = prepareExecution(*request.get());
  if (cont == RequestFlow::Continue) {
    // create a handler and execute
    auto resp = std::make_unique<HttpResponse>(rest::ResponseCode::SERVER_ERROR,
                                               nullptr);
    resp->setContentType(request->contentTypeResponse());
    resp->setContentTypeRequested(request->contentTypeResponse());
    
    executeRequest(std::move(request), std::move(resp));
  } // prepare execution will send an error message
}

template <SocketType T>
void HttpCommTask<T>::processCorsOptions(std::unique_ptr<HttpRequest> request) {
  HttpResponse resp(rest::ResponseCode::OK, nullptr);
  
  resp.setHeaderNCIfNotSet(StaticStrings::Allow, StaticStrings::CorsMethods);

  if (!_origin.empty()) {
    LOG_TOPIC("e1cfa", TRACE, arangodb::Logger::FIXME) << "got CORS preflight request";
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

      LOG_TOPIC("55413", TRACE, arangodb::Logger::FIXME)
          << "client requested validation of the following headers: " << allowHeaders;
    }

    // set caching time (hard-coded value)
    resp.setHeaderNCIfNotSet(StaticStrings::AccessControlMaxAge, StaticStrings::N1800);
  }

  addResponse(resp, nullptr);
}

template <SocketType T>
std::unique_ptr<GeneralResponse> HttpCommTask<T>::createResponse(rest::ResponseCode responseCode,
                                                              uint64_t /* messageId */) {
  return std::make_unique<HttpResponse>(responseCode, nullptr);
}

template <SocketType T>
ResponseCode HttpCommTask<T>::handleAuthHeader(HttpRequest* req) {
  bool found;
  std::string const& authStr = req->header(StaticStrings::Authorization, found);
  if (!found) {
    if (_auth->isActive()) {
      events::CredentialsMissing(*req);
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
      LOG_TOPIC("c4536", DEBUG, arangodb::Logger::REQUESTS)
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
        _authToken = _auth->tokenCache().checkAuthentication(authMethod, auth);
        req->setAuthenticated(_authToken.authenticated());
        req->setUser(_authToken._username); // do copy here, so that we do not invalidate the member
      }

      if (req->authenticated() || !_auth->isActive()) {
        events::Authenticated(*req, authMethod);
        return rest::ResponseCode::OK;
      } else if (_auth->isActive()) {
        events::CredentialsBad(*req, authMethod);
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

  events::UnknownAuthenticationMethod(*req);
  return rest::ResponseCode::UNAUTHORIZED;
}


template <SocketType T>
void HttpCommTask<T>::addResponse(GeneralResponse& baseResponse, RequestStatistics* stat) {
  #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  HttpResponse& response = dynamic_cast<HttpResponse&>(baseResponse);
  #else
  HttpResponse& response = static_cast<HttpResponse&>(baseResponse);
  #endif

  finishExecution(baseResponse);
  _keepAliveTimer.cancel();
  
  // CORS response handling
  if (!_origin.empty()) {
    // the request contained an Origin header. We have to send back the
    // access-control-allow-origin header now
    LOG_TOPIC("ae603", TRACE, arangodb::Logger::FIXME) << "handling CORS response";

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

  // TODO lease buffers
  auto header = std::make_unique<VPackBuffer<uint8_t>>();
  header->reserve(220);
  
  header->append(TRI_CHAR_LENGTH_PAIR("HTTP/1.1 "));
  header->append(GeneralResponse::responseString(response.responseCode()));
  header->append("\r\n", 2);

  bool seenServerHeader = false;
  bool seenConnectionHeader = false;
  for (auto const& it : response.headers()) {
    std::string const& key = it.first;
    size_t const keyLength = key.size();
    
    // ignore content-length
    if (key == StaticStrings::ContentLength ||
        key == StaticStrings::TransferEncoding) {
      continue;
    }
    
    if (key == StaticStrings::Server) {
      seenServerHeader = true;
    } else if (key == StaticStrings::Connection) {
      seenConnectionHeader = true;
    }
    
    // reserve enough space for header name + ": " + value + "\r\n"
    header->reserve(key.size() + 2 + it.second.size() + 2);
    
    char const* p = key.data();
    char const* end = p + keyLength;
    int capState = 1;
    while (p < end) {
      if (capState == 1) {
        // upper case
        header->push_back(::toupper(*p));
        capState = 0;
      } else if (capState == 0) {
        // normal case
        header->push_back(::tolower(*p));
        if (*p == '-') {
          capState = 1;
        } else if (*p == ':') {
          capState = 2;
        }
      } else {
        // output as is
        header->push_back(*p);
      }
      ++p;
    }
    
    header->append(": ", 2);
    header->append(it.second);
    header->append("\r\n", 2);
  }
  
  // add "Server" response header
  if (!seenServerHeader && !HttpResponse::HIDE_PRODUCT_HEADER) {
    header->append(TRI_CHAR_LENGTH_PAIR("Server: ArangoDB\r\n"));
  }
  if (_should_keep_alive) {
    header->append(TRI_CHAR_LENGTH_PAIR("Connection: Keep-Alive\r\n"));
  } else {
    header->append(TRI_CHAR_LENGTH_PAIR("Connection: Close\r\n"));
  }
  
  // add "Content-Type" header
  switch (response.contentType()) {
    case ContentType::UNSET:
    case ContentType::JSON:
      header->append(TRI_CHAR_LENGTH_PAIR("Content-Type: application/json; charset=utf-8\r\n"));
      break;
    case ContentType::VPACK:
      header->append(TRI_CHAR_LENGTH_PAIR("Content-Type: application/x-velocypack\r\n"));
      break;
    case ContentType::TEXT:
      header->append(TRI_CHAR_LENGTH_PAIR("Content-Type: text/plain; charset=utf-8\r\n"));
      break;
    case ContentType::HTML:
      header->append(TRI_CHAR_LENGTH_PAIR("Content-Type: text/html; charset=utf-8\r\n"));
      break;
    case ContentType::DUMP:
      header->append(TRI_CHAR_LENGTH_PAIR("Content-Type: application/x-arango-dump; charset=utf-8\r\n"));
      break;
    case ContentType::CUSTOM: // don't do anything
      break;
  }
  
  
  for (auto const& it : response.cookies()) {
    header->append(TRI_CHAR_LENGTH_PAIR("Set-Cookie: "));
    header->append(it);
    header->append("\r\n", 2);
  }
  
  header->append(TRI_CHAR_LENGTH_PAIR("Content-Length: "));
  header->append(std::to_string(response.bodySize()));
  header->append("\r\n\r\n", 4);

  std::unique_ptr<basics::StringBuffer> body = response.stealBody();
  // append write buffer and statistics
  double const totalTime = RequestStatistics::ELAPSED_SINCE_READ_START(stat);
  LOG_DEVEL << "elapsed " << totalTime;
  
  std::vector<asio_ns::const_buffer> buffers;
  buffers.reserve(2);
  buffers.emplace_back(header->data(), header->size());
  if (HTTP_HEAD != _parser.method) {
    buffers.emplace_back(body->data(), body->size());
  }
  
#warning perform sync write first
  
  auto cb = [this,
             h = std::move(header),
             b = std::move(body)](asio_ns::error_code ec,
                                  size_t transferred) {
    llhttp_errno_t err = llhttp_get_errno(&_parser);
    if (ec || !_should_keep_alive || err != HPE_PAUSED) {
      if (ec) {
        LOG_DEVEL << "boost error " << ec.message();
      }
      this->close();
    } else {
      llhttp_resume(&_parser);
      this->asyncReadSome();
    }
  };
  asio_ns::async_write(_protocol->socket, std::move(buffers), std::move(cb));
  
//  std::unique_ptr<basics::StringBuffer> body = response.stealBody();
//  returnStringBuffer(body.release());  // takes care of deleting
}

template class arangodb::rest::HttpCommTask<SocketType::Tcp>;
template class arangodb::rest::HttpCommTask<SocketType::Ssl>;
#ifndef _WIN32
template class arangodb::rest::HttpCommTask<SocketType::Unix>;
#endif
