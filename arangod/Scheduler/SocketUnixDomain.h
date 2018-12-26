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

#include <boost/asio/local/stream_protocol.hpp>

namespace arangodb {
namespace basics {
class StringBuffer;
}

class SocketUnixDomain final : public Socket {
 public:
  SocketUnixDomain(boost::asio::io_service& ioService, boost::asio::ssl::context&& context)
      : Socket(ioService, std::move(context), false),
        _socket(ioService),
        _peerEndpoint() {}

  SocketUnixDomain(SocketUnixDomain&& that) = default;

  void setNonBlocking(bool v) override { _socket.non_blocking(v); }

  std::string peerAddress() override { return "local"; }

  int peerPort() override { return 0; }

  bool sslHandshake() override { return false; }

  size_t write(basics::StringBuffer* buffer, boost::system::error_code& ec) override;

  void asyncWrite(boost::asio::mutable_buffers_1 const& buffer,
                  AsyncHandler const& handler) override;

  size_t read(boost::asio::mutable_buffers_1 const& buffer,
              boost::system::error_code& ec) override;

  std::size_t available(boost::system::error_code& ec) override;

  void asyncRead(boost::asio::mutable_buffers_1 const& buffer,
                 AsyncHandler const& handler) override;

 protected:
  void shutdownReceive(boost::system::error_code& ec) override;
  void shutdownSend(boost::system::error_code& ec) override;
  void close(boost::system::error_code& ec) override;

 public:
  boost::asio::local::stream_protocol::socket _socket;

  boost::asio::local::stream_protocol::acceptor::endpoint_type _peerEndpoint;
};
}  // namespace arangodb

#endif
