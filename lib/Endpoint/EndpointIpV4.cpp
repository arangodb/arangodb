////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "EndpointIpV4.h"

#include "Logger/Logger.h"

#include "Endpoint/Endpoint.h"

using namespace arangodb;

EndpointIpV4::EndpointIpV4(EndpointType type, EncryptionType encryption,
                           int listenBacklog, bool reuseAddress,
                           std::string const& host, uint16_t const port)
    : EndpointIp(DomainType::IPV4, type, encryption, listenBacklog,
                 reuseAddress, host, port) {}

bool EndpointIpV4::isBroadcastBind() const { return host() == "0.0.0.0"; }
