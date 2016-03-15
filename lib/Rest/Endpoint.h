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

#ifndef LIB_REST_ENDPOINT_H
#define LIB_REST_ENDPOINT_H 1

#include "Basics/socket-utils.h"
#include "Basics/Common.h"

#ifdef TRI_HAVE_LINUX_SOCKETS
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/file.h>
#endif

#ifdef TRI_HAVE_WINSOCK2_H
#include <WinSock2.h>
#include <WS2tcpip.h>
#endif

namespace arangodb {
namespace rest {
class Endpoint {
 public:
  enum EndpointType { ENDPOINT_SERVER, ENDPOINT_CLIENT };

  enum DomainType {
    DOMAIN_UNKNOWN = 0,
    DOMAIN_UNIX,
    DOMAIN_IPV4,
    DOMAIN_IPV6,
    DOMAIN_SRV
  };

  enum EncryptionType { ENCRYPTION_NONE = 0, ENCRYPTION_SSL };

 protected:
  Endpoint(EndpointType, DomainType, EncryptionType, std::string const&, int);

 public:
  virtual ~Endpoint();

 public:
  static std::string getUnifiedForm(std::string const&);
  static Endpoint* serverFactory(std::string const&, int, bool reuseAddress);
  static Endpoint* clientFactory(std::string const&);
  static Endpoint* factory(const EndpointType type, std::string const&, int,
                           bool);
  static std::string const getDefaultEndpoint();

 public:
  bool operator==(Endpoint const&) const;
  EndpointType getType() const { return _type; }
  EncryptionType getEncryption() const { return _encryption; }
  std::string getSpecification() const { return _specification; }

 public:
  virtual TRI_socket_t connect(double, double) = 0;
  virtual void disconnect() = 0;

  virtual bool initIncoming(TRI_socket_t) = 0;
  virtual bool setTimeout(TRI_socket_t, double);
  virtual bool isConnected() const { return _connected; }
  virtual bool setSocketFlags(TRI_socket_t);
  virtual DomainType getDomainType() const { return _domainType; }
  virtual int getDomain() const = 0;
  virtual int getPort() const = 0;
  virtual std::string getHost() const = 0;
  virtual std::string getHostString() const = 0;

 public:
  std::string _errorMessage;

 protected:
  bool _connected;
  TRI_socket_t _socket;
  EndpointType _type;
  DomainType _domainType;
  EncryptionType _encryption;
  std::string _specification;
  int _listenBacklog;
};
}
}

#endif
