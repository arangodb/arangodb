////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_SCHEDULER_SOCKET_SSL_TCP_H
#define ARANGOD_SCHEDULER_SOCKET_SSL_TCP_H 1

#include "GeneralServer/Socket.h"

#include "Logger/Logger.h"

namespace arangodb {
class SocketSslTcp final : public Socket {
  friend class AcceptorTcp;

 public:
  SocketSslTcp(rest::GeneralServer::IoContext& context, asio_ns::ssl::context&& sslContext)
      : Socket(context, /*encrypted*/ true),
        _sslContext(std::move(sslContext)),
        _sslSocket(context.newSslSocket(_sslContext)),
        _socket(_sslSocket->next_layer()),
        _peerEndpoint() {}

  SocketSslTcp(SocketSslTcp const& that) = delete;

  SocketSslTcp(SocketSslTcp&& that) = delete;

 public:
  std::string peerAddress() const override {
    return _peerEndpoint.address().to_string();
  }

  int peerPort() const override { return _peerEndpoint.port(); }

  void setNonBlocking(bool v) override { _socket.non_blocking(v); }

  size_t writeSome(basics::StringBuffer* buffer, asio_ns::error_code& ec) override {
    return _sslSocket->write_some(asio_ns::buffer(buffer->begin(), buffer->length()), ec);
  }

  void asyncWrite(asio_ns::mutable_buffers_1 const& buffer, AsyncHandler const& handler) override {
    return asio_ns::async_write(*_sslSocket, buffer, handler);
  }

  size_t readSome(asio_ns::mutable_buffers_1 const& buffer, asio_ns::error_code& ec) override {
    return _sslSocket->read_some(buffer, ec);
  }

  void asyncRead(asio_ns::mutable_buffers_1 const& buffer, AsyncHandler const& handler) override {
    return _sslSocket->async_read_some(buffer, handler);
  }

  std::size_t available(asio_ns::error_code& ec) override {
    return static_cast<size_t>(_socket.available(ec));
  }

  void close(asio_ns::error_code& ec) override {
    if (_socket.is_open()) {
      _socket.close(ec);
      if (ec && ec != asio_ns::error::not_connected) {
        LOG_TOPIC("0d3a4", DEBUG, Logger::COMMUNICATION)
            << "closing socket failed with: " << ec.message();
      }
    }
  }

 protected:
  bool sslHandshake() override;

  void shutdownReceive(asio_ns::error_code& ec) override {
    _socket.shutdown(asio_ns::ip::tcp::socket::shutdown_receive, ec);
  }

  void shutdownSend(asio_ns::error_code& ec) override {
    _socket.shutdown(asio_ns::ip::tcp::socket::shutdown_send, ec);
  }

 private:
  asio_ns::ssl::context _sslContext;
  std::unique_ptr<asio_ns::ssl::stream<asio_ns::ip::tcp::socket>> _sslSocket;
  asio_ns::ip::tcp::socket& _socket;
  asio_ns::ip::tcp::acceptor::endpoint_type _peerEndpoint;
};
}  // namespace arangodb

#endif
