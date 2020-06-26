////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "GeneralConnection.h"

#include "debugging.h"

namespace arangodb { namespace fuerte {

//template <SocketType ST>
//GeneralConnection<ST>::GeneralConnection(
//    EventLoopService& loop, detail::ConnectionConfiguration const& config)
//    : Connection(config),
//      _io_context(loop.nextIOContext()),
//      _proto(loop, *_io_context),
//      _state(Connection::State::Disconnected),
//      _numQueued(0) {}
//
///// @brief cancel the connection, unusable afterwards
//template <SocketType ST>
//void GeneralConnection<ST>::cancel() {
//
//}
//
//// Activate this connection.
//template <SocketType ST>
//void GeneralConnection<ST>::start() {
//  asio_ns::post(*this->_io_context, [self = Connection::shared_from_this()] {
//    static_cast<GeneralConnection<ST>&>(*self).startConnection();
//  });
//}
//
//template <SocketType ST>
//void GeneralConnection<ST>::startConnection() {
//  // start connecting only if state is disconnected
//  Connection::State exp = Connection::State::Disconnected;
//  if (_state.compare_exchange_strong(exp, Connection::State::Connecting)) {
//    FUERTE_LOG_DEBUG << "startConnection: this=" << this << "\n";
//    FUERTE_ASSERT(_config._maxConnectRetries > 0);
//    tryConnect(_config._maxConnectRetries, std::chrono::steady_clock::now(),
//               asio_ns::error_code());
//  } else {
//    FUERTE_LOG_DEBUG << "startConnection: this=" << this << " found unexpected state "
//      << static_cast<int>(exp) << " not equal to 'Disconnected'";
//    FUERTE_ASSERT(false);
//  }
//}
//
//// shutdown the connection and cancel all pending messages.
//template <SocketType ST>
//void GeneralConnection<ST>::shutdownConnection(const Error err, std::string const& msg) {}
//
//// Connect with a given number of retries
//template <SocketType ST>
//void GeneralConnection<ST>::tryConnect(unsigned retries,
//                                       std::chrono::steady_clock::time_point start,
//                                       asio_ns::error_code const& ec) {
//}
// asyncReadSome reads the next bytes from the server.
//template <SocketType ST>
//void GeneralConnection<ST>::asyncReadSome() {
//
//}


/// abort all requests lingering in the queue
//template <SocketType ST, typename T>
//void GeneralConnection<ST>::drainQueue(const fuerte::Error ec) {
//
//}

//template class arangodb::fuerte::GeneralConnection<SocketType::Tcp>;
//template class arangodb::fuerte::GeneralConnection<SocketType::Ssl>;
//#ifdef ASIO_HAS_LOCAL_SOCKETS
//template class arangodb::fuerte::GeneralConnection<SocketType::Unix>;
//#endif

}}  // namespace arangodb::fuerte
