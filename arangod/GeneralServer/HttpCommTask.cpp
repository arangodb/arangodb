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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "HttpCommTask.h"

#include "Basics/EncodingUtils.h"
#include "Basics/asio_ns.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/RestHandler.h"
#include "GeneralServer/RestHandlerFactory.h"
#include "GeneralServer/VstCommTask.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Meta/conversion.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "Statistics/ConnectionStatistics.h"
#include "Statistics/RequestStatistics.h"
#include "Utils/Events.h"

#include <cstring>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

namespace {
using namespace arangodb;
using namespace arangodb::rest;

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
    case HTTP_PATCH:
      return RequestType::PATCH;
    default:
      return RequestType::ILLEGAL;
  }
}
}  // namespace

template <SocketType T>
int HttpCommTask<T>::on_message_began(llhttp_t* p) {
  HttpCommTask<T>* self = static_cast<HttpCommTask<T>*>(p->data);
  self->_lastHeaderField.clear();
  self->_lastHeaderValue.clear();
  self->_origin.clear();
  self->_request = std::make_unique<HttpRequest>(self->_connectionInfo, /*header*/ nullptr,
                                                 0, self->_allowMethodOverride);
  self->_response.reset();
  self->_lastHeaderWasValue = false;
  self->_shouldKeepAlive = false;
  self->_messageDone = false;

  // acquire a new statistics entry for the request
  RequestStatistics* stat = self->acquireStatistics(1UL);
  RequestStatistics::SET_READ_START(stat, TRI_microtime());

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

  RequestStatistics* stat = self->statistics(1UL);
  RequestStatistics::SET_REQUEST_TYPE(stat, self->_request->requestType());

  return HPE_OK;
}

template <SocketType T>
int HttpCommTask<T>::on_status(llhttp_t* p, const char* at, size_t len) {
  // should not be used
  return HPE_OK;
}

template <SocketType T>
int HttpCommTask<T>::on_header_field(llhttp_t* p, const char* at, size_t len) {
  HttpCommTask<T>* self = static_cast<HttpCommTask<T>*>(p->data);
  if (self->_lastHeaderWasValue) {
    self->_request->setHeaderV2(std::move(self->_lastHeaderField),
                                std::move(self->_lastHeaderValue));
    self->_lastHeaderField.assign(at, len);
  } else {
    self->_lastHeaderField.append(at, len);
  }
  self->_lastHeaderWasValue = false;
  return HPE_OK;
}

template <SocketType T>
int HttpCommTask<T>::on_header_value(llhttp_t* p, const char* at, size_t len) {
  HttpCommTask<T>* self = static_cast<HttpCommTask<T>*>(p->data);
  if (self->_lastHeaderWasValue) {
    self->_lastHeaderValue.append(at, len);
  } else {
    self->_lastHeaderValue.assign(at, len);
  }
  self->_lastHeaderWasValue = true;
  return HPE_OK;
}

template <SocketType T>
int HttpCommTask<T>::on_header_complete(llhttp_t* p) {
  HttpCommTask<T>* self = static_cast<HttpCommTask<T>*>(p->data);
  if (!self->_lastHeaderField.empty()) {
    self->_request->setHeaderV2(std::move(self->_lastHeaderField),
                                std::move(self->_lastHeaderValue));
  }

  if ((p->http_major != 1 && p->http_minor != 0) &&
      (p->http_major != 1 && p->http_minor != 1)) {
    self->addSimpleResponse(rest::ResponseCode::HTTP_VERSION_NOT_SUPPORTED,
                            rest::ContentType::UNSET, 1, VPackBuffer<uint8_t>());
    return HPE_USER;
  }
  if (p->content_length > GeneralCommTask<T>::MaximalBodySize) {
    self->addSimpleResponse(rest::ResponseCode::REQUEST_ENTITY_TOO_LARGE,
                            rest::ContentType::UNSET, 1, VPackBuffer<uint8_t>());
    return HPE_USER;
  }
  if (p->content_length > 0) {
    // lets not reserve more than 64MB at once
    uint64_t maxReserve = std::min<uint64_t>(2 << 26, p->content_length);
    self->_request->body().reserve(maxReserve + 1);
  }
  self->_shouldKeepAlive = llhttp_should_keep_alive(p);

  bool found;
  std::string const& expect = self->_request->header(StaticStrings::Expect, found);
  if (found && StringUtils::trim(expect) == "100-continue") {
    LOG_TOPIC("2b604", TRACE, arangodb::Logger::REQUESTS)
        << "received a 100-continue request";
    char const* response = "HTTP/1.1 100 Continue\r\n\r\n";
    auto buff = asio_ns::buffer(response, strlen(response));
    asio_ns::async_write(self->_protocol->socket, buff,
                         [self = self->shared_from_this()](asio_ns::error_code const& ec,
                                                           std::size_t) {
                           if (ec) {
                             static_cast<HttpCommTask<T>*>(self.get())->close();
                           }
                         });
    return HPE_OK;
  }
  if (self->_request->requestType() == RequestType::HEAD) {
    // Assume that request/response has no body, proceed parsing next message
    return 1;  // 1 is defined by parser
  }
  return HPE_OK;
}

