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

#include "Scheduler/AcceptorTcp.h"

#include "Basics/Common.h"
#include "Endpoint/EndpointIp.h"
#include "Scheduler/SocketTcp.h"

using namespace arangodb;

void AcceptorTcp::open() {
  boost::asio::ip::tcp::resolver resolver(_ioService);

  auto hostname = _endpoint->host();
  auto portNumber = _endpoint->port();

  boost::asio::ip::tcp::endpoint asioEndpoint;
  std::unique_ptr<boost::asio::ip::tcp::resolver::query> query;
  if (_endpoint->domain() == AF_INET6) {
    query.reset(new boost::asio::ip::tcp::resolver::query(boost::asio::ip::tcp::v6(), hostname, std::to_string(portNumber)));
  } else if (_endpoint->domain() == AF_INET) {
    query.reset(new boost::asio::ip::tcp::resolver::query(boost::asio::ip::tcp::v4(), hostname, std::to_string(portNumber)));
  } else {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_IP_ADDRESS_INVALID);
  }
  boost::asio::ip::tcp::resolver::iterator iter = resolver.resolve(*query);

  asioEndpoint = iter->endpoint();
  _acceptor.open(asioEndpoint.protocol());

  _acceptor.set_option(
      boost::asio::ip::tcp::acceptor::reuse_address(
        ((EndpointIp*)_endpoint)->reuseAddress()));

  _acceptor.bind(asioEndpoint);
  _acceptor.listen();
}

void AcceptorTcp::asyncAccept(AcceptHandler const& handler) {
  createPeer();
  auto peer = dynamic_cast<SocketTcp*>(_peer.get());
  _acceptor.async_accept(peer->_socket, peer->_peerEndpoint, handler);
}

void AcceptorTcp::createPeer() {
  if (_endpoint->encryption() == Endpoint::EncryptionType::SSL) {
    _peer.reset(new SocketTcp(_ioService, SslServerFeature::SSL->createSslContext(), true));
  } else {
    _peer.reset(new SocketTcp(
        _ioService,
        boost::asio::ssl::context(boost::asio::ssl::context::method::sslv23),
        false));
  }
}
