////////////////////////////////////////////////////////////////////////////////
/// @brief connection endpoint, ip-based
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Oreste Costa-Panaia
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "EndpointIp.h"

#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/logging.h"

#include "Rest/Endpoint.h"

using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                        EndpointIp
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                               static initialisers
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief set port if none specified
////////////////////////////////////////////////////////////////////////////////

const uint16_t EndpointIp::_defaultPort = 8529;

////////////////////////////////////////////////////////////////////////////////
/// @brief default host if none specified
////////////////////////////////////////////////////////////////////////////////

const std::string EndpointIp::_defaultHost = "127.0.0.1";

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an IP socket endpoint
////////////////////////////////////////////////////////////////////////////////

EndpointIp::EndpointIp (const Endpoint::EndpointType type,
                        const Endpoint::DomainType domainType,
                        const Endpoint::EncryptionType encryption,
                        const std::string& specification,
                        int listenBacklog,
                        bool reuseAddress,
                        const std::string& host,
                        const uint16_t port)
  : Endpoint(type, domainType, encryption, specification, listenBacklog),
    _reuseAddress(reuseAddress),
    _host(host),
    _port(port) {

  TRI_ASSERT(domainType == DOMAIN_IPV4 || domainType == Endpoint::DOMAIN_IPV6);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an IP socket endpoint
////////////////////////////////////////////////////////////////////////////////

EndpointIp::~EndpointIp () {
  if (_connected) {
    disconnect();
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief connects a socket
////////////////////////////////////////////////////////////////////////////////

TRI_socket_t EndpointIp::connectSocket (const struct addrinfo* aip,
                                        double connectTimeout,
                                        double requestTimeout) {
  // set address and port
  char host[NI_MAXHOST];
  char serv[NI_MAXSERV];

  if (::getnameinfo(aip->ai_addr, (socklen_t) aip->ai_addrlen,
                    host, sizeof(host),
                    serv, sizeof(serv), NI_NUMERICHOST | NI_NUMERICSERV) == 0) {

    LOG_TRACE("bind to address '%s', port %d", host, (int) _port);
  }

  TRI_socket_t listenSocket;
  listenSocket = TRI_socket(aip->ai_family, aip->ai_socktype, aip->ai_protocol);

  if (! TRI_isvalidsocket(listenSocket)) {
    LOG_ERROR("socket() failed with %d (%s)", errno, strerror(errno));
    return listenSocket;
  }

  if (_type == ENDPOINT_SERVER) {

#ifdef _WIN32
    int excl = 1;
    if (TRI_setsockopt(listenSocket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
          reinterpret_cast<char*> (&excl), sizeof (excl)) == -1) {
      LOG_ERROR("setsockopt() failed with %d", WSAGetLastError());

      TRI_CLOSE_SOCKET(listenSocket);
      TRI_invalidatesocket(&listenSocket);
      return listenSocket;
    }
#else
    // try to reuse address
    if (_reuseAddress) {
      int opt = 1;
      if (TRI_setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*> (&opt), sizeof (opt)) == -1) {
        LOG_ERROR("setsockopt() failed with %d (%s)", errno, strerror(errno));

        TRI_CLOSE_SOCKET(listenSocket);
        TRI_invalidatesocket(&listenSocket);
        return listenSocket;
      }
    }
#endif

    // server needs to bind to socket
    int result = TRI_bind(listenSocket, aip->ai_addr, (int) aip->ai_addrlen);

    if (result != 0) {
      // error
#ifndef _WIN32
      LOG_ERROR("bind() failed with %d (%s)", errno, strerror(errno));
#else
      LOG_ERROR("bind() failed with %d", WSAGetLastError());
#endif
      TRI_CLOSE_SOCKET(listenSocket);

      TRI_invalidatesocket(&listenSocket);
      return listenSocket;
    }

    // listen for new connection, executed for server endpoints only
    LOG_TRACE("using backlog size %d", (int) _listenBacklog);
    result = TRI_listen(listenSocket, _listenBacklog);

    if (result != 0) {
      // todo: get the correct error code using WSAGetLastError for windows
      LOG_ERROR("listen() failed with %d (%s)", errno, strerror(errno));

      TRI_CLOSE_SOCKET(listenSocket);
      TRI_invalidatesocket(&listenSocket);
      return listenSocket;
    }
  }
  else if (_type == ENDPOINT_CLIENT) {
    // connect to endpoint, executed for client endpoints only

    // set timeout
    setTimeout(listenSocket, connectTimeout);

    int result = TRI_connect(listenSocket, (const struct sockaddr*) aip->ai_addr, (int) aip->ai_addrlen);

    if (result != 0) {
      TRI_CLOSE_SOCKET(listenSocket);
      TRI_invalidatesocket(&listenSocket);
      return listenSocket;
    }
  }

  if (! setSocketFlags(listenSocket)) { // set some common socket flags for client and server
    TRI_CLOSE_SOCKET(listenSocket);
    TRI_invalidatesocket(&listenSocket);
    return listenSocket;
  }

  if (_type == ENDPOINT_CLIENT) {
    setTimeout(listenSocket, requestTimeout);
  }

  _connected = true;
  _socket = listenSocket;

  return _socket;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief connect the endpoint
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

TRI_socket_t EndpointIp::connect (double connectTimeout,
                                  double requestTimeout) {
  struct addrinfo* result = 0;
  struct addrinfo* aip;
  struct addrinfo hints;
  int error;
  TRI_socket_t listenSocket;
  TRI_invalidatesocket(&listenSocket);

  LOG_DEBUG("connecting to ip endpoint '%s'", _specification.c_str());

  TRI_ASSERT(!TRI_isvalidsocket(_socket));
  TRI_ASSERT(!_connected);

  memset(&hints, 0, sizeof (struct addrinfo));
  hints.ai_family = getDomain(); // Allow IPv4 or IPv6
  hints.ai_flags = TRI_CONNECT_AI_FLAGS;
  hints.ai_socktype = SOCK_STREAM;

  string portString = StringUtils::itoa(_port);

  error = getaddrinfo(_host.c_str(), portString.c_str(), &hints, &result);

  if (error != 0) {
    int lastError = WSAGetLastError();

    if (error == WSANOTINITIALISED) {
      // can not call WSAGetLastError - since the socket layer hasn't been initialised yet!
      lastError = error;
    }
    else {
      lastError = WSAGetLastError();
    }

    switch (lastError) {
      case WSANOTINITIALISED: {
        LOG_ERROR("getaddrinfo for host '%s': WSAStartup was not called or not called successfully.", _host.c_str());
        break;
      }
      default: {
        LOG_ERROR("getaddrinfo for host '%s': %s", _host.c_str(), gai_strerror(error));
        break;
      }
    }

    if (result != 0) {
      freeaddrinfo(result);
    }

    return listenSocket;
  }


  // Try all returned addresses until one works
  for (aip = result; aip != NULL; aip = aip->ai_next) {
    // try to bind the address info pointer
    listenSocket = connectSocket(aip, connectTimeout, requestTimeout);
    if (TRI_isvalidsocket(listenSocket)) {
      // OK
      break;
    }
  }

  freeaddrinfo(result);

  return listenSocket;
}

#else

TRI_socket_t EndpointIp::connect (double connectTimeout, double requestTimeout) {
  struct addrinfo* result = 0;
  struct addrinfo* aip;
  struct addrinfo hints;
  int error;
  TRI_socket_t listenSocket;
  TRI_invalidatesocket(&listenSocket);

  LOG_DEBUG("connecting to ip endpoint '%s'", _specification.c_str());

  TRI_ASSERT(!TRI_isvalidsocket(_socket));
  TRI_ASSERT(!_connected);

  memset(&hints, 0, sizeof (struct addrinfo));
  hints.ai_family = getDomain(); // Allow IPv4 or IPv6
  hints.ai_flags = TRI_CONNECT_AI_FLAGS;
  hints.ai_socktype = SOCK_STREAM;

  string portString = StringUtils::itoa(_port);

  error = getaddrinfo(_host.c_str(), portString.c_str(), &hints, &result);

  if (error != 0) {
    LOG_ERROR("getaddrinfo for host '%s': %s", _host.c_str(), gai_strerror(error));

    if (result != 0) {
      freeaddrinfo(result);
    }
    return listenSocket;
  }


  // Try all returned addresses until one works
  for (aip = result; aip != NULL; aip = aip->ai_next) {
    // try to bind the address info pointer
    listenSocket = connectSocket(aip, connectTimeout, requestTimeout);
    if (TRI_isvalidsocket(listenSocket)) {
      // OK
      break;
    }
  }

  freeaddrinfo(result);

  return listenSocket;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an IPv4 socket endpoint
////////////////////////////////////////////////////////////////////////////////

void EndpointIp::disconnect () {
  if (_connected) {
    TRI_ASSERT(TRI_isvalidsocket(_socket));

    _connected = false;
    TRI_CLOSE_SOCKET(_socket);
    TRI_invalidatesocket(&_socket);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief init an incoming connection
////////////////////////////////////////////////////////////////////////////////

bool EndpointIp::initIncoming (TRI_socket_t incoming) {
  // disable nagle's algorithm
  int n = 1;
  int res = TRI_setsockopt(incoming, IPPROTO_TCP, TCP_NODELAY, (char*) &n, sizeof(n));

  if (res != 0 ) {
    // todo: get correct windows error code
    LOG_WARNING("setsockopt failed with %d (%s)", errno, strerror(errno));

    return false;
  }

  return setSocketFlags(incoming);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
