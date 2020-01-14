////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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

#include "H2CommTask.h"

#include "Basics/Exceptions.h"
#include "Basics/ScopeGuard.h"
#include "Basics/asio_ns.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "Logger/LogMacros.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "Statistics/ConnectionStatistics.h"
#include "Statistics/RequestStatistics.h"

#include <velocypack/velocypack-aliases.h>
#include <cstring>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

using arangodb::velocypack::StringRef;

template <SocketType T>
/*static*/ int H2CommTask<T>::on_begin_headers(nghttp2_session* session,
                                               const nghttp2_frame* frame, void* user_data) {
  H2CommTask<T>* me = static_cast<H2CommTask<T>*>(user_data);

  if (frame->hd.type != NGHTTP2_HEADERS || frame->headers.cat != NGHTTP2_HCAT_REQUEST) {
    return 0;
  }

  int32_t sid = frame->hd.stream_id;
  auto req = std::make_unique<HttpRequest>(me->_connectionInfo, /*messageId*/ sid,
                                           /*allowMethodOverride*/ false);
  me->createStream(sid, std::move(req));

  return 0;
}

template <SocketType T>
/*static*/ int H2CommTask<T>::on_header(nghttp2_session* session, const nghttp2_frame* frame,
                                        const uint8_t* name, size_t namelen,
                                        const uint8_t* value, size_t valuelen,
                                        uint8_t flags, void* user_data) {
  H2CommTask<T>* me = static_cast<H2CommTask<T>*>(user_data);
  int32_t stream_id = frame->hd.stream_id;

  if (frame->hd.type != NGHTTP2_HEADERS || frame->headers.cat != NGHTTP2_HCAT_REQUEST) {
    return 0;
  }

  Stream* strm = me->findStream(stream_id);
  if (!strm) {
    return 0;
  }

  // handle pseudo headers https://http2.github.io/http2-spec/#rfc.section.8.1.2.3
  StringRef field(reinterpret_cast<const char*>(name), namelen);
  StringRef val(reinterpret_cast<const char*>(value), valuelen);

  if (StringRef(":method") == field) {
    strm->request->setRequestType(GeneralRequest::translateMethod(val));
  } else if (StringRef(":scheme") == field) {
    //    strm->request->
  } else if (StringRef(":path") == field) {
    strm->request->parseUrl(reinterpret_cast<const char*>(value), valuelen);
  } else if (StringRef(":authority") == field) {
  } else {  // fall through
    strm->request->setHeaderV2(field.toString(),
                               std::string(reinterpret_cast<const char*>(value), valuelen));

    // TODO limit max header size ??
  }

  return 0;
}

template <SocketType T>
int H2CommTask<T>::on_frame_recv(nghttp2_session* session,
                                 const nghttp2_frame* frame, void* user_data) {
  H2CommTask<T>* me = static_cast<H2CommTask<T>*>(user_data);

  switch (frame->hd.type) {
    case NGHTTP2_DATA:
    case NGHTTP2_HEADERS: {
      if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
        int32_t stream_id = frame->hd.stream_id;
        Stream* strm = me->findStream(stream_id);

        if (strm) {
          me->processStream(strm);
        }
      }
      break;
    }
  }

  return 0;
}

template <SocketType T>
/*static*/ int H2CommTask<T>::on_data_chunk_recv(nghttp2_session* session, uint8_t flags,
                                                 int32_t stream_id, const uint8_t* data,
                                                 size_t len, void* user_data) {
  H2CommTask<T>* me = static_cast<H2CommTask<T>*>(user_data);
  Stream* strm = me->findStream(stream_id);
  if (strm) {
    strm->request->body().append(data, len);
  }

  return 0;
}

template <SocketType T>
/*static*/ int H2CommTask<T>::on_stream_close(nghttp2_session* session, int32_t stream_id,
                                              uint32_t error_code, void* user_data) {
  H2CommTask<T>* me = static_cast<H2CommTask<T>*>(user_data);
  me->_streams.erase(stream_id);
  return 0;
}

template <SocketType T>
/*static*/ int H2CommTask<T>::on_frame_send(nghttp2_session* session,
                                            const nghttp2_frame* frame, void* user_data) {
  // can be used for push promises
  return 0;
}

template <SocketType T>
/*static*/ int H2CommTask<T>::on_frame_not_send(nghttp2_session* session,
                                                const nghttp2_frame* frame,
                                                int lib_error_code, void* user_data) {
  if (frame->hd.type != NGHTTP2_HEADERS) {
    return 0;
  }

  // Issue RST_STREAM so that stream does not hang around.
  nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE, frame->hd.stream_id,
                            NGHTTP2_INTERNAL_ERROR);

  return 0;
}

