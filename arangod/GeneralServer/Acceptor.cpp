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

#include "GeneralServer/Acceptor.h"

#include "Basics/operating-system.h"
#include "GeneralServer/AcceptorTcp.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

#ifdef ARANGODB_HAVE_DOMAIN_SOCKETS
#include "GeneralServer/AcceptorUnixDomain.h"
#endif

using namespace arangodb;
using namespace arangodb::rest;

Acceptor::Acceptor(rest::GeneralServer& server, rest::IoContext& context, Endpoint* endpoint)
    : _server(server), _ctx(context), _endpoint(endpoint), _open(false), _acceptFailures(0) {}

std::unique_ptr<Acceptor> Acceptor::factory(rest::GeneralServer& server,
                                            rest::IoContext& context, Endpoint* endpoint) {
#ifdef ARANGODB_HAVE_DOMAIN_SOCKETS
  if (endpoint->domainType() == Endpoint::DomainType::UNIX) {
    return std::make_unique<AcceptorUnixDomain>(server, context, endpoint);
  }
#endif
  if (endpoint->encryption() == Endpoint::EncryptionType::SSL) {
    return std::make_unique<AcceptorTcp<rest::SocketType::Ssl>>(server, context, endpoint);
  } else {
    TRI_ASSERT(endpoint->encryption() == Endpoint::EncryptionType::NONE);
    return std::make_unique<AcceptorTcp<rest::SocketType::Tcp>>(server, context, endpoint);
  }
}

void Acceptor::handleError(asio_ns::error_code const& ec) {
  // On shutdown, the _open flag will be reset first and then the
  // acceptor is cancelled and closed, in this case we do not want
  // to start another async_accept. In other cases, we do want to
  // continue after an error.
  if (!_open && ec == asio_ns::error::operation_aborted) {
    // this "error" is accepted, so it doesn't justify a warning
    LOG_TOPIC("74339", DEBUG, arangodb::Logger::COMMUNICATION)
        << "accept failed: " << ec.message();
    return;
  }

  if (++_acceptFailures <= maxAcceptErrors) {
    LOG_TOPIC("644df", WARN, arangodb::Logger::COMMUNICATION)
        << "accept failed: " << ec.message();
    if (_acceptFailures == maxAcceptErrors) {
      LOG_TOPIC("40ca3", WARN, arangodb::Logger::COMMUNICATION)
          << "too many accept failures, stopping to report";
    }
  }
  asyncAccept();  // retry
  return;
}
