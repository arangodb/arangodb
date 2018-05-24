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
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_SCHEDULER_SOCKET_UNIXDOMAIN_H
#define ARANGOD_SCHEDULER_SOCKET_UNIXDOMAIN_H 1

#include "Scheduler/Socket.h"

#include <asio/local/stream_protocol.hpp>

namespace arangodb {
class AcceptorUnixDomain;
namespace basics {
class StringBuffer;
}

class SocketUnixDomain final : public Socket {
  friend class AcceptorUnixDomain;
  public:
    SocketUnixDomain(asio::io_context& ioService)
        : Socket(ioService, false), _socket(ioService) {}

    SocketUnixDomain(SocketUnixDomain&& that) = default;
  
    std::string peerAddress() const override { return "local"; }
    int peerPort() const override { return 0; }
  
    void setNonBlocking(bool v) override { _socket.non_blocking(v); }
    
    size_t writeSome(basics::StringBuffer* buffer, asio::error_code& ec) override;
    
    void asyncWrite(asio::mutable_buffers_1 const& buffer, AsyncHandler const& handler) override;
    
    size_t readSome(asio::mutable_buffers_1 const& buffer, asio::error_code& ec) override;
    
    std::size_t available(asio::error_code& ec) override;
  
    void asyncRead(asio::mutable_buffers_1 const& buffer, AsyncHandler const& handler) override;

  protected:
    bool sslHandshake() override { return false; }
    void shutdownReceive(asio::error_code& ec) override;
    void shutdownSend(asio::error_code& ec) override;
    void close(asio::error_code& ec) override;

private:
    asio::local::stream_protocol::socket _socket;
    asio::local::stream_protocol::acceptor::endpoint_type _peerEndpoint;
};
}

#endif
