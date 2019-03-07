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

#include "Scheduler/SocketTcp.h"

using namespace arangodb;

size_t SocketTcp::write(basics::StringBuffer* buffer, boost::system::error_code& ec) {
  MUTEX_LOCKER(guard, _lock);
  if (_encrypted) {
    return socketcommon::doWrite(_sslSocket, buffer, ec);
  } else {
    return socketcommon::doWrite(_socket, buffer, ec);
  }
}

void SocketTcp::asyncWrite(boost::asio::mutable_buffers_1 const& buffer,
                           AsyncHandler const& handler) {
  MUTEX_LOCKER(guard, _lock);
  if (_encrypted) {
    return socketcommon::doAsyncWrite(_sslSocket, buffer, handler);
  } else {
    return socketcommon::doAsyncWrite(_socket, buffer, handler);
  }
}

size_t SocketTcp::read(boost::asio::mutable_buffers_1 const& buffer,
                       boost::system::error_code& ec) {
  MUTEX_LOCKER(guard, _lock);
  if (_encrypted) {
    return socketcommon::doRead(_sslSocket, buffer, ec);
  } else {
    return socketcommon::doRead(_socket, buffer, ec);
  }
}

void SocketTcp::shutdown(boost::system::error_code& ec, bool closeSend, bool closeReceive) {
  MUTEX_LOCKER(guard, _lock);
  Socket::shutdown(ec, closeSend, closeReceive);
}

void SocketTcp::close(boost::system::error_code& ec) {
  MUTEX_LOCKER(guard, _lock);
  if (_socket.is_open()) {
    _socket.close(ec);
    if (ec && ec != boost::asio::error::not_connected) {
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
          << "closing socket failed with: " << ec.message();
    }
  }
}

std::size_t SocketTcp::available(boost::system::error_code& ec) {
  MUTEX_LOCKER(guard, _lock);
  return static_cast<size_t>(_socket.available(ec));
}

void SocketTcp::asyncRead(boost::asio::mutable_buffers_1 const& buffer,
                          AsyncHandler const& handler) {
  MUTEX_LOCKER(guard, _lock);
  if (_encrypted) {
    return socketcommon::doAsyncRead(_sslSocket, buffer, handler);
  } else {
    return socketcommon::doAsyncRead(_socket, buffer, handler);
  }
}

void SocketTcp::shutdownReceive(boost::system::error_code& ec) {
  _socket.shutdown(boost::asio::ip::tcp::socket::shutdown_receive, ec);
}

void SocketTcp::shutdownSend(boost::system::error_code& ec) {
  _socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
}
