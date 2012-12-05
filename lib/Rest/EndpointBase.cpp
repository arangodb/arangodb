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


#include "BasicsC/socket-utils.h"
#include "Basics/StringUtils.h"
#include "Basics/FileUtils.h"
#include "Logger/Logger.h"

#include "EndpointUnixDomain.h"
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
/// @brief create an abstract endpoint
////////////////////////////////////////////////////////////////////////////////
      
EndpointBase::EndpointBase (const EndpointBase::EndPointType endpointType, 
                            const EndpointBase::DomainType domainType, 
                            const EndpointBase::ProtocolType protocolType, 
                            const EndpointBase::EncryptionType encryptionType,
                            const std::string& specification) :
  _connected(false),
  _socket(0),
  _endpointType(endpointType),
  _domainType(domainType),
  _protocol(protocolType),
  _encryption(encryptionType),
  _specification(specification) {
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

EndpointBase* EndpointBase::clientFactory (const std::string& specification) {
  return EndpointBase::factory(ENDPOINT_CLIENT, specification);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a server endpoint object from a string value
////////////////////////////////////////////////////////////////////////////////

EndpointBase* EndpointBase::serverFactory (const std::string& specification) {
  return EndpointBase::factory(ENDPOINT_SERVER, specification);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an endpoint object from a string value
////////////////////////////////////////////////////////////////////////////////

EndpointBase* EndpointBase::factory (const EndpointBase::EndpointType endpointType, 
                                    const std::string& specification) {
  if (specification.size() < 7) {
    return 0;
  }

  string copy = specification;
  if (specification[specification.size() - 1] == '/') {
    // address ends with a slash => remove
    copy = copy.substr(0, copy.size() - 1);
  }

  // default protocol is HTTP
  EndpointBase::ProtocolType protocol = PROTOCOL_HTTP;

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
  
  EndpointBase::EncryptionType encryption = ENCRYPTION_NONE;
  
  
  string domainType = StringUtils::tolower(copy.substr(0, 7));
  if (StringUtils::isPrefix(domainType, "unix://")) {
    // unix socket
    return new EndpointUnix(endpointType, protocol, specification, copy.substr(strlen("unix://")));
  }
  else if (StringUtils::isPrefix(domainType, "ssl://")) {
    // ssl 
    encryption = ENCRYPTION_SSL;
  }
  else if (! StringUtils::isPrefix(domainType, "tcp://")) {
    // invalid domain type
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

      return new EndpointIpV6(endpointType, protocol, encryption, specification, copy.substr(0, found + 1), port);
    }

    found = copy.find("]", 1);
    if (found != string::npos && found + 1 == copy.size()) {
      // hostname only (e.g. [address])

      return new EndpointIpV6(endpointType, protocol, encryption, specification, copy.substr(0, found + 1), EndpointIp::_defaultPort);
    }

    // invalid address specification
    return 0;
  }

  // ipv4
  found = copy.find(':');

  if (found != string::npos && found + 1 < copy.size()) {
    // hostname and port
    uint16_t port = (uint16_t) StringUtils::uint32(copy.substr(found + 1));

    return new EndpointIpV4(endpointType, protocol, encryption, specification, copy.substr(0, found), port);
  }

  // hostname only
  return new EndpointIpV4(endpointType, protocol, encryption, specification, copy, EndpointIp::_defaultPort);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two endpoints
////////////////////////////////////////////////////////////////////////////////

bool EndpointBase::operator== (const EndpointBase& that) const {
  return getSpecification() == that.getSpecification();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the default endpoint
////////////////////////////////////////////////////////////////////////////////

const std::string EndpointBase::getDefaultEndpoint () {   
   return "tcp://" + EndpointIp::_defaultHost + ":" + StringUtils::itoa(EndpointIp::_defaultPort);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set socket timeout
////////////////////////////////////////////////////////////////////////////////
  
void EndpointBase::setTimeout (socket_t s, double timeout) {
  struct timeval tv;
  tv.tv_sec = (uint64_t) timeout;
  tv.tv_usec = ((uint64_t) (timeout * 1000000.0)) % 1000000;

  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set common socket flags
////////////////////////////////////////////////////////////////////////////////
  
bool EndpointBase::setSocketFlags (socket_t _socket) {
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

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
