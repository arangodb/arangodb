////////////////////////////////////////////////////////////////////////////////
/// @brief connection endpoints
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Endpoint.h"

#include <netinet/in.h>
#include <sys/file.h>

#include "BasicsC/socket-utils.h"

#include "Basics/StringUtils.h"
#include "Basics/FileUtils.h"
#include "Logger/Logger.h"

using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                          Endpoint
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
/// @brief create an endpoint
////////////////////////////////////////////////////////////////////////////////
      
Endpoint::Endpoint (const Endpoint::Type type, 
                    const Endpoint::DomainType domainType, 
                    const Endpoint::Protocol protocol,
                    const Endpoint::Encryption encryption,
                    const string& specification) :
  _connected(false),
  _socket(0),
  _type(type),
  _domainType(domainType),
  _protocol(protocol),
  _encryption(encryption),
  _specification(specification) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy an endpoint
////////////////////////////////////////////////////////////////////////////////
      
Endpoint::~Endpoint () {
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
/// @brief create a client endpoint object from a string value
////////////////////////////////////////////////////////////////////////////////

Endpoint* Endpoint::clientFactory (const string& specification) {
  return Endpoint::factory(ENDPOINT_CLIENT, specification);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a server endpoint object from a string value
////////////////////////////////////////////////////////////////////////////////

Endpoint* Endpoint::serverFactory (const string& specification) {
  return Endpoint::factory(ENDPOINT_SERVER, specification);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an endpoint object from a string value
////////////////////////////////////////////////////////////////////////////////

Endpoint* Endpoint::factory (const Endpoint::Type type, 
                             const string& specification) {
  if (specification.size() < 7) {
    return 0;
  }

  string copy = specification;
  if (specification[specification.size() - 1] == '/') {
    // address ends with a slash => remove
    copy = copy.substr(0, copy.size() - 1);
  }

  // default protocol is HTTP
  Endpoint::Protocol protocol = PROTOCOL_HTTP;

  // read protocol from string
  size_t found = copy.find('@');
  if (found != string::npos) {
    string protoString = StringUtils::tolower(copy.substr(0, found));
    if (protoString == "binary") {
      protocol = PROTOCOL_BINARY;
      copy = copy.substr(strlen("binary@"));
    }
    else if (protoString == "http") {
      copy = copy.substr(strlen("http@"));
    }
    else {
      // invalid protocol
      return 0;
    }
  }
  
  Encryption encryption = ENCRYPTION_NONE;
  string domainType = StringUtils::tolower(copy.substr(0, 7));
  if (StringUtils::isPrefix(domainType, "unix://")) {
    // unix socket
    return new EndpointUnix(type, protocol, specification, copy.substr(strlen("unix://")));
  }
  else if (StringUtils::isPrefix(domainType, "ssl://")) {
    // ssl 
    encryption = ENCRYPTION_SSL;
  }
  else if (! StringUtils::isPrefix(domainType, "tcp://")) {
    // invalid type
    return 0;
  }

  // tcp/ip or ssl
  copy = copy.substr(strlen("tcp://"), copy.length());
  
  if (copy[0] == '[') {
    // ipv6
    found = copy.find("]:", 1);
    if (found != string::npos && found + 2 < copy.size()) {
      // hostname and port (e.g. [address]:port)
      uint16_t port = (uint16_t) StringUtils::uint32(copy.substr(found + 2));

      return new EndpointIpV6(type, protocol, encryption, specification, copy.substr(0, found + 1), port);
    }

    found = copy.find("]", 1);
    if (found != string::npos && found + 1 == copy.size()) {
      // hostname only (e.g. [address])

      return new EndpointIpV6(type, protocol, encryption, specification, copy.substr(0, found + 1), EndpointIp::_defaultPort);
    }

    // invalid address specification
    return 0;
  }

  // ipv4
  found = copy.find(':');

  if (found != string::npos && found + 1 < copy.size()) {
    // hostname and port
    uint16_t port = (uint16_t) StringUtils::uint32(copy.substr(found + 1));

    return new EndpointIpV4(type, protocol, encryption, specification, copy.substr(0, found), port);
  }

  // hostname only
  return new EndpointIpV4(type, protocol, encryption, specification, copy, EndpointIp::_defaultPort);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two endpoints
////////////////////////////////////////////////////////////////////////////////

bool Endpoint::operator== (Endpoint const &that) const {
  return getSpecification() == that.getSpecification();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the default endpoint
////////////////////////////////////////////////////////////////////////////////

const std::string Endpoint::getDefaultEndpoint () {   
   return "tcp://" + EndpointIp::_defaultHost + ":" + StringUtils::itoa(EndpointIp::_defaultPort);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set socket timeout
////////////////////////////////////////////////////////////////////////////////
  
void Endpoint::setTimeout (socket_t s, double timeout) {
  struct timeval tv;
  tv.tv_sec = (uint64_t) timeout;
  tv.tv_usec = ((uint64_t) (timeout * 1000000.0)) % 1000000;

  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set common socket flags
////////////////////////////////////////////////////////////////////////////////
  
bool Endpoint::setSocketFlags (socket_t _socket) {
  if (_encryption == ENCRYPTION_SSL && _type == ENDPOINT_CLIENT) {
    // SSL client endpoints are not set to non-blocking
    return true;
  }

  // set to non-blocking, executed for both client and server endpoints
  bool ok = TRI_SetNonBlockingSocket(_socket);
  if (!ok) {
    LOGGER_ERROR << "cannot switch to non-blocking: " << errno << " (" << strerror(errno) << ")";

    return false;
  }
  
  // set close-on-exec flag, executed for both client and server endpoints
  ok = TRI_SetCloseOnExecSocket(_socket);
  if (!ok) {
    LOGGER_ERROR << "cannot set close-on-exit: " << errno << " (" << strerror(errno) << ")";

    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_LINUX_SOCKETS

// -----------------------------------------------------------------------------
// --SECTION--                                                      EndpointUnix
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Rest
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a Unix socket endpoint
////////////////////////////////////////////////////////////////////////////////

EndpointUnix::EndpointUnix (const Endpoint::Type type, 
                            const Endpoint::Protocol protocol,
                            string const& specification, 
                            string const& path) :
    Endpoint(type, ENDPOINT_UNIX, protocol, ENCRYPTION_NONE, specification),
    _path(path) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a Unix socket endpoint
////////////////////////////////////////////////////////////////////////////////

EndpointUnix::~EndpointUnix () {
  if (_connected) {
    disconnect();
  }
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

socket_t EndpointUnix::connect (double connectTimeout, double requestTimeout) {
  assert(_socket == 0);
  assert(!_connected);

  if (_type == ENDPOINT_SERVER && FileUtils::exists(_path)) {
    // socket file already exists
    LOGGER_WARNING << "socket file '" << _path << "' already exists.";

    int error = 0;
    // delete previously existing socket file
    if (FileUtils::remove(_path, &error)) {
      LOGGER_WARNING << "deleted previously existing socket file '" << _path << "'.";
    }
    else {
      LOGGER_ERROR << "unable to delete previously existing socket file '" << _path << "'.";
    
      return 0;
    }
  }

  int listenSocket = socket(AF_UNIX, SOCK_STREAM, 0);
  if (listenSocket == -1) {
    return 0;
  }
  
  // reuse address
  int opt = 1;
    
  if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*> (&opt), sizeof (opt)) == -1) {
    LOGGER_ERROR << "setsockopt failed with " << errno << " (" << strerror(errno) << ")";

    close(listenSocket);

    return 0;
  }
  LOGGER_TRACE << "reuse address flag set";
    
  struct sockaddr_un address;

  memset(&address, 0, sizeof(address));
  address.sun_family = AF_UNIX;
  snprintf(address.sun_path, 100, "%s", _path.c_str());

  if (_type == ENDPOINT_SERVER) {
    int result = bind(listenSocket, (struct sockaddr*) &address, SUN_LEN(&address));
    if (result != 0) {
      // bind error
      close(listenSocket);

      return 0;
    }
    
    // listen for new connection, executed for server endpoints only
    result = listen(listenSocket, 10);

    if (result < 0) {
      close(listenSocket);

      LOGGER_ERROR << "listen failed with " << errno << " (" << strerror(errno) << ")";

      return 0;
    }
  }
  else if (_type == ENDPOINT_CLIENT) {
    // connect to endpoint, executed for client endpoints only

    // set timeout
    setTimeout(listenSocket, connectTimeout);

    if (::connect(listenSocket, (const struct sockaddr*) &address, SUN_LEN(&address)) != 0) {
      close(listenSocket);

      return 0;
    }
  }
    
  if (!setSocketFlags(listenSocket)) {
    close(listenSocket);

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
/// @brief disconnect the endpoint
////////////////////////////////////////////////////////////////////////////////

void EndpointUnix::disconnect () {
  if (_connected) {
    assert(_socket);

    _connected = false;
    close(_socket);

    _socket = 0;

    if (_type == ENDPOINT_SERVER) {   
      int error = 0; 
      FileUtils::remove(_path, &error);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief init an incoming connection
////////////////////////////////////////////////////////////////////////////////

bool EndpointUnix::initIncoming (socket_t incoming) {
  return setSocketFlags(incoming);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                        EndpointIp
// -----------------------------------------------------------------------------

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

EndpointIp::EndpointIp (const Endpoint::Type type, 
                        const Endpoint::DomainType domainType, 
                        const Endpoint::Protocol protocol,
                        const Endpoint::Encryption encryption,
                        string const& specification, 
                        string const& host, 
                        const uint16_t port) :
    Endpoint(type, domainType, protocol, encryption, specification), _host(host), _port(port) {
  
  assert(domainType == ENDPOINT_IPV4 || domainType == ENDPOINT_IPV6);
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
  if (getnameinfo(aip->ai_addr, aip->ai_addrlen,
                  host, sizeof(host),
                  serv, sizeof(serv), NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
    
    LOGGER_TRACE << "bind to address '" << string(host) << "' port '" << _port << "'";
  }
  
  int listenSocket = socket(aip->ai_family, aip->ai_socktype, aip->ai_protocol);
  if (listenSocket == -1) {
    return 0;
  }
  
  // try to reuse address
  int opt = 1;
  if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*> (&opt), sizeof (opt)) == -1) {
    LOGGER_ERROR << "setsockopt failed with " << errno << " (" << strerror(errno) << ")";

    close(listenSocket);

    return 0;
  }
  LOGGER_TRACE << "reuse address flag set";
 
  if (_type == ENDPOINT_SERVER) {
    // server needs to bind to socket
    int result = bind(listenSocket, aip->ai_addr, aip->ai_addrlen);
    if (result != 0) {
      // error 
      close(listenSocket);

      return 0;
    }
    // listen for new connection, executed for server endpoints only
    result = listen(listenSocket, 10);
    if (result < 0) {
      close(listenSocket);

      LOGGER_ERROR << "listen failed with " << errno << " (" << strerror(errno) << ")";

      return 0;
    }
  }
  else if (_type == ENDPOINT_CLIENT) {
    // connect to endpoint, executed for client endpoints only
    
    // set timeout
    setTimeout(listenSocket, connectTimeout);

    if (::connect(listenSocket, (const struct sockaddr*) aip->ai_addr, aip->ai_addrlen) != 0) {
      close(listenSocket);

      return 0;
    }
  }
    
  if (!setSocketFlags(listenSocket)) {
    close(listenSocket);

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

  int listenSocket = 0;
  
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
    close(_socket);

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
    LOGGER_WARNING << "setsockopt failed with " << errno << " (" << strerror(errno) << ")";
      
    return false;
  }

  return setSocketFlags(incoming);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      EndpointIpV4
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Rest
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an IPv4 socket endpoint
////////////////////////////////////////////////////////////////////////////////

EndpointIpV4::EndpointIpV4 (const Endpoint::Type type,
                            const Endpoint::Protocol protocol,
                            const Endpoint::Encryption encryption,
                            string const& specification, 
                            string const& host, 
                            const uint16_t port) :
    EndpointIp(type, ENDPOINT_IPV4, protocol, encryption, specification, host, port) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an IPv4 socket endpoint
////////////////////////////////////////////////////////////////////////////////

EndpointIpV4::~EndpointIpV4 () {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                         EndpointIpV6
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Rest
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an IPv6 socket endpoint
////////////////////////////////////////////////////////////////////////////////

EndpointIpV6::EndpointIpV6 (const Endpoint::Type type,
                            const Endpoint::Protocol protocol, 
                            const Endpoint::Encryption encryption,
                            string const& specification, 
                            string const& host, 
                            const uint16_t port) :
    EndpointIp(type, ENDPOINT_IPV6, protocol, encryption, specification, host, port) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an IPv6 socket endpoint
////////////////////////////////////////////////////////////////////////////////

EndpointIpV6::~EndpointIpV6 () {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
