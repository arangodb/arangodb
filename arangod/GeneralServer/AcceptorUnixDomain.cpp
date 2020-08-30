////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
#include "Endpoint/ConnectionInfo.h"
#include "Endpoint/EndpointUnixDomain.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/HttpCommTask.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

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
  
  _open = true;
  asyncAccept();
}

void AcceptorUnixDomain::asyncAccept() {
  // In most cases _asioSocket will be nullptr here, however, if
  // the async_accept returns with an error, then an old _asioSocket
  // is already set. Therefore, we do no longer assert here that
  // _asioSocket is nullptr.
  IoContext& context = _server.selectIoContext();

  _asioSocket.reset(new AsioSocket<SocketType::Unix>(context));

  auto handler = [this](asio_ns::error_code const& ec) {
    if (ec) {
      handleError(ec);
      return;
    }

    // set the endpoint
    ConnectionInfo info;
    info.endpoint = _endpoint->specification();
    info.endpointType = _endpoint->domainType();
    info.encryptionType = _endpoint->encryption();
    info.serverAddress = _endpoint->host();
    info.serverPort = _endpoint->port();
    info.clientAddress = "local";
    info.clientPort = 0;

    auto commTask =
        std::make_shared<HttpCommTask<SocketType::Unix>>(_server,
                                                         std::move(info),
                                                         std::move(_asioSocket));
    _server.registerTask(std::move(commTask));
    this->asyncAccept();
  };

  // cppcheck-suppress accessMoved
  _acceptor.async_accept(_asioSocket->socket, _asioSocket->peer, handler);
}

void AcceptorUnixDomain::close() {
  if (_open) {
    _open = false;  // make sure this flag is reset to `false` before
                    // we cancel/close the acceptor, otherwise the
                    // handleError method would restart async_accept
                    // right away
    _acceptor.close();
    int error = 0;
    std::string path = static_cast<EndpointUnixDomain*>(_endpoint)->path();
    if (!basics::FileUtils::remove(path, &error)) {
      LOG_TOPIC("56b89", TRACE, arangodb::Logger::FIXME)
          << "unable to remove socket file '" << path << "'";
    }
  }
}

void AcceptorUnixDomain::cancel() {
  _acceptor.cancel();
}

