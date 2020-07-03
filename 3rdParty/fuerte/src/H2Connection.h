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
#pragma once
#ifndef ARANGO_CXX_DRIVER_H2_CONNECTION_H
#define ARANGO_CXX_DRIVER_H2_CONNECTION_H 1

#include <nghttp2/nghttp2.h>

#include <boost/lockfree/queue.hpp>

#include "GeneralConnection.h"
#include "http.h"

// naming in this file will be closer to asio for internal functions and types
// functions that are exposed to other classes follow ArangoDB conding
// conventions

namespace arangodb { namespace fuerte { inline namespace v1 { namespace http {

// Connection object that handles sending and receiving of
//  Velocystream Messages.
template <SocketType T>
class H2Connection final : public fuerte::GeneralConnection<T> {
 public:
  explicit H2Connection(EventLoopService& loop,
                        detail::ConnectionConfiguration const&);

  ~H2Connection();

 public:
  // this function prepares the request for sending
  // by creating a RequestItem and setting:
  //  - a messageid
  //  - the buffer to be send
  // this item is then moved to the request queue
  // and a write action is triggerd when there is
  // no other write in progress
  void sendRequest(std::unique_ptr<Request>, RequestCallback) override;

  // Return the number of unfinished requests.
  std::size_t requestsLeft() const override;

 protected:
  void finishConnect() override;

  /// The following is called when the connection is permanently failed. It is
  /// used to shut down any activity in a way that avoids sleeping barbers
  void terminateActivity() override;

  // Thread-Safe: activate the writer loop (if off and items are queud)
  void startWriting() override;

  // called by the async_read handler (called from IO thread)
  void asyncReadCallback(asio_ns::error_code const&) override;

  /// abort ongoing / unfinished requests (locally)
  void abortOngoingRequests(const fuerte::Error) override;

  /// abort all requests lingering in the queue
  void drainQueue(const fuerte::Error) override;

 private:
  static int on_begin_headers(nghttp2_session* session,
                              const nghttp2_frame* frame, void* user_data);
  static int on_header(nghttp2_session* session, const nghttp2_frame* frame,
                       const uint8_t* name, size_t namelen,
                       const uint8_t* value, size_t valuelen, uint8_t flags,
                       void* user_data);
  static int on_frame_recv(nghttp2_session* session, const nghttp2_frame* frame,
                           void* user_data);
  static int on_data_chunk_recv(nghttp2_session* session, uint8_t flags,
                                int32_t stream_id, const uint8_t* data,
                                size_t len, void* user_data);
  static int on_stream_close(nghttp2_session* session, int32_t stream_id,
                             uint32_t error_code, void* user_data);
  static int on_frame_not_send(nghttp2_session* session,
                               const nghttp2_frame* frame, int lib_error_code,
                               void* user_data);

 private:
  // ongoing Http2 stream
  struct Stream {
    velocypack::Buffer<uint8_t> data;

    RequestCallback callback;
    std::unique_ptr<arangodb::fuerte::v1::Request> request;
    std::unique_ptr<arangodb::fuerte::v1::Response> response;
    size_t responseOffset = 0;
    /// point in time when the message expires
    std::chrono::steady_clock::time_point expires;

    inline void invokeOnError(Error e) {
      callback(e, std::move(request), nullptr);
    }
  };

  void initNgHttp2Session();

  void sendHttp1UpgradeRequest();
  void readSwitchingProtocolsResponse();

  // adjust the timeouts (only call from IO-Thread)
  void setTimeout();

  // queue the response onto the session, call only on IO thread
  void queueHttp2Requests();

  /// actually perform writing
  void doWrite();

  Stream* findStream(int32_t sid) const;
  
  std::unique_ptr<Stream> eraseStream(int32_t sid);
  
  /// should close connection
  bool shouldStop() const;
  
  // ping ensures server does not close the connection
  void startPing();

 private:
  velocypack::Buffer<uint8_t> _outbuffer;

  /// elements to send out
  boost::lockfree::queue<Stream*, boost::lockfree::capacity<512>> _queue;

  std::map<int32_t, std::unique_ptr<Stream>> _streams;
  
  asio_ns::steady_timer _ping; // keep connection open

  const std::string _authHeader;

  nghttp2_session* _session = nullptr;

  std::atomic<uint32_t> _streamCount{0};

  bool _writing = false;
  std::atomic<bool> _signaledWrite{false};
};

}}}}  // namespace arangodb::fuerte::v1::http
#endif
