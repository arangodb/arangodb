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

#include "EndpointIpV6.h"

#include "Endpoint/Endpoint.h"

using namespace arangodb;

EndpointIpV6::EndpointIpV6(EndpointType type, TransportType transport,
                           EncryptionType encryption, int listenBacklog, bool reuseAddress,
                           std::string const& host, uint16_t const port)
    : EndpointIp(DomainType::IPV6, type, transport, encryption, listenBacklog,
                 reuseAddress, host, port) {}
bool EndpointIpV6::isBroadcastBind() const {
  return host() == "::";
}
