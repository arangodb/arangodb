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

#include <boost/lockfree/queue.hpp>

#include "GeneralConnection.h"
#include "vst.h"

// naming in this file will be closer to asio for internal functions and types
// functions that are exposed to other classes follow ArangoDB conding
// conventions

namespace arangodb { namespace fuerte { inline namespace v1 { namespace vst {

// Connection object that handles sending and receiving of
//  Velocystream Messages.
template <SocketType ST>
class VstConnection final : public fuerte::GeneralConnection<ST, vst::RequestItem> {
 public:
  explicit VstConnection(EventLoopService& loop,
                         detail::ConnectionConfiguration const&);

  ~VstConnection();

 public:
  // Return the number of unfinished requests.
  std::size_t requestsLeft() const override;

 protected:
  void finishConnect() override;

  ///  Call on IO-Thread: writes out one queued request
  void doWrite() override {
    asyncWriteNextRequest();
  }

  // called by the async_read handler (called from IO thread)
  void asyncReadCallback(asio_ns::error_code const&) override;

  /// abort ongoing / unfinished requests
  void abortOngoingRequests(const fuerte::Error) override;
  
  void setIOTimeout() override;
  
  std::unique_ptr<vst::RequestItem> createRequest(std::unique_ptr<Request>&& req,
                                    RequestCallback&& cb) override {
    return std::make_unique<vst::RequestItem>(std::move(req), std::move(cb));
  }

 private:
  ///  Call on IO-Thread: writes out one queued request
  void asyncWriteNextRequest();

  // called by the async_write handler (called from IO thread)
  void asyncWriteCallback(asio_ns::error_code const& ec);

  // Send out the authentication message on this connection
  void sendAuthenticationRequest();

  // Process the given incoming chunk.
  void processChunk(Chunk const& chunk);
  // Create a response object for given RequestItem & received response buffer.
  std::unique_ptr<Response> createResponse(
      RequestItem& item, std::unique_ptr<velocypack::Buffer<uint8_t>>&);
  
  void abortExpiredRequests();

 private:
  /// stores in-flight messages
  std::map<MessageID, std::unique_ptr<vst::RequestItem>> _messages;
  std::atomic<uint32_t> _numMessages;

  const VSTVersion _vstVersion;
};

}}}}  // namespace arangodb::fuerte::v1::vst
#endif