template <SocketType T>
int HttpCommTask<T>::on_body(llhttp_t* p, const char* at, size_t len) {
  HttpCommTask<T>* self = static_cast<HttpCommTask<T>*>(p->data);
  self->_request->body().append(at, len);
  return HPE_OK;
}

template <SocketType T>
int HttpCommTask<T>::on_message_complete(llhttp_t* p) {
  HttpCommTask<T>* self = static_cast<HttpCommTask<T>*>(p->data);

  RequestStatistics* stat = self->statistics(1UL);
  RequestStatistics::SET_READ_END(stat);
  self->_messageDone = true;

  return HPE_PAUSED;
}

template <SocketType T>
HttpCommTask<T>::HttpCommTask(GeneralServer& server, ConnectionInfo info,
                              std::unique_ptr<AsioSocket<T>> so)
    : GeneralCommTask<T>(server, "HttpCommTask", std::move(info), std::move(so)),
      _lastHeaderWasValue(false),
      _shouldKeepAlive(false),
      _messageDone(false),
      _allowMethodOverride(GeneralServerFeature::allowMethodOverride()) {
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
HttpCommTask<T>::~HttpCommTask() = default;

template <SocketType T>
void HttpCommTask<T>::start() {
  this->_protocol->setNonBlocking(true);
  asio_ns::post(this->_protocol->context.io_context, [self = this->shared_from_this()] {
    auto* thisPtr = static_cast<HttpCommTask<T>*>(self.get());
    thisPtr->checkVSTPrefix();
  });
}

/// @brief send error response including response body
template <SocketType T>
void HttpCommTask<T>::addSimpleResponse(rest::ResponseCode code,
                                        rest::ContentType respType, uint64_t /*messageId*/,
                                        velocypack::Buffer<uint8_t>&& buffer) {
  try {
    auto resp = std::make_unique<HttpResponse>(code, /*buffer*/ nullptr);
    resp->setContentType(respType);
    if (!buffer.empty()) {
      resp->setPayload(std::move(buffer), true, VPackOptions::Defaults);
    }
    sendResponse(std::move(resp), this->stealStatistics(1UL));
  } catch (...) {
    LOG_TOPIC("fc831", WARN, Logger::REQUESTS)
        << "addSimpleResponse received an exception, closing connection";
    this->close();
  }
}

template <SocketType T>
bool HttpCommTask<T>::readCallback(asio_ns::error_code ec) {
  llhttp_errno_t err = HPE_OK;
  if (ec) {  // got a connection error
    if (ec == asio_ns::error::misc_errors::eof) {
      err = llhttp_finish(&_parser);
    } else {
      LOG_TOPIC("395fe", DEBUG, Logger::REQUESTS)
          << "Error while reading from socket: '" << ec.message() << "'";
      err = HPE_INVALID_EOF_STATE;
    }
  } else {  // Inspect the received data

    size_t parsedBytes = 0;
    for (auto const& buffer : this->_protocol->buffer.data()) {
      const char* data = reinterpret_cast<const char*>(buffer.data());

      err = llhttp_execute(&_parser, data, buffer.size());
      if (err != HPE_OK) {
        parsedBytes += llhttp_get_error_pos(&_parser) - data;
        break;
      }
      parsedBytes += buffer.size();
    }

    TRI_ASSERT(parsedBytes < std::numeric_limits<size_t>::max());
    // Remove consumed data from receive buffer.
    this->_protocol->buffer.consume(parsedBytes);
    // And count it in the statistics:
    RequestStatistics* stat = this->statistics(1UL);
    RequestStatistics::ADD_RECEIVED_BYTES(stat, parsedBytes);

    if (err == HPE_PAUSED_UPGRADE) {
      this->addSimpleResponse(rest::ResponseCode::NOT_IMPLEMENTED,
                              rest::ContentType::UNSET, 1, VPackBuffer<uint8_t>());
      return false;  // stop read loop
    }
  }

  if (_messageDone) {
    TRI_ASSERT(err == HPE_PAUSED);
    _messageDone = false;
    processRequest();
    return false;  // stop read loop
  }

  if (err != HPE_OK && err != HPE_USER) {
    if (err == HPE_INVALID_EOF_STATE) {
      LOG_TOPIC("595fd", TRACE, Logger::REQUESTS)
          << "Connection closed by peer, with ptr " << this;
    } else {
      LOG_TOPIC("595fe", TRACE, Logger::REQUESTS)
          << "HTTP parse failure: '" << llhttp_get_error_reason(&_parser) << "'";
    }
    this->close();
  }

  return err == HPE_OK && !ec;
}

namespace {
static constexpr const char* vst10 = "VST/1.0\r\n\r\n";
static constexpr const char* vst11 = "VST/1.1\r\n\r\n";
}  // namespace

template <SocketType T>
void HttpCommTask<T>::checkVSTPrefix() {
  auto cb = [self = this->shared_from_this()](asio_ns::error_code const& ec, size_t nread) {
    auto* thisPtr = static_cast<HttpCommTask<T>*>(self.get());
    if (ec || nread < 11) {
      thisPtr->close();
      return;
    }
    thisPtr->_protocol->buffer.commit(nread);

    auto bg = asio_ns::buffers_begin(thisPtr->_protocol->buffer.data());
    if (std::equal(::vst10, ::vst10 + 11, bg, bg + 11)) {
      thisPtr->_protocol->buffer.consume(11);  // remove VST/1.0 prefix
      auto commTask =
          std::make_unique<VstCommTask<T>>(thisPtr->_server, thisPtr->_connectionInfo,
                                           std::move(thisPtr->_protocol),
                                           fuerte::vst::VST1_0);
      thisPtr->_server.registerTask(std::move(commTask));
      return;  // vst 1.0

    } else if (std::equal(::vst11, ::vst11 + 11, bg, bg + 11)) {
      thisPtr->_protocol->buffer.consume(11);  // remove VST/1.1 prefix
      auto commTask =
          std::make_unique<VstCommTask<T>>(thisPtr->_server, thisPtr->_connectionInfo,
                                           std::move(thisPtr->_protocol),
                                           fuerte::vst::VST1_1);
      thisPtr->_server.registerTask(std::move(commTask));
      return;  // vst 1.1
    }

    thisPtr->asyncReadSome();  // continue reading
  };
  auto buffs = this->_protocol->buffer.prepare(GeneralCommTask<T>::ReadBlockSize);
  asio_ns::async_read(this->_protocol->socket, buffs,
                      asio_ns::transfer_at_least(11), std::move(cb));
}

template <SocketType T>
bool HttpCommTask<T>::checkHttpUpgrade() {
  // TODO
  return true;
}

template <SocketType T>
void HttpCommTask<T>::processRequest() {
  TRI_ASSERT(_request);

  // ensure there is a null byte termination. RestHandlers use
  // C functions like strchr that except a C string as input
  _request->body().push_back('\0');
  _request->body().resetTo(_request->body().size() - 1);
  this->_protocol->timer.cancel();

  {
    LOG_TOPIC("6e770", INFO, Logger::REQUESTS)
        << "\"http-request-begin\",\"" << (void*)this << "\",\""
        << this->_connectionInfo.clientAddress << "\",\""
        << HttpRequest::translateMethod(_request->requestType()) << "\",\""
        << (_request->databaseName().empty() ? "" : "/_db/" + _request->databaseName())
        << (Logger::logRequestParameters() ? _request->fullUrl() : _request->requestPath())
        << "\"";

    VPackStringRef body = _request->rawPayload();
    if (!body.empty() && Logger::isEnabled(LogLevel::TRACE, Logger::REQUESTS) &&
        Logger::logRequestParameters()) {
      LOG_TOPIC("b9e76", TRACE, Logger::REQUESTS)
          << "\"http-request-body\",\"" << (void*)this << "\",\""
          << StringUtils::escapeUnicode(body.toString()) << "\"";
    }
  }

  // store origin heaer
  _origin = _request->header(StaticStrings::Origin);

  // OPTIONS requests currently go unauthenticated
  if (_request->requestType() == rest::RequestType::OPTIONS) {
    processCorsOptions();
    return;
  }

  // scrape the auth headers to determine and authenticate the user
  rest::ResponseCode authResult = handleAuthHeader(*_request);

  // authenticated
  if (authResult == rest::ResponseCode::SERVER_ERROR) {
    std::string realm = "Bearer token_type=\"JWT\", realm=\"ArangoDB\"";
    auto res = std::make_unique<HttpResponse>(rest::ResponseCode::UNAUTHORIZED, nullptr);
    res->setHeaderNC(StaticStrings::WwwAuthenticate, std::move(realm));
    sendResponse(std::move(res), nullptr);
    return;
  }

  // We want to separate superuser token traffic:
  if (_request->authenticated() && _request->user().empty()) {
    RequestStatistics::SET_SUPERUSER(this->statistics(1UL));
  }

  // first check whether we allow the request to continue
  CommTask::Flow cont = this->prepareExecution(*_request);
  if (cont != CommTask::Flow::Continue) {
    return;  // prepareExecution sends the error message
  }

  // unzip / deflate
  if (!handleContentEncoding(*_request)) {
    this->addErrorResponse(rest::ResponseCode::BAD, _request->contentTypeResponse(),
                           1, TRI_ERROR_BAD_PARAMETER, "decoding error");
    return;
  }

  // create a handler and execute
  auto resp = std::make_unique<HttpResponse>(rest::ResponseCode::SERVER_ERROR, nullptr);
  resp->setContentType(_request->contentTypeResponse());
  resp->setContentTypeRequested(_request->contentTypeResponse());

  this->executeRequest(std::move(_request), std::move(resp));
}

/// deny credentialed requests or not (only CORS)
namespace {
bool allowCredentials(std::string const& origin) {
  // default is to allow nothing
  bool allowCredentials = false;
  if (origin.empty()) {
    return allowCredentials;
  }  // else handle origin headers

  // if the request asks to allow credentials, we'll check against the
  // configured whitelist of origins
  std::vector<std::string> const& accessControlAllowOrigins =
      GeneralServerFeature::accessControlAllowOrigins();

  if (!accessControlAllowOrigins.empty()) {
    if (accessControlAllowOrigins[0] == "*") {
      // special case: allow everything
      allowCredentials = true;
    } else if (!origin.empty()) {
      // copy origin string
      if (origin[origin.size() - 1] == '/') {
        // strip trailing slash
        auto result = std::find(accessControlAllowOrigins.begin(),
                                accessControlAllowOrigins.end(),
                                origin.substr(0, origin.size() - 1));
        allowCredentials = (result != accessControlAllowOrigins.end());
      } else {
        auto result = std::find(accessControlAllowOrigins.begin(),
                                accessControlAllowOrigins.end(), origin);
        allowCredentials = (result != accessControlAllowOrigins.end());
      }
    } else {
      TRI_ASSERT(!allowCredentials);
    }
  }
  return allowCredentials;
}
}  // namespace

/// handle an OPTIONS request
template <SocketType T>
void HttpCommTask<T>::processCorsOptions() {
  auto resp = std::make_unique<HttpResponse>(rest::ResponseCode::OK, nullptr);

  resp->setHeaderNCIfNotSet(StaticStrings::Allow, StaticStrings::CorsMethods);

  if (!_origin.empty()) {
    LOG_TOPIC("e1cfa", DEBUG, arangodb::Logger::REQUESTS)
        << "got CORS preflight request";
    std::string const allowHeaders =
        StringUtils::trim(_request->header(StaticStrings::AccessControlRequestHeaders));

    // send back which HTTP methods are allowed for the resource
    // we'll allow all
    resp->setHeaderNCIfNotSet(StaticStrings::AccessControlAllowMethods,
                              StaticStrings::CorsMethods);

    if (!allowHeaders.empty()) {
      // allow all extra headers the client requested
      // we don't verify them here. the worst that can happen is that the
      // client sends some broken headers and then later cannot access the data
      // on the server. that's a client problem.
      resp->setHeaderNCIfNotSet(StaticStrings::AccessControlAllowHeaders, allowHeaders);

      LOG_TOPIC("55413", TRACE, arangodb::Logger::REQUESTS)
          << "client requested validation of the following headers: " << allowHeaders;
    }

    // set caching time (hard-coded value)
    resp->setHeaderNCIfNotSet(StaticStrings::AccessControlMaxAge, StaticStrings::N1800);
  }

  _request.reset();  // forge the request
  sendResponse(std::move(resp), nullptr);
}

template <SocketType T>
ResponseCode HttpCommTask<T>::handleAuthHeader(HttpRequest& req) {
  bool found;
  std::string const& authStr = req.header(StaticStrings::Authorization, found);
  if (!found) {
    if (this->_auth->isActive()) {
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
      LOG_TOPIC("c4536", DEBUG, arangodb::Logger::REQUESTS)
          << "\"authorization-header\",\"" << (void*)this << "\",\"" << authStr << "\"";
    }

    try {
      AuthenticationMethod authMethod = AuthenticationMethod::NONE;
      if (authStr.size() >= 6) {
        if (strncasecmp(authStr.c_str(), "basic ", 6) == 0) {
          authMethod = AuthenticationMethod::BASIC;
        } else if (strncasecmp(authStr.c_str(), "bearer ", 7) == 0) {
          authMethod = AuthenticationMethod::JWT;
        }
      }

      req.setAuthenticationMethod(authMethod);
      if (authMethod != AuthenticationMethod::NONE) {
        this->_authToken = this->_auth->tokenCache().checkAuthentication(authMethod, auth);
        req.setAuthenticated(this->_authToken.authenticated());
        req.setUser(this->_authToken._username);  // do copy here, so that we do not invalidate the member
      }

      if (req.authenticated() || !this->_auth->isActive()) {
        events::Authenticated(req, authMethod);
        return rest::ResponseCode::OK;
      } else if (this->_auth->isActive()) {
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

/// decompress content
template <SocketType T>
bool HttpCommTask<T>::handleContentEncoding(HttpRequest& req) {
  // TODO consider doing the decoding on the fly
  auto encode = [&](std::string const& encoding) {
    uint8_t* src = req.body().data();
    size_t len = req.body().length();
    if (encoding == "gzip") {
      VPackBuffer<uint8_t> dst;
      if (!arangodb::encoding::gzipUncompress(src, len, dst)) {
        return false;
      }
      req.body() = std::move(dst);
      return true;
    } else if (encoding == "deflate") {
      VPackBuffer<uint8_t> dst;
      if (!arangodb::encoding::gzipDeflate(src, len, dst)) {
        return false;
      }
      req.body() = std::move(dst);
      return true;
    }
    return false;
  };

  bool found;
  std::string const& val1 = req.header(StaticStrings::TransferEncoding, found);
  if (found) {
    return encode(val1);
  }

  std::string const& val2 = req.header(StaticStrings::ContentEncoding, found);
  if (found) {
    return encode(val2);
  }
  return true;
}

template <SocketType T>
void HttpCommTask<T>::sendResponse(std::unique_ptr<GeneralResponse> baseRes,
                                   RequestStatistics* stat) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  HttpResponse& response = dynamic_cast<HttpResponse&>(*baseRes);
#else
  HttpResponse& response = static_cast<HttpResponse&>(*baseRes);
#endif

  this->finishExecution(*baseRes);

  // CORS response handling
  if (!_origin.empty()) {
    // the request contained an Origin header. We have to send back the
    // access-control-allow-origin header now
    LOG_TOPIC("ae603", DEBUG, arangodb::Logger::REQUESTS)
        << "handling CORS response";

    // send back original value of "Origin" header
    response.setHeaderNCIfNotSet(StaticStrings::AccessControlAllowOrigin, _origin);

    // send back "Access-Control-Allow-Credentials" header
    response.setHeaderNCIfNotSet(StaticStrings::AccessControlAllowCredentials,
                                 (::allowCredentials(_origin) ? "true" : "false"));

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

  _header.clear();
  _header.reserve(220);

  _header.append(TRI_CHAR_LENGTH_PAIR("HTTP/1.1 "));
  _header.append(GeneralResponse::responseString(response.responseCode()));
  _header.append("\r\n", 2);

  bool seenServerHeader = false;
  // bool seenConnectionHeader = false;
  for (auto const& it : response.headers()) {
    std::string const& key = it.first;
    size_t const keyLength = key.size();
    // ignore content-length
    if (key == StaticStrings::ContentLength || key == StaticStrings::Connection ||
        key == StaticStrings::TransferEncoding) {
      continue;
    }

    if (key == StaticStrings::Server) {
      seenServerHeader = true;
    }

    // reserve enough space for header name + ": " + value + "\r\n"
    _header.reserve(key.size() + 2 + it.second.size() + 2);

    char const* p = key.data();
    char const* end = p + keyLength;
    int capState = 1;
    while (p < end) {
      if (capState == 1) {
        // upper case
        _header.push_back(StringUtils::toupper(*p));
        capState = 0;
      } else if (capState == 0) {
        // normal case
        _header.push_back(StringUtils::tolower(*p));
        if (*p == '-') {
          capState = 1;
        } else if (*p == ':') {
          capState = 2;
        }
      } else {
        // output as is
        _header.push_back(*p);
      }
      ++p;
    }

    _header.append(": ", 2);
    _header.append(it.second);
    _header.append("\r\n", 2);
  }

  // add "Server" response header
  if (!seenServerHeader && !HttpResponse::HIDE_PRODUCT_HEADER) {
    _header.append(TRI_CHAR_LENGTH_PAIR("Server: ArangoDB\r\n"));
  }

  // turn on the keepAlive timer
  double secs = GeneralServerFeature::keepAliveTimeout();
  if (_shouldKeepAlive && secs > 0) {
    int64_t millis = static_cast<int64_t>(secs * 1000);
    this->_protocol->timer.expires_after(std::chrono::milliseconds(millis));
    this->_protocol->timer.async_wait([self = CommTask::weak_from_this()](asio_ns::error_code ec) {
      std::shared_ptr<CommTask> s;
      if (ec || !(s = self.lock())) {  // was canceled / deallocated
        return;
      }
      LOG_TOPIC("5c1e0", INFO, Logger::REQUESTS)
          << "keep alive timeout, closing stream!";
      s->close();
    });

    _header.append(TRI_CHAR_LENGTH_PAIR("Connection: Keep-Alive\r\n"));
  } else {
    _header.append(TRI_CHAR_LENGTH_PAIR("Connection: Close\r\n"));
  }

  // add "Content-Type" header
  switch (response.contentType()) {
    case ContentType::UNSET:
    case ContentType::JSON:
      _header.append(TRI_CHAR_LENGTH_PAIR(
          "Content-Type: application/json; charset=utf-8\r\n"));
      break;
    case ContentType::VPACK:
      _header.append(
          TRI_CHAR_LENGTH_PAIR("Content-Type: application/x-velocypack\r\n"));
      break;
    case ContentType::TEXT:
      _header.append(
          TRI_CHAR_LENGTH_PAIR("Content-Type: text/plain; charset=utf-8\r\n"));
      break;
    case ContentType::HTML:
      _header.append(
          TRI_CHAR_LENGTH_PAIR("Content-Type: text/html; charset=utf-8\r\n"));
      break;
    case ContentType::DUMP:
      _header.append(TRI_CHAR_LENGTH_PAIR(
          "Content-Type: application/x-arango-dump; charset=utf-8\r\n"));
      break;
    case ContentType::CUSTOM:  // don't do anything
      break;
  }

  for (auto const& it : response.cookies()) {
    _header.append(TRI_CHAR_LENGTH_PAIR("Set-Cookie: "));
    _header.append(it);
    _header.append("\r\n", 2);
  }

  size_t len = response.bodySize();
  _header.append(TRI_CHAR_LENGTH_PAIR("Content-Length: "));
  _header.append(std::to_string(len));
  _header.append("\r\n\r\n", 4);

  TRI_ASSERT(_response == nullptr);
  _response = response.stealBody();
  // append write buffer and statistics
  double const totalTime = RequestStatistics::ELAPSED_SINCE_READ_START(stat);

  // and give some request information
  LOG_TOPIC("8f555", DEBUG, Logger::REQUESTS)
      << "\"http-request-end\",\"" << (void*)this << "\",\""
      << this->_connectionInfo.clientAddress << "\",\""
      << GeneralRequest::translateMethod(::llhttpToRequestType(&_parser))
      << "\",\"" << static_cast<int>(response.responseCode()) << "\","
      << Logger::FIXED(totalTime, 6);
  
  if constexpr (SocketType::Ssl == T) {
    this->_protocol->context.io_context.dispatch([self = this->shared_from_this(), stat]() mutable {
      auto* thisPtr = static_cast<HttpCommTask<T>*>(self.get());
      thisPtr->writeResponse(stat);
    });
  } else {
    writeResponse(stat);
  }
}

// called on IO context thread
template <SocketType T>
void HttpCommTask<T>::writeResponse(RequestStatistics* stat) {
  TRI_ASSERT(!_header.empty());
  
  RequestStatistics::SET_WRITE_START(stat);

  std::array<asio_ns::const_buffer, 2> buffers;
  buffers[0] = asio_ns::buffer(_header.data(), _header.size());
  if (HTTP_HEAD != _parser.method) {
    buffers[1] = asio_ns::buffer(_response->data(), _response->size());
  }

  // FIXME measure performance w/o sync write
  asio_ns::async_write(this->_protocol->socket, buffers,
                       [self = this->shared_from_this(), stat](asio_ns::error_code ec, size_t nwrite) {
                         auto* thisPtr = static_cast<HttpCommTask<T>*>(self.get());
                         RequestStatistics::SET_WRITE_END(stat);
                         RequestStatistics::ADD_SENT_BYTES(stat, nwrite);
    
                         thisPtr->_response.reset();

                         llhttp_errno_t err = llhttp_get_errno(&thisPtr->_parser);
                         if (ec || !thisPtr->_shouldKeepAlive || err != HPE_PAUSED) {
                           if (ec) {
                             LOG_TOPIC("2b6b4", DEBUG, arangodb::Logger::REQUESTS)
                                 << "asio write error: '" << ec.message() << "'";
                           }
                           thisPtr->close();
                         } else {  // ec == HPE_PAUSED
                           llhttp_resume(&thisPtr->_parser);
                           thisPtr->asyncReadSome();
                         }
                         if (stat != nullptr) {
                           stat->release();
                         }
                       });
}

template <SocketType T>
std::unique_ptr<GeneralResponse> HttpCommTask<T>::createResponse(rest::ResponseCode responseCode,
                                                                 uint64_t /* messageId */) {
  return std::make_unique<HttpResponse>(responseCode, nullptr);
}

template class arangodb::rest::HttpCommTask<SocketType::Tcp>;
template class arangodb::rest::HttpCommTask<SocketType::Ssl>;
#ifndef _WIN32
template class arangodb::rest::HttpCommTask<SocketType::Unix>;
#endif
