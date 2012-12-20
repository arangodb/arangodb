////////////////////////////////////////////////////////////////////////////////
/// @brief connection endpoint, ip-based
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. O
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "EndpointIp.h"

#include "Basics/FileUtils.h"
#include <Basics/StringUtils.h>
#include "Logger/Logger.h"

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
/// @addtogroup Rest
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief set port if none specified
////////////////////////////////////////////////////////////////////////////////

const uint16_t EndpointIp::_defaultPort = 8529;

////////////////////////////////////////////////////////////////////////////////
/// @brief default host if none specified
////////////////////////////////////////////////////////////////////////////////

const std::string EndpointIp::_defaultHost = "127.0.0.1";

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Rest
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an IP socket endpoint
////////////////////////////////////////////////////////////////////////////////

EndpointIp::EndpointIp (const Endpoint::EndpointType type, 
                        const Endpoint::DomainType domainType, 
                        const Endpoint::ProtocolType protocol,
                        const Endpoint::EncryptionType encryption,
                        const std::string& specification, 
                        int listenBacklog,
                        const std::string& host, 
                        const uint16_t port) :
    Endpoint(type, domainType, protocol, encryption, specification, listenBacklog), _host(host), _port(port) {

  assert(domainType == DOMAIN_IPV4 || domainType == Endpoint::DOMAIN_IPV6);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an IPv4 socket endpoint
////////////////////////////////////////////////////////////////////////////////

EndpointIp::~EndpointIp () {
  if (_connected) {
    disconnect();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Rest
/// @{
////////////////////////////////////////////////////////////////////////////////
  
socket_t EndpointIp::connectSocket (const struct addrinfo* aip, double connectTimeout, double requestTimeout) {
  // set address and port
  char host[NI_MAXHOST], serv[NI_MAXSERV];
  if (::getnameinfo(aip->ai_addr, aip->ai_addrlen,
                  host, sizeof(host),
                  serv, sizeof(serv), NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
    
    LOGGER_TRACE << "bind to address '" << string(host) << "' port '" << _port << "'";
  }
  
  socket_t listenSocket = ::socket(aip->ai_family, aip->ai_socktype, aip->ai_protocol);
  if (listenSocket == INVALID_SOCKET) {
    return 0;
  }
  
  // try to reuse address
  int opt = 1;
  if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*> (&opt), sizeof (opt)) == -1) {
    LOGGER_ERROR << "setsockopt failed with " << errno << " (" << strerror(errno) << ")";

    TRI_CLOSE_SOCKET(listenSocket);

    return 0;
  }
  LOGGER_TRACE << "reuse address flag set";
 
  if (_type == ENDPOINT_SERVER) {
    // server needs to bind to socket
    int result = ::bind(listenSocket, aip->ai_addr, aip->ai_addrlen);
    if (result != 0) {
      // error 
      TRI_CLOSE_SOCKET(listenSocket);
      
      return 0;
    }

    // listen for new connection, executed for server endpoints only
    LOGGER_TRACE << "using backlog size " << _listenBacklog;
    result = ::listen(listenSocket, _listenBacklog);

    if (result == SOCKET_ERROR) {
      TRI_CLOSE_SOCKET(listenSocket);
      // todo: get the correct error code using WSAGetLastError for windows
      LOGGER_ERROR << "listen failed with " << errno << " (" << strerror(errno) << ")";

      return 0;
    }
  }
  else if (_type == ENDPOINT_CLIENT) {
    // connect to endpoint, executed for client endpoints only
    
    // set timeout
    setTimeout(listenSocket, connectTimeout);

    int result = ::connect(listenSocket, (const struct sockaddr*) aip->ai_addr, aip->ai_addrlen); 
    if ( result == SOCKET_ERROR) {
      TRI_CLOSE_SOCKET(listenSocket);

      return 0;
    }
  }
    
  if (!setSocketFlags(listenSocket)) { // set some common socket flags for a server
    TRI_CLOSE_SOCKET(listenSocket);

    return 0;
  }
    
  if (_type == ENDPOINT_CLIENT) {
    setTimeout(listenSocket, requestTimeout);
  }
  
  _connected = true;
  _socket = listenSocket;

  return _socket;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Rest
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief connect the endpoint
////////////////////////////////////////////////////////////////////////////////

socket_t EndpointIp::connect (double connectTimeout, double requestTimeout) {
  struct addrinfo* result = 0;
  struct addrinfo* aip;
  struct addrinfo hints;
  int error;
  
  LOGGER_DEBUG << "connecting to ip endpoint " << _specification;
  
  assert(_socket == 0);
  assert(!_connected);
  
  memset(&hints, 0, sizeof (struct addrinfo));
  hints.ai_family = getDomain(); // Allow IPv4 or IPv6
  hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV | AI_ALL;
  hints.ai_socktype = SOCK_STREAM;
  
  string portString = StringUtils::itoa(_port);
  
  error = getaddrinfo(_host.c_str(), portString.c_str(), &hints, &result);
  
  if (error != 0) {
    LOGGER_ERROR << "getaddrinfo for host: " << _host.c_str() << " => " << gai_strerror(error);
 
    if (result != 0) { 
      freeaddrinfo(result);
    }

    return 0;
  }

  socket_t listenSocket = 0;
  
  // Try all returned addresses until one works
  for (aip = result; aip != NULL; aip = aip->ai_next) {
    // try to bind the address info pointer
    listenSocket = connectSocket(aip, connectTimeout, requestTimeout);
    if (listenSocket != 0) {
      // OK
      break;
    }
  }
  
  freeaddrinfo(result);
  
  return listenSocket;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an IPv4 socket endpoint
////////////////////////////////////////////////////////////////////////////////

void EndpointIp::disconnect () {
  if (_connected) {
    assert(_socket);

    _connected = false;
    TRI_CLOSE_SOCKET(_socket);

    _socket = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief init an incoming connection
////////////////////////////////////////////////////////////////////////////////

bool EndpointIp::initIncoming (socket_t incoming) {
  // disable nagle's algorithm
  int n = 1;
  int res = setsockopt(incoming, IPPROTO_TCP, TCP_NODELAY, (char*) &n, sizeof(n));
    
  if (res != 0 ) {
    // todo: get correct windows error code
    LOGGER_WARNING << "setsockopt failed with " << errno << " (" << strerror(errno) << ")";
      
    return false;
  }

  return setSocketFlags(incoming);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