template <SocketType T>
H2CommTask<T>::H2CommTask(GeneralServer& server, ConnectionInfo info,
                          std::unique_ptr<AsioSocket<T>> so)
    : GeneralCommTask<T>(server, std::move(info), std::move(so)) {
  ConnectionStatistics::SET_HTTP(this->_connectionStatistics);
  initNgHttp2Session();
}

template <SocketType T>
H2CommTask<T>::~H2CommTask() noexcept {
  nghttp2_session_del(_session);
  _session = nullptr;
  if (!_streams.empty()) {
    LOG_TOPIC("924cf", WARN, Logger::REQUESTS) << "got "
      << _streams.size() << " remaining H2 streams";
  }
}

/// init h2 session
template <SocketType T>
void H2CommTask<T>::initNgHttp2Session() {
  nghttp2_session_callbacks* callbacks;
  int rv = nghttp2_session_callbacks_new(&callbacks);
  if (rv != 0) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    return;
  }

  auto cbScope = scopeGuard([&] { nghttp2_session_callbacks_del(callbacks); });

  nghttp2_session_callbacks_set_on_begin_headers_callback(callbacks, H2CommTask<T>::on_begin_headers);
  nghttp2_session_callbacks_set_on_header_callback(callbacks, H2CommTask<T>::on_header);
  nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, H2CommTask<T>::on_frame_recv);
  nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, H2CommTask<T>::on_data_chunk_recv);
  nghttp2_session_callbacks_set_on_stream_close_callback(callbacks, H2CommTask<T>::on_stream_close);
  nghttp2_session_callbacks_set_on_frame_send_callback(callbacks, H2CommTask<T>::on_frame_send);
  nghttp2_session_callbacks_set_on_frame_not_send_callback(callbacks, H2CommTask<T>::on_frame_not_send);

  rv = nghttp2_session_server_new(&_session, callbacks, /*args*/ this);
  if (rv != 0) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    return;
  }
}

template <SocketType T>
void H2CommTask<T>::upgrade(std::unique_ptr<HttpRequest> req) {
  bool found;
  std::string const& settings = req->header("http2-settings", found);
  bool const wasHead = req->requestType() == RequestType::HEAD;

  const uint8_t* src = reinterpret_cast<const uint8_t*>(settings.data());
  int rv = nghttp2_session_upgrade2(_session, src, settings.size(), wasHead, nullptr);

  if (rv != 0) {
    if (rv == NGHTTP2_ERR_INVALID_ARGUMENT) {
      // The settings_payload is badly formed.
      LOG_TOPIC("0103c", ERR, Logger::REQUESTS) << "invalid upgrade header";
    }
    this->close();
    return;
  }

  // https://http2.github.io/http2-spec/#discover-http
  auto str = std::make_shared<std::string>();
  str->append(
      "HTTP/1.1 101 Switching Protocols\r\nConnection: "
      "Upgrade\r\nUpgrade: h2c\r\n\r\n");

  asio_ns::async_write(this->_protocol->socket, asio_ns::buffer(str->data(), str->size()), [str, self(this->shared_from_this()), req(std::move(req))](const asio_ns::error_code& ec, std::size_t) mutable {
    auto& me = static_cast<H2CommTask<T>&>(*self);
    if (ec) {
      me.close(ec);
      return;
    }

    nghttp2_settings_entry ent{NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 100};
    nghttp2_submit_settings(me._session, NGHTTP2_FLAG_NONE, &ent, 1);

    // The HTTP/1.1 request that is sent prior to upgrade is assigned
    // a stream identifier of 1 (see Section 5.1.1).
    // Stream 1 is implicitly "half-closed" from the client toward the server

    TRI_ASSERT(req->messageId() == 1);
    auto* strm = me.createStream(1, std::move(req));

    // will start writing later
    me.processStream(strm);

    // start reading
    me.asyncReadSome();
  });
}

template <SocketType T>
void H2CommTask<T>::start() {
  LOG_TOPIC("db5ab", DEBUG, Logger::REQUESTS)
    << "opened H2 connection \"" << (void*)this << "\"";
  
  asio_ns::post(this->_protocol->context.io_context, [self = this->shared_from_this()] {
    auto& me = static_cast<H2CommTask<T>&>(*self);

    // queueing the server connection preface,
    // which always consists of a SETTINGS frame.
    nghttp2_settings_entry ent{NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 100};
    nghttp2_submit_settings(me._session, NGHTTP2_FLAG_NONE, &ent, 1);

    me.doWrite();  // write out preface
    me.asyncReadSome();
  });
}

