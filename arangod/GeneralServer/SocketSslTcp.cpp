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

#include "GeneralServer/SocketSslTcp.h"

#include <thread>

using namespace arangodb;

bool SocketSslTcp::sslHandshake() {
  asio_ns::error_code ec;

  uint64_t tries = 0;
  double start = 0.0;

  while (true) {
    ec.clear();
    _sslSocket->handshake(asio_ns::ssl::stream_base::handshake_type::server, ec);

    if (ec.value() != asio_ns::error::would_block) {
      break;
    }

    // got error EWOULDBLOCK and need to try again
    ++tries;

    // following is a helpless fix for connections hanging in the handshake
    // phase forever. we've seen this happening when the underlying peer
    // connection was closed during the handshake.
    // with the helpless fix, handshakes will be aborted it they take longer
    // than x seconds. a proper fix is to make the handshake run asynchronously
    // and somehow signal it that the connection got closed. apart from that
    // running it asynchronously will not block the scheduler thread as it
    // does now. anyway, even the helpless fix allows self-healing of busy
    // scheduler threads after a network failure
    if (tries == 1) {
      // capture start time of handshake
      start = TRI_microtime();
    } else if (tries % 50 == 0) {
      // check if we have spent more than x seconds handshaking and then abort
      TRI_ASSERT(start != 0.0);

      if (TRI_microtime() - start >= 3) {
#if ARANGODB_STANDALONE_ASIO
        ec.assign(asio_ns::error::connection_reset, std::generic_category());
#else
        ec.assign(asio_ns::error::connection_reset, boost::system::generic_category());
#endif
        LOG_TOPIC("aae1b", DEBUG, Logger::COMMUNICATION)
            << "forcefully shutting down connection after wait time";
        break;
      } else {
        std::this_thread::sleep_for(std::chrono::microseconds(10000));
      }
    }

    // next iteration
  }

  if (ec) {
    // this message will also be emitted if a connection is attempted
    // with a wrong protocol (e.g. HTTP instead of SSL/TLS). so it's
    // definitely not worth logging an error here
    LOG_TOPIC("cb6ca", DEBUG, Logger::COMMUNICATION)
        << "unable to perform ssl handshake: " << ec.message() << " : " << ec.value();
    return false;
  }
  return true;
}
