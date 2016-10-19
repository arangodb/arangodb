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

#ifndef ARANGOD_SCHEDULER_SOCKET_TCP_H
#define ARANGOD_SCHEDULER_SOCKET_TCP_H 1

#include "Scheduler/Socket.h"

#include <boost/asio/ip/tcp.hpp>

namespace arangodb {
class SocketTcp: public Socket {
  public:
    SocketTcp(boost::asio::io_service& ioService,
           boost::asio::ssl::context&& context, bool encrypted)
        : Socket(ioService, std::move(context), encrypted),
          _sslSocket(ioService, _context),
          _socket(_sslSocket.next_layer()),
          _peerEndpoint() {}

    SocketTcp(SocketTcp&& that) = default;
    
    void close() override { _socket.close(); }
    
    void close(boost::system::error_code& ec) override { _socket.close(ec); }
    
    void setNonBlocking(bool v) override { _socket.non_blocking(v); }
    
    boost::asio::serial_port_service::native_handle_type nativeHandle() override { return _socket.native_handle(); }
    
    std::string peerAddress() override { return _peerEndpoint.address().to_string(); }
    
    int peerPort() override { return _peerEndpoint.port(); }
    
    bool sslHandshake() override { return socketcommon::doSslHandshake(_sslSocket); }
    
    size_t write(basics::StringBuffer* buffer, boost::system::error_code& ec) override;
    
    void asyncWrite(boost::asio::mutable_buffers_1 const& buffer, AsyncHandler const& handler) override;
    
    size_t read(boost::asio::mutable_buffers_1 const& buffer, boost::system::error_code& ec) override;
    
    void shutdownReceive() override;
    
    void shutdownReceive(boost::system::error_code& ec) override;
    
    void shutdownSend(boost::system::error_code& ec) override;
    
    int available(boost::system::error_code& ec) override;
    
    void asyncRead(boost::asio::mutable_buffers_1 const& buffer, AsyncHandler const& handler) override;

  public:
    boost::asio::ssl::stream<boost::asio::ip::tcp::socket> _sslSocket;
    boost::asio::ip::tcp::socket& _socket;

    boost::asio::ip::tcp::acceptor::endpoint_type _peerEndpoint;
};
}

#endif