template <SocketType T>
bool H2CommTask<T>::readCallback(asio_ns::error_code ec) {
  if (ec) {
    this->close(ec);
    return false;  // stop read loop
  }

  size_t parsedBytes = 0;
  for (auto const& buffer : this->_protocol->buffer.data()) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buffer.data());

    int rv = nghttp2_session_mem_recv(_session, data, buffer.size());
    if (rv < 0) {
      this->close(ec);
      return false;  // stop read loop
    }

    parsedBytes += buffer.size();
  }

  TRI_ASSERT(parsedBytes < std::numeric_limits<size_t>::max());
  // Remove consumed data from receive buffer.
  this->_protocol->buffer.consume(parsedBytes);

  // perhaps start write loop
  doWrite();

  if (!_writing && shouldStop()) {
    this->close();
    return false;  // stop read loop
  }

  return true;  //  continue read lopp
}

template <SocketType T>
void H2CommTask<T>::processStream(H2CommTask<T>::Stream* stream) {
  TRI_ASSERT(stream);

  std::unique_ptr<HttpRequest> req = std::move(stream->request);
  // ensure there is a null byte termination. RestHandlers use
  // C functions like strchr that except a C string as input
  req->body().push_back('\0');
  req->body().resetTo(req->body().size() - 1);

  this->_protocol->timer.cancel();

  {
    LOG_TOPIC("924ce", INFO, Logger::REQUESTS)
        << "\"h2-request-begin\",\"" << (void*)this << "\",\""
        << this->_connectionInfo.clientAddress << "\",\""
        << HttpRequest::translateMethod(req->requestType()) << "\",\""
        << (req->databaseName().empty() ? "" : "/_db/" + req->databaseName())
        << (Logger::logRequestParameters() ? req->fullUrl() : req->requestPath())
        << "\"";

    VPackStringRef body = req->rawPayload();
    if (!body.empty() && Logger::isEnabled(LogLevel::TRACE, Logger::REQUESTS) &&
        Logger::logRequestParameters()) {
      LOG_TOPIC("b6dc3", TRACE, Logger::REQUESTS)
          << "\"h2-request-body\",\"" << (void*)this << "\",\""
          << StringUtils::escapeUnicode(body.toString()) << "\"";
    }
  }

  // OPTIONS requests currently go unauthenticated
  if (req->requestType() == rest::RequestType::OPTIONS) {
    this->processCorsOptions(std::move(req));
    return;
  }

  // store origin header for later use
  stream->origin = req->header(StaticStrings::Origin);

  // scrape the auth headers to determine and authenticate the user
  auto authToken = this->checkAuthHeader(*req);

  // We want to separate superuser token traffic:
  if (req->authenticated() && req->user().empty()) {
    RequestStatistics::SET_SUPERUSER(this->statistics(1UL));
  }

  // first check whether we allow the request to continue
  CommTask::Flow cont = this->prepareExecution(authToken, *req);
  if (cont != CommTask::Flow::Continue) {
    return;  // prepareExecution sends the error message
  }

  // unzip / deflate
  if (!this->handleContentEncoding(*req)) {
    this->addErrorResponse(rest::ResponseCode::BAD, req->contentTypeResponse(),
                           1, TRI_ERROR_BAD_PARAMETER, "decoding error");
    return;
  }

  // create a handler and execute
  auto resp = std::make_unique<HttpResponse>(rest::ResponseCode::SERVER_ERROR,
                                             req->messageId(), nullptr);
  resp->setContentType(req->contentTypeResponse());
  resp->setContentTypeRequested(req->contentTypeResponse());

  this->executeRequest(std::move(req), std::move(resp));
}

namespace {
bool expectResponseBody(int status_code) {
  return status_code == 101 ||
         (status_code / 100 != 1 && status_code != 304 && status_code != 204);
}
}  // namespace

template <SocketType T>
void H2CommTask<T>::sendResponse(std::unique_ptr<GeneralResponse> baseRes,
                                 RequestStatistics* stat) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  HttpResponse& response = dynamic_cast<HttpResponse&>(*baseRes);
#else
  HttpResponse& response = static_cast<HttpResponse&>(*baseRes);
