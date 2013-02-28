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

#include "BasicsC/socket-utils.h"
#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Logger/Logger.h"

#if TRI_HAVE_LINUX_SOCKETS  
#include "Rest/EndpointUnixDomain.h"
#endif
#include "Rest/EndpointIpV4.h"
#include "Rest/EndpointIpV6.h"

using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                          Endpoint
// -----------------------------------------------------------------------------

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
      
Endpoint::Endpoint (const Endpoint::EndpointType type, 
                    const Endpoint::DomainType domainType, 
                    const Endpoint::ProtocolType protocol,
                    const Endpoint::EncryptionType encryption,
                    const std::string& specification,
                    int listenBacklog) :
  _connected(false),
  _type(type),
  _domainType(domainType),
  _protocol(protocol),
  _encryption(encryption),
  _specification(specification),
  _listenBacklog(listenBacklog) {
  _socket.fileHandle = 0;
  _socket.fileDescriptor = 0;
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

Endpoint* Endpoint::clientFactory (const std::string& specification) {
  return Endpoint::factory(ENDPOINT_CLIENT, specification, 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a server endpoint object from a string value
////////////////////////////////////////////////////////////////////////////////

Endpoint* Endpoint::serverFactory (const std::string& specification, int listenBacklog) {
  return Endpoint::factory(ENDPOINT_SERVER, specification, listenBacklog);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an endpoint object from a string value
////////////////////////////////////////////////////////////////////////////////

Endpoint* Endpoint::factory (const Endpoint::EndpointType type, 
                             const std::string& specification,
                             int listenBacklog) {
  if (specification.size() < 7) {
    return 0;
  }

  if (listenBacklog > 0 && type == ENDPOINT_CLIENT) {
    // backlog is only allowed for server endpoints
    assert(false);
  }

  string copy = specification;
  if (specification[specification.size() - 1] == '/') {
    // address ends with a slash => remove
    copy = copy.substr(0, copy.size() - 1);
  }

  // default protocol is HTTP
  Endpoint::ProtocolType protocol = PROTOCOL_HTTP;

  // read protocol from string
  size_t found = copy.find('@');
  if (found != string::npos) {
    string protoString = StringUtils::tolower(copy.substr(0, found));
    if (protoString == "http") {
      copy = copy.substr(strlen("http@"));
    }
    else {
      // invalid protocol
      return 0;
    }
  }
  
  EncryptionType encryption = ENCRYPTION_NONE;
  string domainType = StringUtils::tolower(copy.substr(0, 7));


  if (StringUtils::isPrefix(domainType, "ssl://")) {
    // ssl 
    encryption = ENCRYPTION_SSL;
  }

#if TRI_HAVE_LINUX_SOCKETS  
  else if (StringUtils::isPrefix(domainType, "unix://")) {
    // unix socket
    return new EndpointUnixDomain(type, protocol, specification, listenBacklog, copy.substr(strlen("unix://")));
  }
#else
    // no unix socket for windows
  else if (StringUtils::isPrefix(domainType, "unix://")) {
    // unix socket
    return 0;
  }
#endif

  else if (! StringUtils::isPrefix(domainType, "tcp://")) {
    // invalid type
    return 0;
  }

  // tcp/ip or ssl
  copy = copy.substr(strlen("tcp://"), copy.length());
  
  if (copy[0] == '[') {
    // ipv6
    found = copy.find("]:", 1);    
    if (found != string::npos && found > 2 && found + 2 < copy.size()) {
      // hostname and port (e.g. [address]:port)
      uint16_t port = (uint16_t) StringUtils::uint32(copy.substr(found + 2));

      return new EndpointIpV6(type, protocol, encryption, specification, listenBacklog, copy.substr(1, found - 1), port);
    }

    found = copy.find("]", 1);
    if (found != string::npos && found > 2 && found + 1 == copy.size()) {
      // hostname only (e.g. [address])

      return new EndpointIpV6(type, protocol, encryption, specification, listenBacklog, copy.substr(1, found - 1), EndpointIp::_defaultPort);
    }

    // invalid address specification
    return 0;
  }

  // ipv4
  found = copy.find(':');

  if (found != string::npos && found + 1 < copy.size()) {
    // hostname and port
    uint16_t port = (uint16_t) StringUtils::uint32(copy.substr(found + 1));

    return new EndpointIpV4(type, protocol, encryption, specification, listenBacklog, copy.substr(0, found), port);
  }

  // hostname only
  return new EndpointIpV4(type, protocol, encryption, specification, listenBacklog, copy, EndpointIp::_defaultPort);
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
  
void Endpoint::setTimeout (TRI_socket_t s, double timeout) {
  struct timeval tv;

  // shut up Valgrind
  memset(&tv, 0, sizeof(tv));
  tv.tv_sec = (time_t) timeout;
  tv.tv_usec = ((suseconds_t) (timeout * 1000000.0)) % 1000000;

  // conversion to (const char*) ensures windows does not complain
  setsockopt(s.fileHandle, SOL_SOCKET, SO_RCVTIMEO, (const char*)(&tv), sizeof(tv)); 
  setsockopt(s.fileHandle, SOL_SOCKET, SO_SNDTIMEO, (const char*)(&tv), sizeof(tv));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set common socket flags
////////////////////////////////////////////////////////////////////////////////
  
bool Endpoint::setSocketFlags (TRI_socket_t s) {
  if (_encryption == ENCRYPTION_SSL && _type == ENDPOINT_CLIENT) {
    // SSL client endpoints are not set to non-blocking
    return true;
  }

  // set to non-blocking, executed for both client and server endpoints
  bool ok = TRI_SetNonBlockingSocket(s);
  if (!ok) {
    LOGGER_ERROR << "cannot switch to non-blocking: " << errno << " (" << strerror(errno) << ")";

    return false;
  }
  
  // set close-on-exec flag, executed for both client and server endpoints
  ok = TRI_SetCloseOnExitSocket(s);
  if (!ok) {
    LOGGER_ERROR << "cannot set close-on-exit: " << errno << " (" << strerror(errno) << ")";

    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
