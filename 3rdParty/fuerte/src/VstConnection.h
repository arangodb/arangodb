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

#include "AsioConnection.h"
#include "vst.h"

// naming in this file will be closer to asio for internal functions and types
// functions that are exposed to other classes follow ArangoDB conding
// conventions

namespace arangodb { namespace fuerte { inline namespace v1 { namespace vst {

class VstConnection final
    : public AsioConnection<arangodb::fuerte::v1::vst::RequestItem> {
 public:
  explicit VstConnection(std::shared_ptr<asio_ns::io_context> const&,
                         detail::ConnectionConfiguration const&);
  virtual ~VstConnection();

 public:
  // this function prepares the request for sending
  // by creating a RequestItem and setting:
  //  - a messageid
  //  - the buffer to be send
  // this item is then moved to the request queue
  // and a write action is triggerd when there is
  // no other write in progress
  MessageID sendRequest(std::unique_ptr<Request>, RequestCallback) override;

 public:
  // Return the number of unfinished requests.
  size_t requestsLeft() const override;

 private:
  // socket connection is up (with optional SSL), now initiate the VST protocol.
  void finishInitialization() override;

  // fetch the buffers for the write-loop (called from IO thread)
  std::vector<asio_ns::const_buffer> prepareRequest(
      std::shared_ptr<RequestItem> const&) override;

  // Thread-Safe: activate the writer loop (if off and items are queud)
  void startWriting();

  // called by the async_write handler (called from IO thread)
  void asyncWriteCallback(asio_ns::error_code const& error,
                          size_t transferred,
                          std::shared_ptr<RequestItem>) override;

  // Thread-Safe: activate the read loop (if needed)
  void startReading();

  // Thread-Safe: stops read loop
  void stopReading();

  // called by the async_read handler (called from IO thread)
  void asyncReadCallback(asio_ns::error_code const&,
                         size_t transferred) override;

 private:
  // Send out the authentication message on this connection
  void sendAuthenticationRequest();

  // Process the given incoming chunk.
  void processChunk(ChunkHeader&& chunk, asio_ns::const_buffer const&);
  // Create a response object for given RequestItem & received response buffer.
  std::unique_ptr<Response> createResponse(RequestItem& item,
                                           std::unique_ptr<velocypack::Buffer<uint8_t>>&);
      
  // adjust the timeouts (only call from IO-Thread)
  void setTimeout();

 private:
  const VSTVersion _vstVersion;
  /// @brief timer to handle connection timeouts
  asio_ns::steady_timer _requestTimeout;
};

}}}}  // namespace arangodb::fuerte::v1::vst
#endif
