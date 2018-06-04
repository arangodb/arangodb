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

#include <asio/ip/tcp.hpp>
#include <asio/ssl.hpp>
#include <thread>

#include "Logger/Logger.h"

using namespace arangodb;

void SocketTcp::close(asio_ns::error_code& ec) {
  if (_socket.is_open()) {
    _socket.close(ec);
    if (ec && ec != asio_ns::error::not_connected) {
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "closing socket failed with: "
                                              << ec.message();
    }
  }
}

void SocketTcp::shutdownReceive(asio_ns::error_code& ec) {
  _socket.shutdown(asio_ns::ip::tcp::socket::shutdown_receive, ec);
}

void SocketTcp::shutdownSend(asio_ns::error_code& ec) {
  _socket.shutdown(asio_ns::ip::tcp::socket::shutdown_send, ec);
}
