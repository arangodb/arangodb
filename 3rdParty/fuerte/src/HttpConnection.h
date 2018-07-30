////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
#pragma once
#ifndef ARANGO_CXX_DRIVER_HTTP_CONNECTION_H
#define ARANGO_CXX_DRIVER_HTTP_CONNECTION_H 1

#include <atomic>
#include <chrono>
#include <mutex>
#include <stdexcept>

#include <fuerte/FuerteLogger.h>
#include <fuerte/connection.h>
#include <fuerte/helper.h>
#include <fuerte/loop.h>
#include <fuerte/message.h>
#include <fuerte/types.h>

#include "AsioConnection.h"
#include "CallOnceRequestCallback.h"
#include "http.h"
#include "http_parser/http_parser.h"

namespace arangodb { namespace fuerte { inline namespace v1 { namespace http {

// HttpConnection implements a client->server connection using
//  the HTTP protocol.
template<SocketType ST>
class HttpConnection : public AsioConnection<fuerte::v1::http::RequestItem, ST> {
 public:
  explicit HttpConnection(std::shared_ptr<asio_io_context>& ctx,
                          detail::ConnectionConfiguration const&);

 public:
  // Start an asynchronous request.
  MessageID sendRequest(std::unique_ptr<Request>, RequestCallback) override;

  // Return the number of unfinished requests.
  /*std::size_t requestsLeft() const override {
    return _curlm->requestsLeft();
  }*/

 protected:
  // socket connection is up (with optional SSL), now initiate the VST protocol.
  void finishInitialization() override;

  // called on shutdown, always call superclass
  void shutdownConnection(const ErrorCondition) override;

  // fetch the buffers for the write-loop (called from IO thread)
  std::vector<asio_ns::const_buffer> prepareRequest(
      std::shared_ptr<RequestItem> const&) override;

  // called by the async_read handler (called from IO thread)
  void asyncReadCallback(asio_ns::error_code const&,
                         size_t transferred) override;

  /// Thread-Safe: activate the writer if needed
  void startWriting();
      
  /// Thread-Safe: disable write loop if nothing is queued
  uint32_t tryStopWriting();

  // called by the async_write handler (called from IO thread)
  void asyncWriteCallback(asio_ns::error_code const& error,
                          size_t transferred,
                          std::shared_ptr<RequestItem>) override;

 private:
  class Options {
   public:
    double connectionTimeout = 2.0;
  };

 private:
  // createRequestItem prepares a RequestItem for the given parameters.
  std::unique_ptr<RequestItem> createRequestItem(
      std::unique_ptr<Request> request, RequestCallback cb);
  
  /// set the timer accordingly
  void setTimeout(std::chrono::milliseconds);
  
 private:
  /// cached authentication header
  std::string _authHeader;
  /// currently in fligth request
  std::shared_ptr<RequestItem> _inFlight;
  /// the node http-parser
  http_parser _parser;
  http_parser_settings _parserSettings;

  // int _stillRunning;
};
}}}}  // namespace arangodb::fuerte::v1::http

#endif
