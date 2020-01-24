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
/// @author Dr. Frank Celler
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_ENDPOINT_CONNECTION_INFO_H
#define ARANGODB_ENDPOINT_CONNECTION_INFO_H 1

#include "Basics/Common.h"
#include "Endpoint/Endpoint.h"

namespace arangodb {

struct ConnectionInfo {
 public:
  ConnectionInfo()
      : serverAddress(),
        clientAddress(),
        endpoint(),
        serverPort(0),
        clientPort(0),
        endpointType(Endpoint::DomainType::UNKNOWN),
        encryptionType(Endpoint::EncryptionType::NONE) {}

 public:
  std::string portType() const {
    switch (endpointType) {
      case Endpoint::DomainType::UNIX:
        return "unix";
      case Endpoint::DomainType::IPV4:
      case Endpoint::DomainType::IPV6:
        return "tcp/ip";
      default:
        return "unknown";
    }
  }

  std::string fullClient() const {
    return clientAddress + ":" + std::to_string(clientPort);
  }

  std::string serverAddress;
  std::string clientAddress;
  std::string endpoint;
  
  int serverPort;
  int clientPort;
  
  Endpoint::DomainType endpointType;
  Endpoint::EncryptionType encryptionType;
};

}  // namespace arangodb

#endif
