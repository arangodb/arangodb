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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "EndpointUnixDomain.h"

#ifdef ARANGODB_HAVE_DOMAIN_SOCKETS

#include <errno.h>
#include <stdio.h>
#include <sys/un.h>
#include <cstring>

#include "Basics/FileUtils.h"
#include "Basics/debugging.h"
#include "Endpoint/Endpoint.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

using namespace arangodb;
using namespace arangodb::basics;

EndpointUnixDomain::EndpointUnixDomain(EndpointType type, int listenBacklog,
                                       std::string const& path)
    : Endpoint(DomainType::UNIX, type, TransportType::HTTP,
               EncryptionType::NONE, "http+unix://" + path, listenBacklog),
      _path(path) {}

EndpointUnixDomain::~EndpointUnixDomain() {
  if (_connected) {
    disconnect();
  }
}

TRI_socket_t EndpointUnixDomain::connect(double connectTimeout, double requestTimeout) {
  TRI_socket_t listenSocket;
  TRI_invalidatesocket(&listenSocket);

  LOG_TOPIC("bd9f7", DEBUG, arangodb::Logger::FIXME)
      << "connecting to unix endpoint '" << _specification << "'";

  TRI_ASSERT(!TRI_isvalidsocket(_socket));
  TRI_ASSERT(!_connected);

  listenSocket = TRI_socket(AF_UNIX, SOCK_STREAM, 0);
  if (!TRI_isvalidsocket(listenSocket)) {
    LOG_TOPIC("112fd", ERR, arangodb::Logger::FIXME)
        << "socket() failed with " << errno << " (" << strerror(errno) << ")";
    return listenSocket;
  }

  struct sockaddr_un address;

  memset(&address, 0, sizeof(address));
  address.sun_family = AF_UNIX;
  snprintf(address.sun_path, 100, "%s", _path.c_str());

  if (_type == EndpointType::SERVER) {
    int result =
        TRI_bind(listenSocket, (struct sockaddr*)&address, (int)SUN_LEN(&address));
    if (result != 0) {
      // bind error
      LOG_TOPIC("56d98", ERR, arangodb::Logger::FIXME)
          << "bind() failed with " << errno << " (" << strerror(errno) << ")";
      TRI_CLOSE_SOCKET(listenSocket);
      TRI_invalidatesocket(&listenSocket);
      return listenSocket;
    }

    // listen for new connection, executed for server endpoints only
    LOG_TOPIC("bf147", TRACE, arangodb::Logger::FIXME) << "using backlog size " << _listenBacklog;
    result = TRI_listen(listenSocket, _listenBacklog);

    if (result < 0) {
      LOG_TOPIC("34922", ERR, arangodb::Logger::FIXME)
          << "listen() failed with " << errno << " (" << strerror(errno) << ")";
      TRI_CLOSE_SOCKET(listenSocket);
      TRI_invalidatesocket(&listenSocket);
      return listenSocket;
    }
  }

  else if (_type == EndpointType::CLIENT) {
    // connect to endpoint, executed for client endpoints only

    // set timeout
    setTimeout(listenSocket, connectTimeout);

    if (TRI_connect(listenSocket, (const struct sockaddr*)&address, SUN_LEN(&address)) != 0) {
      TRI_CLOSE_SOCKET(listenSocket);
      TRI_invalidatesocket(&listenSocket);
      return listenSocket;
    }
  }

  if (!setSocketFlags(listenSocket)) {
    TRI_CLOSE_SOCKET(listenSocket);
    TRI_invalidatesocket(&listenSocket);
    return listenSocket;
  }

  if (_type == EndpointType::CLIENT) {
    setTimeout(listenSocket, requestTimeout);
  }

  _connected = true;
  _socket = listenSocket;

  return _socket;
}

void EndpointUnixDomain::disconnect() {
  if (_connected) {
    TRI_ASSERT(TRI_isvalidsocket(_socket));

    _connected = false;
    TRI_CLOSE_SOCKET(_socket);

    TRI_invalidatesocket(&_socket);

    if (_type == EndpointType::SERVER) {
      int error = 0;
      if (!FileUtils::remove(_path, &error)) {
        LOG_TOPIC("9a8d6", TRACE, arangodb::Logger::FIXME)
            << "unable to remove socket file '" << _path << "'";
      }
    }
  }
}

bool EndpointUnixDomain::initIncoming(TRI_socket_t incoming) {
  return setSocketFlags(incoming);
}

#endif
