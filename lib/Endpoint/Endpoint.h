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

#ifndef ARANGODB_ENDPOINT_ENDPOINT_H
#define ARANGODB_ENDPOINT_ENDPOINT_H 1

#include "Basics/Common.h"

#include "Basics/socket-utils.h"

#ifdef TRI_HAVE_WINSOCK2_H
#include <WinSock2.h>
#include <WS2tcpip.h>
#endif

#include <ostream>

namespace arangodb {

class Endpoint {
 public:
  enum class TransportType { HTTP, VPP };
  enum class EndpointType { SERVER, CLIENT };
  enum class EncryptionType { NONE = 0, SSL };
  enum class DomainType { UNKNOWN = 0, UNIX, IPV4, IPV6, SRV };

 protected:
  Endpoint(DomainType, EndpointType, TransportType, EncryptionType,
           std::string const&, int);

 public:
  virtual ~Endpoint() {}

 public:
  static std::string uriForm(std::string const&);
  static std::string unifiedForm(std::string const&);
  static Endpoint* serverFactory(std::string const&, int, bool reuseAddress);
  static Endpoint* clientFactory(std::string const&);
  static Endpoint* factory(const EndpointType type, std::string const&, int,
                           bool);
  static std::string const defaultEndpoint(TransportType);

 public:
  bool operator==(Endpoint const&) const;
  TransportType transport() const { return _transport; }
  EndpointType type() const { return _type; }
  EncryptionType encryption() const { return _encryption; }
  std::string specification() const { return _specification; }

 public:
  virtual TRI_socket_t connect(double, double) = 0;
  virtual void disconnect() = 0;

  virtual bool initIncoming(TRI_socket_t) = 0;
  virtual bool setTimeout(TRI_socket_t, double);
  virtual bool isConnected() const { return _connected; }
  virtual bool setSocketFlags(TRI_socket_t);
  virtual DomainType domainType() const { return _domainType; }

  virtual int domain() const = 0;
  virtual int port() const = 0;
  virtual std::string host() const = 0;
  virtual std::string hostAndPort() const = 0;

 public:
  std::string _errorMessage;

 protected:
  DomainType _domainType;
  EndpointType _type;
  TransportType _transport;
  EncryptionType _encryption;
  std::string _specification;
  int _listenBacklog;

  bool _connected;
  TRI_socket_t _socket;
};
}

std::ostream& operator<<(std::ostream&, arangodb::Endpoint::TransportType);
std::ostream& operator<<(std::ostream&, arangodb::Endpoint::EndpointType);
std::ostream& operator<<(std::ostream&, arangodb::Endpoint::EncryptionType);
std::ostream& operator<<(std::ostream&, arangodb::Endpoint::DomainType);

#endif
