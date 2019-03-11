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

#include "GeneralServer/Socket.h"

namespace arangodb {
namespace basics {
class StringBuffer;
}

class AcceptorUnixDomain;

class SocketUnixDomain final : public Socket {
  friend class AcceptorUnixDomain;

 public:
  explicit SocketUnixDomain(rest::GeneralServer::IoContext& context)
      : Socket(context, false), _socket(context.newDomainSocket()) {}

  SocketUnixDomain(SocketUnixDomain&& that) = delete;

  std::string peerAddress() const override { return "local"; }
  int peerPort() const override { return 0; }

  void setNonBlocking(bool v) override { _socket->non_blocking(v); }

  size_t writeSome(basics::StringBuffer* buffer, asio_ns::error_code& ec) override;

  void asyncWrite(asio_ns::mutable_buffers_1 const& buffer, AsyncHandler const& handler) override;

  size_t readSome(asio_ns::mutable_buffers_1 const& buffer, asio_ns::error_code& ec) override;

  std::size_t available(asio_ns::error_code& ec) override;

  void asyncRead(asio_ns::mutable_buffers_1 const& buffer, AsyncHandler const& handler) override;

 protected:
  bool sslHandshake() override { return false; }
  void shutdownReceive(asio_ns::error_code& ec) override;
  void shutdownSend(asio_ns::error_code& ec) override;
  void close(asio_ns::error_code& ec) override;

 private:
  std::unique_ptr<asio_ns::local::stream_protocol::socket> _socket;
  asio_ns::local::stream_protocol::acceptor::endpoint_type _peerEndpoint;
};
}  // namespace arangodb

#endif
