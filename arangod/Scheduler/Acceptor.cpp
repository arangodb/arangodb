////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#include "Scheduler/Acceptor.h"

#include "Basics/operating-system.h"
#include "Scheduler/AcceptorTcp.h"

#ifdef ARANGODB_HAVE_DOMAIN_SOCKETS
#include "Scheduler/AcceptorUnixDomain.h"
#endif

using namespace arangodb;

Acceptor::Acceptor(rest::GeneralServer& server,
                   rest::GeneralServer::IoContext& context, Endpoint* endpoint)
    : _server(server), _context(context), _endpoint(endpoint) {}

std::unique_ptr<Acceptor> Acceptor::factory(rest::GeneralServer& server,
                                            rest::GeneralServer::IoContext& context,
                                            Endpoint* endpoint) {
#ifdef ARANGODB_HAVE_DOMAIN_SOCKETS
  if (endpoint->domainType() == Endpoint::DomainType::UNIX) {
    return std::make_unique<AcceptorUnixDomain>(server, context, endpoint);
  }
#endif
  return std::make_unique<AcceptorTcp>(server, context, endpoint);
}
