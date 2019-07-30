////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
/// @author Ewout Prangsma
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef ARANGO_CXX_DRIVER_VST_CONNECTION_H
#define ARANGO_CXX_DRIVER_VST_CONNECTION_H 1

#include "GeneralConnection.h"
#include "MessageStore.h"
#include "vst.h"

#include <boost/lockfree/queue.hpp>

// naming in this file will be closer to asio for internal functions and types
// functions that are exposed to other classes follow ArangoDB conding
// conventions

namespace arangodb { namespace fuerte { inline namespace v1 { namespace vst {

// Connection object that handles sending and receiving of
//  Velocystream Messages.
template <SocketType ST>
class VstConnection final : public fuerte::GeneralConnection<ST> {
 public:
  explicit VstConnection(EventLoopService& loop,
                         detail::ConnectionConfiguration const&);

  ~VstConnection();

 public:
  // this function prepares the request for sending
  // by creating a RequestItem and setting:
  //  - a messageid
  //  - the buffer to be send
  // this item is then moved to the request queue
  // and a write action is triggerd when there is
  // no other write in progress
  MessageID sendRequest(std::unique_ptr<Request>, RequestCallback) override;

  // Return the number of unfinished requests.
  std::size_t requestsLeft() const override {
    return (_loopState.load(std::memory_order_acquire) &
            WRITE_LOOP_QUEUE_MASK) +
           _messageStore.size();
  }

 protected:
  void finishConnect() override;

  // Thread-Safe: activate the writer loop (if off and items are queud)
  void startWriting() override;

  // called by the async_read handler (called from IO thread)
  void asyncReadCallback(asio_ns::error_code const&) override;

  /// abort ongoing / unfinished requests
  void abortOngoingRequests(const fuerte::Error) override;

  /// abort all requests lingering in the queue
  void drainQueue(const fuerte::Error) override;

 private:
  ///  Call on IO-Thread: writes out one queued request
  void asyncWriteNextRequest();

  // called by the async_write handler (called from IO thread)
  void asyncWriteCallback(asio_ns::error_code const& ec, size_t transferred,
                          std::shared_ptr<RequestItem>);

  // Thread-Safe: activate the read loop (if needed)
  void startReading();

  // Thread-Safe: stops read loop
  void stopReading();

 private:
  // Send out the authentication message on this connection
  void sendAuthenticationRequest();

  // Process the given incoming chunk.
  void processChunk(Chunk const& chunk);
  // Create a response object for given RequestItem & received response buffer.
  std::unique_ptr<Response> createResponse(
      RequestItem& item, std::unique_ptr<velocypack::Buffer<uint8_t>>&);

  // adjust the timeouts (only call from IO-Thread)
  void setTimeout();

 private:
  /// elements to send out
  boost::lockfree::queue<vst::RequestItem*, boost::lockfree::capacity<1024>>
      _writeQueue;

  /// stores in-flight messages
  MessageStore<vst::RequestItem> _messageStore;

  const VSTVersion _vstVersion;

  /// highest two bits mean read or write loops are active
  /// low 30 bit contain number of queued request items
  std::atomic<uint32_t> _loopState;
  static constexpr uint32_t READ_LOOP_ACTIVE = 1 << 31;
  static constexpr uint32_t WRITE_LOOP_ACTIVE = 1 << 30;
  static constexpr uint32_t LOOP_FLAGS = READ_LOOP_ACTIVE | WRITE_LOOP_ACTIVE;
  static constexpr uint32_t WRITE_LOOP_QUEUE_INC = 1;
  static constexpr uint32_t WRITE_LOOP_QUEUE_MASK = WRITE_LOOP_ACTIVE - 1;
  static_assert((WRITE_LOOP_ACTIVE & WRITE_LOOP_QUEUE_MASK) == 0, "");
  static_assert((WRITE_LOOP_ACTIVE & READ_LOOP_ACTIVE) == 0, "");
};

}}}}  // namespace arangodb::fuerte::v1::vst
#endif
