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

#include "EndpointIpV4.h"

#include "Logger/Logger.h"

#include "Endpoint/Endpoint.h"

using namespace arangodb;

EndpointIpV4::EndpointIpV4(EndpointType type, TransportType transport,
                           EncryptionType encryption, int listenBacklog,
                           bool reuseAddress, std::string const& host,
                           uint16_t const port)
    : EndpointIp(DomainType::IPV4, type, transport, encryption, listenBacklog,
                 reuseAddress, host, port) {}

void EndpointIpV4::openAcceptor(boost::asio::io_service* ioService,
                                boost::asio::ip::tcp::acceptor* acceptor) {
  boost::asio::ip::tcp::resolver resolver(*ioService);

  auto hostname = host();
  auto portNumber = port();

  boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(),
                                          portNumber);

  if (hostname != "0.0.0.0") {
    boost::asio::ip::tcp::resolver::query query(
        boost::asio::ip::tcp::v4(), hostname, std::to_string(portNumber));
    boost::asio::ip::tcp::resolver::iterator iter = resolver.resolve(query);

    endpoint = iter->endpoint();
  }

  acceptor->open(endpoint.protocol());

  acceptor->set_option(
      boost::asio::ip::tcp::acceptor::reuse_address(reuseAddress()));

  acceptor->bind(endpoint);
  acceptor->listen();
}
