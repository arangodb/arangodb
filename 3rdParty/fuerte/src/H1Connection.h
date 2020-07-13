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
#ifndef ARANGO_CXX_DRIVER_H1_CONNECTION_H
#define ARANGO_CXX_DRIVER_H1_CONNECTION_H 1

#include <fuerte/helper.h>
#include <fuerte/loop.h>
#include <fuerte/message.h>

#include <atomic>
#include <chrono>

#include "GeneralConnection.h"
#include "http.h"
#include "http_parser/http_parser.h"

namespace arangodb { namespace fuerte { inline namespace v1 { namespace http {

// in-flight request data
struct RequestItem {
  RequestItem(std::unique_ptr<Request>&& req,
              RequestCallback&& cb, std::string&& h);
  
  /// the request header
  std::string requestHeader;

  /// Callback for when request is done (in error or succeeded)
  RequestCallback callback;

  /// Reference to the request we're processing
  std::unique_ptr<arangodb::fuerte::v1::Request> request;
  
  /// point in time when the request expires
  std::chrono::steady_clock::time_point expires;

  inline void invokeOnError(Error e) {
    callback(e, std::move(request), nullptr);
  }
};

// Implements a client->server connection using node.js http-parser
template <SocketType ST>
class H1Connection final : public fuerte::GeneralConnection<ST, RequestItem> {
 public:
  explicit H1Connection(EventLoopService& loop,
                        detail::ConnectionConfiguration const&);
  ~H1Connection();

  /// The following public methods can be called from any thread.
 public:
  
  /// @brief Return the number of requests that have not yet finished.
  size_t requestsLeft() const override;

  /// All methods below here must only be called from the IO thread.
 protected:

  void finishConnect() override;
  
  /// perform writes
  void doWrite() override {
    this->asyncWriteNextRequest();
  }

  // called by the async_read handler (called from IO thread)
  void asyncReadCallback(asio_ns::error_code const&) override;

  /// abort ongoing / unfinished requests
  void abortOngoingRequests(const fuerte::Error) override;
  
  void setIOTimeout() override;
  
  std::unique_ptr<RequestItem> createRequest(std::unique_ptr<Request>&& req,
                                    RequestCallback&& cb) override {
    auto h = buildRequestHeader(*req);
    return std::make_unique<RequestItem>(std::move(req), std::move(cb), std::move(h));
  }
  
 private:

  // build request body for given request
  std::string buildRequestHeader(Request const& req);

  ///  Call on IO-Thread: writes out one queued request
  void asyncWriteNextRequest();

  // called by the async_write handler (called from IO thread)
  void asyncWriteCallback(asio_ns::error_code const&, size_t nwrite);

 private:
  static int on_message_begin(http_parser* p);
  static int on_status(http_parser* p, const char* at, size_t len);
  static int on_header_field(http_parser* p, const char* at, size_t len);
  static int on_header_value(http_parser* p, const char* at, size_t len);
  static int on_header_complete(http_parser* p);
  static int on_body(http_parser* p, const char* at, size_t len);
  static int on_message_complete(http_parser* p);

 private:
  /// cached authentication header
  std::string _authHeader;

  /// the node http-parser
  http_parser _parser;
  http_parser_settings _parserSettings;

  std::atomic<int> _leased;
    // This member is used to allow to lease a connection from the connection
    // pool without the idle alarm going off when we believe to have
    // leased the connection successfully. Here is the workflow:
    // Normally, the value is 0. If the idle alarm goes off, it first
    // compare-exchanges this value from 0 to -1, then shuts down the
    // connection (setting the _state to Failed in the TLS case). After
    // that, the value is set back to 0.
    // The lease operation tries to compare-exchange the value from 0 to 1,
    // and if this has worked, it checks again that the _state is not Failed.
    // This means, that if a lease has happened successfully, the idle alarm
    // does no longer shut down the connection.
    // If `sendRequest` is called on the connection (typically after a lease),
    // a compare-exchange operation from 1 to 2 is tried. The value 2 indicates
    // that the next time the idle alarm is set (after write/read activity
    // finishes), a compare-exchange to 0 happens, such that the idle alarm
    // can happen again.
    // All this has the net effect that from the moment of a successful lease
    // until the next call to `sendRequest`, we are sure that the idle alarm
    // does not go off.
    // Note that if one does not lease the connection, the idle alarm can
    // go off at any time and the next `sendRequest` might fail because of
    // this.
    // Furthermore, note that if one leases the connection and does not call
    // `sendRequest`, then the idle alarm does no longer go off and the
    // connection stays open indefinitely. This is misusage.
    // Finally, note that if the idle alarm goes off between a lease and
    // the following call to `sendRequest`, the connection might stay open
    // indefinitely, until activity on the connection has ceased again
    // and a new idle alarm is set. However, this will automatically happen
    // if that `sendRequest` and the corresponding request finishes.

  // parser state
  std::string _lastHeaderField;
  std::string _lastHeaderValue;

  /// response buffer, moved after writing
  velocypack::Buffer<uint8_t> _responseBuffer;

  /// currently in-flight request item
  std::unique_ptr<RequestItem> _item;
  /// response data, may be null before response header is received
  std::unique_ptr<arangodb::fuerte::v1::Response> _response;

  bool _lastHeaderWasValue = false;
  bool _shouldKeepAlive = false;
  bool _messageComplete = false;
};
}}}}  // namespace arangodb::fuerte::v1::http

#endif
