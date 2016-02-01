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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Endpoint.h"

#include "Basics/Logger.h"
#include "Basics/socket-utils.h"
#include "Basics/StringUtils.h"

#if TRI_HAVE_LINUX_SOCKETS
#include "Rest/EndpointUnixDomain.h"
#endif
#include "Rest/EndpointIpV4.h"
#include "Rest/EndpointIpV6.h"

using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief create an endpoint
////////////////////////////////////////////////////////////////////////////////

Endpoint::Endpoint(Endpoint::EndpointType type,
                   Endpoint::DomainType domainType,
                   Endpoint::EncryptionType encryption,
                   std::string const& specification, int listenBacklog)
    : _connected(false),
      _type(type),
      _domainType(domainType),
      _encryption(encryption),
      _specification(specification),
      _listenBacklog(listenBacklog) {
  TRI_invalidatesocket(&_socket);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy an endpoint
////////////////////////////////////////////////////////////////////////////////

Endpoint::~Endpoint() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the endpoint specification in a unified form
////////////////////////////////////////////////////////////////////////////////

std::string Endpoint::getUnifiedForm(std::string const& specification) {
  if (specification.size() < 7) {
    return "";
  }

  std::string copy = specification;
  StringUtils::trimInPlace(copy);
  copy = StringUtils::tolower(copy);

  if (specification[specification.size() - 1] == '/') {
    // address ends with a slash => remove
    copy = copy.substr(0, copy.size() - 1);
  }

  // read protocol from string
  if (StringUtils::isPrefix(copy, "http@")) {
    copy = copy.substr(5);
  }

#if TRI_HAVE_LINUX_SOCKETS
  if (StringUtils::isPrefix(copy, "unix://")) {
    // unix socket
    return copy;
  }
#else
  // no unix socket for windows
  if (StringUtils::isPrefix(copy, "unix://")) {
    // unix socket
    return "";
  }
#endif
  else if (!StringUtils::isPrefix(copy, "ssl://") &&
           !StringUtils::isPrefix(copy, "tcp://")) {
    // invalid type
    return "";
  }

  size_t found;
  /*
  // turn "localhost" into "127.0.0.1"
  // technically this is not always correct, but circumvents obvious problems
  // when the configuration contains both "127.0.0.1" and "localhost" endpoints
  found = copy.find("localhost");
  if (found != string::npos && found >= 6 && found < 10) {
    copy = copy.replace(found, strlen("localhost"), "127.0.0.1");
  }
  */

  // tcp/ip or ssl
  std::string temp = copy.substr(6, copy.length());  // strip tcp:// or ssl://
  if (temp[0] == '[') {
    // ipv6
    found = temp.find("]:", 1);
    if (found != std::string::npos && found > 2 && found + 2 < temp.size()) {
      // hostname and port (e.g. [address]:port)
      return copy;
    }

    found = temp.find("]", 1);
    if (found != std::string::npos && found > 2 && found + 1 == temp.size()) {
      // hostname only (e.g. [address])
      return copy + ":" + StringUtils::itoa(EndpointIp::_defaultPort);
    }

    // invalid address specification
    return "";
  }

  // ipv4
  found = temp.find(':');

  if (found != std::string::npos && found + 1 < temp.size()) {
    // hostname and port
    return copy;
  }

  // hostname only
  return copy + ":" + StringUtils::itoa(EndpointIp::_defaultPort);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a client endpoint object from a string value
////////////////////////////////////////////////////////////////////////////////

Endpoint* Endpoint::clientFactory(std::string const& specification) {
  return Endpoint::factory(ENDPOINT_CLIENT, specification, 0, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a server endpoint object from a string value
////////////////////////////////////////////////////////////////////////////////

Endpoint* Endpoint::serverFactory(std::string const& specification,
                                  int listenBacklog, bool reuseAddress) {
  return Endpoint::factory(ENDPOINT_SERVER, specification, listenBacklog,
                           reuseAddress);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an endpoint object from a string value
////////////////////////////////////////////////////////////////////////////////

Endpoint* Endpoint::factory(const Endpoint::EndpointType type,
                            std::string const& specification, int listenBacklog,
                            bool reuseAddress) {
  if (specification.size() < 7) {
    return nullptr;
  }

  if (listenBacklog > 0 && type == ENDPOINT_CLIENT) {
    // backlog is only allowed for server endpoints
    TRI_ASSERT(false);
  }

  if (listenBacklog == 0 && type == ENDPOINT_SERVER) {
    // use some default value
    listenBacklog = 10;
  }

  std::string copy = specification;
  if (specification[specification.size() - 1] == '/') {
    // address ends with a slash => remove
    copy = copy.substr(0, copy.size() - 1);
  }

  // read protocol from string
  size_t found = copy.find('@');
  if (found != std::string::npos) {
    std::string protoString = StringUtils::tolower(copy.substr(0, found));
    if (protoString == "http") {
      copy = copy.substr(strlen("http@"));
    } else {
      // invalid protocol
      return nullptr;
    }
  }

  EncryptionType encryption = ENCRYPTION_NONE;
  std::string domainType = StringUtils::tolower(copy.substr(0, 7));

  if (StringUtils::isPrefix(domainType, "ssl://")) {
    // ssl
    encryption = ENCRYPTION_SSL;
  }
#if TRI_HAVE_LINUX_SOCKETS
  else if (StringUtils::isPrefix(domainType, "unix://")) {
    // unix socket
    return new EndpointUnixDomain(type, specification, listenBacklog,
                                  copy.substr(strlen("unix://")));
  }
#else
  // no unix socket for windows
  else if (StringUtils::isPrefix(domainType, "unix://")) {
    // unix socket
    return nullptr;
  }
#endif

  else if (!StringUtils::isPrefix(domainType, "tcp://")) {
    // invalid type
    return nullptr;
  }

  // tcp/ip or ssl
  copy = copy.substr(strlen("tcp://"), copy.length());

  if (copy[0] == '[') {
    // ipv6
    found = copy.find("]:", 1);
    if (found != std::string::npos && found > 2 && found + 2 < copy.size()) {
      // hostname and port (e.g. [address]:port)
      uint16_t port = (uint16_t)StringUtils::uint32(copy.substr(found + 2));
      std::string portStr = copy.substr(1, found - 1);
      return new EndpointIpV6(type, encryption, specification, listenBacklog,
                              reuseAddress, portStr, port);
    }

    found = copy.find("]", 1);
    if (found != std::string::npos && found > 2 && found + 1 == copy.size()) {
      // hostname only (e.g. [address])
      std::string portStr = copy.substr(1, found - 1);
      return new EndpointIpV6(type, encryption, specification, listenBacklog,
                              reuseAddress, portStr, EndpointIp::_defaultPort);
    }

    // invalid address specification
    return nullptr;
  }

  // ipv4
  found = copy.find(':');

  if (found != std::string::npos && found + 1 < copy.size()) {
    // hostname and port
    uint16_t port = (uint16_t)StringUtils::uint32(copy.substr(found + 1));
    std::string portStr = copy.substr(0, found);
    return new EndpointIpV4(type, encryption, specification, listenBacklog,
                            reuseAddress, portStr, port);
  }

  // hostname only
  return new EndpointIpV4(type, encryption, specification, listenBacklog,
                          reuseAddress, copy, EndpointIp::_defaultPort);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two endpoints
////////////////////////////////////////////////////////////////////////////////

bool Endpoint::operator==(Endpoint const& that) const {
  return getSpecification() == that.getSpecification();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the default endpoint
////////////////////////////////////////////////////////////////////////////////

std::string const Endpoint::getDefaultEndpoint() {
  return "tcp://" + EndpointIp::_defaultHost + ":" +
         StringUtils::itoa(EndpointIp::_defaultPort);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set socket timeout
////////////////////////////////////////////////////////////////////////////////

bool Endpoint::setTimeout(TRI_socket_t s, double timeout) {
  return TRI_setsockopttimeout(s, timeout);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set common socket flags
////////////////////////////////////////////////////////////////////////////////

bool Endpoint::setSocketFlags(TRI_socket_t s) {
  if (_encryption == ENCRYPTION_SSL && _type == ENDPOINT_CLIENT) {
    // SSL client endpoints are not set to non-blocking
    return true;
  }

  // set to non-blocking, executed for both client and server endpoints
  bool ok = TRI_SetNonBlockingSocket(s);

  if (!ok) {
    LOG(ERR) << "cannot switch to non-blocking: " << errno << " (" << strerror(errno) << ")";

    return false;
  }

  // set close-on-exec flag, executed for both client and server endpoints
  ok = TRI_SetCloseOnExecSocket(s);

  if (!ok) {
    LOG(ERR) << "cannot set close-on-exit: " << errno << " (" << strerror(errno) << ")";

    return false;
  }

  return true;
}
