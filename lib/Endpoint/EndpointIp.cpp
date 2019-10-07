////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include <errno.h>
#include <stdio.h>
#include <cstring>

#include "Basics/operating-system.h"

#ifdef TRI_HAVE_NETDB_H
#include <netdb.h>
#endif

#ifdef TRI_HAVE_NETINET_STAR_H
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

#include "EndpointIp.h"

#include "Basics/StringUtils.h"
#include "Basics/debugging.h"
#include "Endpoint/Endpoint.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

using namespace arangodb;
using namespace arangodb::basics;

#ifdef _WIN32
#define STR_ERROR()                                                  \
  windowsErrorBuf;                                                   \
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0, \
                windowsErrorBuf, sizeof(windowsErrorBuf), NULL);     \
  errno = GetLastError();
#else
// YYY #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
// YYY #warning FRANK move to system or os file
// YYY #endif
#define STR_ERROR() strerror(errno)
#endif

uint16_t const EndpointIp::_defaultPortHttp = 8529;
uint16_t const EndpointIp::_defaultPortVst = 8530;
char const* EndpointIp::_defaultHost = "127.0.0.1";

static std::string buildSpecification(Endpoint::DomainType domainType,
                                      Endpoint::TransportType transport,
                                      Endpoint::EncryptionType encryption,
                                      std::string const& host, uint16_t const port) {
  std::string specification;

  switch (transport) {
    case Endpoint::TransportType::HTTP:
      specification = "http+";
      break;
    case Endpoint::TransportType::VST:
      specification = "vst+";
      break;
  }

  switch (encryption) {
    case Endpoint::EncryptionType::NONE:
      specification += "tcp://";
      break;
    case Endpoint::EncryptionType::SSL:
      specification += "ssl://";
      break;
  }

  switch (domainType) {
    case Endpoint::DomainType::IPV4:
      specification += host + ":" + StringUtils::itoa(port);
      break;

    case Endpoint::DomainType::IPV6:
      specification += "[" + host + "]" + ":" + StringUtils::itoa(port);
      break;

    default:
      TRI_ASSERT(false);
      break;
  }

  return specification;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an IP socket endpoint
////////////////////////////////////////////////////////////////////////////////

EndpointIp::EndpointIp(DomainType domainType, EndpointType type, TransportType transport,
                       EncryptionType encryption, int listenBacklog, bool reuseAddress,
                       std::string const& host, uint16_t const port)
    : Endpoint(domainType, type, transport, encryption,
               buildSpecification(domainType, transport, encryption, host, port),
               listenBacklog),
      _host(host),
      _port(port),
      _reuseAddress(reuseAddress) {
  TRI_ASSERT(domainType == Endpoint::DomainType::IPV4 ||
             domainType == Endpoint::DomainType::IPV6);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an IP socket endpoint
////////////////////////////////////////////////////////////////////////////////

EndpointIp::~EndpointIp() {
  if (_connected) {
    disconnect();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief connects a socket
////////////////////////////////////////////////////////////////////////////////

TRI_socket_t EndpointIp::connectSocket(const struct addrinfo* aip,
                                       double connectTimeout, double requestTimeout) {
  char const* pErr;
  char errBuf[1080];
#ifdef _WIN32
  char windowsErrorBuf[1080];
#endif

  // set address and port
  char host[NI_MAXHOST];
  char serv[NI_MAXSERV];

  if (::getnameinfo(aip->ai_addr, (socklen_t)aip->ai_addrlen, host, sizeof(host),
                    serv, sizeof(serv), NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
    LOG_TOPIC("6f759", TRACE, arangodb::Logger::FIXME)
        << "bind to address '" << host << "', port " << _port;
  }

  TRI_socket_t listenSocket;
  listenSocket = TRI_socket(aip->ai_family, aip->ai_socktype, aip->ai_protocol);

  if (!TRI_isvalidsocket(listenSocket)) {
    pErr = STR_ERROR();
    snprintf(errBuf, sizeof(errBuf), "socket() failed with %d - %s", errno, pErr);

    _errorMessage = errBuf;
    return listenSocket;
  }

  if (_type == EndpointType::SERVER) {
#ifdef _WIN32
    int excl = 1;
    if (TRI_setsockopt(listenSocket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                       reinterpret_cast<char*>(&excl), sizeof(excl)) == -1) {
      pErr = STR_ERROR();
      snprintf(errBuf, sizeof(errBuf), "setsockopt() failed with #%d - %s", errno, pErr);

      _errorMessage = errBuf;

      TRI_CLOSE_SOCKET(listenSocket);
      TRI_invalidatesocket(&listenSocket);
      return listenSocket;
    }
#else
    // try to reuse address
    if (_reuseAddress) {
      int opt = 1;
      if (TRI_setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR,
                         reinterpret_cast<char*>(&opt), sizeof(opt)) == -1) {
        pErr = STR_ERROR();
        snprintf(errBuf, sizeof(errBuf), "setsockopt() failed with #%d - %s", errno, pErr);

        _errorMessage = errBuf;

        TRI_CLOSE_SOCKET(listenSocket);
        TRI_invalidatesocket(&listenSocket);
        return listenSocket;
      }
    }
#endif

    // server needs to bind to socket
    int result = TRI_bind(listenSocket, aip->ai_addr, aip->ai_addrlen);

    if (result != 0) {
      pErr = STR_ERROR();
      snprintf(errBuf, sizeof(errBuf),
               "bind(address '%s', port %d) failed with #%d - %s", host,
               (int)_port, errno, pErr);

      _errorMessage = errBuf;

      TRI_CLOSE_SOCKET(listenSocket);

      TRI_invalidatesocket(&listenSocket);
      return listenSocket;
    }

    // listen for new connection, executed for server endpoints only
    LOG_TOPIC("779e6", TRACE, arangodb::Logger::FIXME) << "using backlog size " << _listenBacklog;
    result = TRI_listen(listenSocket, _listenBacklog);

    if (result != 0) {
      pErr = STR_ERROR();
      snprintf(errBuf, sizeof(errBuf), "listen() failed with #%d - %s", errno, pErr);

      _errorMessage = errBuf;

      TRI_CLOSE_SOCKET(listenSocket);
      TRI_invalidatesocket(&listenSocket);
      return listenSocket;
    }
  } else if (_type == EndpointType::CLIENT) {
    // connect to endpoint, executed for client endpoints only

    // set timeout
    setTimeout(listenSocket, connectTimeout);

    int result = TRI_connect(listenSocket, (const struct sockaddr*)aip->ai_addr,
                             aip->ai_addrlen);

    if (result != 0) {
      pErr = STR_ERROR();
      snprintf(errBuf, sizeof(errBuf), "connect() failed with #%d - %s", errno, pErr);

      _errorMessage = errBuf;

      TRI_CLOSE_SOCKET(listenSocket);
      TRI_invalidatesocket(&listenSocket);
      return listenSocket;
    }
  }

  if (!setSocketFlags(listenSocket)) {  // set some common socket flags for
                                        // client and server
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

////////////////////////////////////////////////////////////////////////////////
/// @brief connect the endpoint
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

TRI_socket_t EndpointIp::connect(double connectTimeout, double requestTimeout) {
  struct addrinfo* result = nullptr;
  struct addrinfo* aip;
  struct addrinfo hints;
  int error;
  TRI_socket_t listenSocket;
  TRI_invalidatesocket(&listenSocket);

  LOG_TOPIC("67fe1", DEBUG, arangodb::Logger::FIXME)
      << "connecting to ip endpoint '" << _specification << "'";

  TRI_ASSERT(!TRI_isvalidsocket(_socket));
  TRI_ASSERT(!_connected);

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = domain();  // Allow IPv4 or IPv6
  hints.ai_flags = TRI_CONNECT_AI_FLAGS;
  hints.ai_socktype = SOCK_STREAM;

  std::string portString = StringUtils::itoa(_port);

  error = getaddrinfo(_host.c_str(), portString.c_str(), &hints, &result);

  if (error != 0) {
    int lastError = WSAGetLastError();

    if (error == WSANOTINITIALISED) {
      // can not call WSAGetLastError - since the socket layer hasn't been
      // initialized yet!
      lastError = error;
    } else {
      lastError = WSAGetLastError();
    }

    switch (lastError) {
      case WSANOTINITIALISED: {
        _errorMessage =
            std::string("getaddrinfo for host '") + _host +
            std::string(
                "': WSAStartup was not called or not called successfully.");
        break;
      }
      default: {
        _errorMessage = std::string("getaddrinfo for host '") + _host +
                        std::string("': ") + gai_strerror(error);
        break;
      }
    }

    if (result != nullptr) {
      freeaddrinfo(result);
    }

    return listenSocket;
  }

  // Try all returned addresses until one works
  for (aip = result; aip != nullptr; aip = aip->ai_next) {
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

TRI_socket_t EndpointIp::connect(double connectTimeout, double requestTimeout) {
  struct addrinfo* result = nullptr;
  struct addrinfo* aip;
  struct addrinfo hints;
  int error;
  TRI_socket_t listenSocket;
  TRI_invalidatesocket(&listenSocket);

  LOG_TOPIC("3906e", DEBUG, arangodb::Logger::FIXME)
      << "connecting to ip endpoint '" << _specification << "'";

  TRI_ASSERT(!TRI_isvalidsocket(_socket));
  TRI_ASSERT(!_connected);

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = domain();  // Allow IPv4 or IPv6
  hints.ai_flags = TRI_CONNECT_AI_FLAGS;
  hints.ai_socktype = SOCK_STREAM;

  std::string portString = StringUtils::itoa(_port);

  error = getaddrinfo(_host.c_str(), portString.c_str(), &hints, &result);

  if (error != 0) {
    _errorMessage = std::string("getaddrinfo for host '") + _host +
                    std::string("': ") + gai_strerror(error);

    if (result != nullptr) {
      freeaddrinfo(result);
    }
    return listenSocket;
  }

  // Try all returned addresses until one works
  for (aip = result; aip != nullptr; aip = aip->ai_next) {
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

void EndpointIp::disconnect() {
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

bool EndpointIp::initIncoming(TRI_socket_t incoming) {
  // disable nagle's algorithm
  int n = 1;
  int res = TRI_setsockopt(incoming, IPPROTO_TCP, TCP_NODELAY, (char*)&n, sizeof(n));

  if (res != 0) {
    char const* pErr;
    char errBuf[256];
#ifdef _WIN32
    char windowsErrorBuf[256];
#endif

    pErr = STR_ERROR();
    snprintf(errBuf, sizeof(errBuf), "setsockopt failed with #%d - %s", errno, pErr);

    _errorMessage = errBuf;
    return false;
  }

  return setSocketFlags(incoming);
}