#endif

  const uint32_t streamId = response.messageId();
  Stream* strm = findStream(streamId);
  if (strm == nullptr) {
    LOG_TOPIC("9912b", ERR, Logger::REQUESTS)
        << "response with message id " << response.messageId() << "has not H2 stream";
    TRI_ASSERT(false);
    return;
  }

  this->finishExecution(*baseRes, strm->origin);

  if (!ServerState::instance()->isDBServer()) {
    // DB server is not user-facing, and does not need to set this header
    // use "IfNotSet" to not overwrite an existing response header
    response.setHeaderNCIfNotSet(StaticStrings::XContentTypeOptions, StaticStrings::NoSniff);
  }

  // append write buffer and statistics
  double const totalTime = RequestStatistics::ELAPSED_SINCE_READ_START(stat);

  // and give some request information
  LOG_TOPIC("924cc", DEBUG, Logger::REQUESTS)
      << "\"h2-request-end\",\"" << (void*)this << "\",\""
      << this->_connectionInfo.clientAddress
      << "\",\""
      //      << GeneralRequest::translateMethod(::llhttpToRequestType(&_parser))
      << "\",\"" << static_cast<int>(response.responseCode()) << "\","
      << Logger::FIXED(totalTime, 6);

  strm->response.reset(static_cast<HttpResponse*>(baseRes.release()));

  int32_t mid = static_cast<int32_t>(strm->response->messageId());
  while (!_waitingResponses.push(mid)) {
    std::this_thread::yield();
  }

  signalWrite();  // starts writing if necessary
}

// queue the response onto the session, call only on IO thread
template <SocketType T>
void H2CommTask<T>::queueHttp2Responses() {
  int32_t streamId;
  while (_waitingResponses.pop(streamId)) {
    Stream* strm = findStream(streamId);
    if (strm == nullptr) {
      LOG_TOPIC("e2773", ERR, Logger::REQUESTS)
          << "response with message id " << streamId << "has not H2 stream";
      TRI_ASSERT(false);
      return;
    }

    auto& res = *strm->response;

    // we need a continous block of memory for headers
    std::vector<nghttp2_nv> nva;
    nva.reserve(4 + res.headers().size());

    std::string status = std::to_string(static_cast<int>(res.responseCode()));
    nva.push_back({(uint8_t*)":status", (uint8_t*)status.data(), 7,
                   status.size(), NGHTTP2_NV_FLAG_NO_COPY_NAME});

    bool seenServerHeader = false;
    // bool seenConnectionHeader = false;
    for (auto const& it : res.headers()) {
      std::string const& key = it.first;
      std::string const& val = it.second;

      // ignore content-length
      if (key == StaticStrings::ContentLength || key == StaticStrings::Connection ||
          key == StaticStrings::TransferEncoding) {
        continue;
      }

      if (key == StaticStrings::Server) {
        seenServerHeader = true;
      }

      nva.push_back({(uint8_t*)key.data(), (uint8_t*)val.data(), key.size(), val.size(),
                     NGHTTP2_NV_FLAG_NO_COPY_NAME | NGHTTP2_NV_FLAG_NO_COPY_VALUE});
    }
    
    // add "Server" response header
    if (!seenServerHeader && !HttpResponse::HIDE_PRODUCT_HEADER) {
      nva.push_back({(uint8_t*)"server", (uint8_t*)"ArangoDB", 6, 8,
                     NGHTTP2_NV_FLAG_NO_COPY_NAME | NGHTTP2_NV_FLAG_NO_COPY_VALUE});
    }

    for (std::string const& cookie : res.cookies()) {
      nva.push_back({(uint8_t*)"set-cookie", (uint8_t*)cookie.c_str(), 10,
                     cookie.length(),
                     NGHTTP2_NV_FLAG_NO_COPY_NAME | NGHTTP2_NV_FLAG_NO_COPY_VALUE});
    }

    std::string type;
    if (res.contentType() != ContentType::CUSTOM) {
      type = rest::contentTypeToString(res.contentType());
      nva.push_back({(uint8_t*)"content-type", (uint8_t*)type.c_str(), 12,
                     type.length(), NGHTTP2_NV_FLAG_NO_COPY_NAME});
    }

    nghttp2_data_provider *prd_ptr = nullptr, prd;
    
    std::string len;
    if (!res.isHeadResponse() &&
        ::expectResponseBody(static_cast<int>(res.responseCode())) &&
        !res.isResponseEmpty()) {
      
      len = std::to_string(res.bodySize());
      nva.push_back({(uint8_t*)"content-length", (uint8_t*)len.c_str(), 14,
                     len.length(), NGHTTP2_NV_FLAG_NO_COPY_NAME});

      prd.source.ptr = strm;
      prd.read_callback = [](nghttp2_session* session, int32_t stream_id,
                             uint8_t* buf, size_t length, uint32_t* data_flags,
                             nghttp2_data_source* source, void* user_data) -> ssize_t {
        auto strm = static_cast<H2CommTask<T>::Stream*>(source->ptr);

        basics::StringBuffer& body = strm->response->body();

        // TODO do not copy the body if it is > 16kb
        TRI_ASSERT(body.size() > strm->responseOffset);
        const char* src = body.data() + strm->responseOffset;
        size_t len = std::min(length, body.size() - strm->responseOffset);
        TRI_ASSERT(len > 0);
        std::copy_n(src, len, buf);

        strm->responseOffset += len;
        if (strm->responseOffset == body.size()) {
          *data_flags |= NGHTTP2_DATA_FLAG_EOF;
        }

        return len;
      };
      prd_ptr = &prd;
    }

    int rv = nghttp2_submit_response(this->_session, streamId, nva.data(),
                                     nva.size(), prd_ptr);
    if (rv != 0) {
      this->close();
      return;
    }
  }
}

