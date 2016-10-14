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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_SCHEDULER_SOCKET_H
#define ARANGOD_SCHEDULER_SOCKET_H 1

#include "Basics/Common.h"

#include <boost/asio/ssl.hpp>

namespace arangodb {
struct Socket {
  Socket(boost::asio::io_service& ioService,
         boost::asio::ssl::context&& context, bool encrypted)
      : _context(std::move(context)),
        _sslSocket(ioService, _context),
        _socket(_sslSocket.next_layer()),
        _peerEndpoint(),
        _encrypted(encrypted) {}

  Socket(Socket&& that) = default;

  boost::asio::ssl::context _context;
  boost::asio::ssl::stream<boost::asio::ip::tcp::socket> _sslSocket;
  boost::asio::ip::tcp::socket& _socket;

  boost::asio::ip::tcp::acceptor::endpoint_type _peerEndpoint;

  bool _encrypted;
};
}

#endif
