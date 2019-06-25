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

#include "GeneralServer/AcceptorUnixDomain.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Endpoint/EndpointUnixDomain.h"
#include "GeneralServer/GeneralServer.h"
#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::rest;

void AcceptorUnixDomain::open() {
  std::string path(((EndpointUnixDomain*)_endpoint)->path());
  if (basics::FileUtils::exists(path)) {
    // socket file already exists
    LOG_TOPIC("e0ae1", WARN, arangodb::Logger::FIXME)
        << "socket file '" << path << "' already exists.";

    int error = 0;
    // delete previously existing socket file
    if (basics::FileUtils::remove(path, &error)) {
      LOG_TOPIC("2b5b6", WARN, arangodb::Logger::FIXME)
          << "deleted previously existing socket file '" << path << "'";
    } else {
      LOG_TOPIC("f6012", ERR, arangodb::Logger::FIXME)
          << "unable to delete previously existing socket file '" << path << "'";
    }
  }

  asio_ns::local::stream_protocol::stream_protocol::endpoint endpoint(path);
  _acceptor.open(endpoint.protocol());
  _acceptor.bind(endpoint);
  _acceptor.listen();
}

void AcceptorUnixDomain::asyncAccept() {
  TRI_ASSERT(!_peer);
  IoContext& context = _server.selectIoContext();

  _peer.reset(new AsioSocket<SocketType::Unix>(context));
  if (_peer == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "unexpected socket type");
  }
  _acceptor.async_accept(_peer->socket, _peer->peer, [](asio_ns::error_code ec) {
//#warning TODO
    TRI_ASSERT(false);
  });
}

void AcceptorUnixDomain::close() {
  if (_open) {
    _acceptor.close();
    int error = 0;
    std::string path = ((EndpointUnixDomain*)_endpoint)->path();
    if (!basics::FileUtils::remove(path, &error)) {
      LOG_TOPIC("56b89", TRACE, arangodb::Logger::FIXME)
          << "unable to remove socket file '" << path << "'";
    }
  }
  _open = false;
}