// called on IO context thread
template <SocketType T>
void H2CommTask<T>::doWrite() {
  if (_writing) {
    return;
  }
  _writing = true;

  queueHttp2Responses();

  std::array<asio_ns::const_buffer, 2> outBuffers;

  size_t len = 0;
  while (true) {
    const uint8_t* data;
    ssize_t nread = nghttp2_session_mem_send(_session, &data);
    if (nread < 0) {  // error
      this->close();
      return;
    }
    if (nread == 0) {  // done
      break;
    }

    // if the data is long we just pass it to async_write
    if (len + nread > _outbuffer.size()) {
      outBuffers[1] = asio_ns::buffer(data, nread);
      break;
    }

    std::copy_n(data, nread, std::begin(_outbuffer) + len);
    len += nread;
  }
  outBuffers[0] = asio_ns::buffer(_outbuffer, len);

  if (asio_ns::buffer_size(outBuffers) == 0) {
    if (shouldStop()) {
      this->close();
    }
    this->_writing = false;
    return;
  }

  // Reset read timer here, because normally client is sending
  // something, it does not expect timeout while doing it.
  this->setTimeout(this->DefaultTimeout);

  asio_ns::async_write(this->_protocol->socket, outBuffers,
                       [self = this->shared_from_this()](const asio_ns::error_code& ec,
                                                         std::size_t) {
                         auto& me = static_cast<H2CommTask<T>&>(*self);
                         if (ec) {
                           me.close(ec);
                           return;
                         }

                         me._writing = false;
                         me.doWrite();
                       });
}

template <SocketType T>
std::unique_ptr<GeneralResponse> H2CommTask<T>::createResponse(rest::ResponseCode responseCode,
                                                               uint64_t mid) {
  return std::make_unique<HttpResponse>(responseCode, mid);
}

template <SocketType T>
typename H2CommTask<T>::Stream* H2CommTask<T>::createStream(int32_t sid,
                                                            std::unique_ptr<HttpRequest> req) {
  TRI_ASSERT(static_cast<uint64_t>(sid) == req->messageId());
  auto stream = std::make_unique<Stream>(std::move(req));
  auto [it, inserted] = _streams.emplace(sid, std::move(stream));
  return it->second.get();
}

template <SocketType T>
typename H2CommTask<T>::Stream* H2CommTask<T>::findStream(int32_t sid) const {
  auto const& it = _streams.find(sid);
  if (it != _streams.end()) {
    return it->second.get();
  }
  return nullptr;
}

/// should close connection
template <SocketType T>
bool H2CommTask<T>::shouldStop() const {
  return !nghttp2_session_want_read(_session) && !nghttp2_session_want_write(_session);
}

// may be used to signal a write from sendResponse
template <SocketType T>
void H2CommTask<T>::signalWrite() {
  // avoid using asio_ns::post if possible
  if (!_signaledWrite.exchange(true)) {
    asio_ns::post(this->_protocol->context.io_context, [self = this->shared_from_this()] {
      auto& me = static_cast<H2CommTask<T>&>(*self);
      me._signaledWrite.store(false);
      me.doWrite();
    });
  }
}

template class arangodb::rest::H2CommTask<SocketType::Tcp>;
template class arangodb::rest::H2CommTask<SocketType::Ssl>;
#ifndef _WIN32
template class arangodb::rest::H2CommTask<SocketType::Unix>;
#endif
