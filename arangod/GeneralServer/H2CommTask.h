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

#ifndef ARANGOD_GENERAL_SERVER_H2_COMM_TASK_H
#define ARANGOD_GENERAL_SERVER_H2_COMM_TASK_H 1

#include "GeneralServer/AsioSocket.h"
#include "GeneralServer/GeneralCommTask.h"

#include <nghttp2/nghttp2.h>
#include <velocypack/StringRef.h>
#include <boost/lockfree/queue.hpp>
#include <memory>

namespace arangodb {
class HttpRequest;

/// @brief maximum number of concurrent streams
static constexpr uint32_t H2MaxConcurrentStreams = 32;

namespace rest {

struct H2Response;
  
template <SocketType T>
class H2CommTask final : public GeneralCommTask<T> {
 public:
  H2CommTask(GeneralServer& server, ConnectionInfo, std::unique_ptr<AsioSocket<T>> so);
  ~H2CommTask() noexcept;

  void start() override;
  /// @brief upgrade from  H1 connection, must not call start
  void upgradeHttp1(std::unique_ptr<HttpRequest> req);
  
 protected:
  virtual bool readCallback(asio_ns::error_code ec) override;
  virtual void setIOTimeout() override;

  virtual void sendResponse(std::unique_ptr<GeneralResponse> response,
                            RequestStatistics::Item stat) override;

  virtual std::unique_ptr<GeneralResponse> createResponse(rest::ResponseCode,
                                                          uint64_t messageId) override;

 private:
  static int on_begin_headers(nghttp2_session* session,
                              const nghttp2_frame* frame, void* user_data);
  static int on_header(nghttp2_session* session, const nghttp2_frame* frame,
                       const uint8_t* name, size_t namelen, const uint8_t* value,
                       size_t valuelen, uint8_t flags, void* user_data);
  static int on_frame_recv(nghttp2_session* session, const nghttp2_frame* frame,
                           void* user_data);
  static int on_data_chunk_recv(nghttp2_session* session, uint8_t flags, int32_t stream_id,
                                const uint8_t* data, size_t len, void* user_data);
  static int on_stream_close(nghttp2_session* session, int32_t stream_id,
                             uint32_t error_code, void* user_data);
  static int on_frame_send(nghttp2_session* session, const nghttp2_frame* frame,
                           void* user_data);

  static int on_frame_not_send(nghttp2_session* session, const nghttp2_frame* frame,
                               int lib_error_code, void* user_data);

 private:
  // ongoing Http2 stream
  struct Stream final {
    explicit Stream(std::unique_ptr<HttpRequest> req)
        : request(std::move(req)) {}

    std::string origin;

    std::unique_ptr<HttpRequest> request;
    std::unique_ptr<H2Response> response;  // hold response memory

    size_t headerBuffSize = 0;  // total header size
    size_t responseOffset = 0;  // current offset in response body
  };

  /// init h2 session
  void initNgHttp2Session();

  /// handle stream request in arangodb
  void processStream(Stream& strm);

  /// should close connection
  bool shouldStop() const;

  // queue the response onto the session, call only on IO thread
  void queueHttp2Responses();

  /// actually perform writing
  void doWrite();

  Stream* createStream(int32_t sid, std::unique_ptr<HttpRequest>);
  Stream* findStream(int32_t sid);

 private:
  velocypack::Buffer<uint8_t> _outbuffer;

  boost::lockfree::queue<H2Response*, boost::lockfree::capacity<H2MaxConcurrentStreams>> _responses;

  std::map<int32_t, Stream> _streams;

  nghttp2_session* _session = nullptr;

  std::atomic<unsigned> _numProcessing{0};

  std::atomic<bool> _signaledWrite{false};
};
}  // namespace rest
}  // namespace arangodb

#endif
