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

#include "Scheduler/AcceptorUnixDomain.h"

#include "Endpoint/EndpointUnixDomain.h"
#include "Scheduler/SocketUnixDomain.h"

using namespace arangodb;

void AcceptorUnixDomain::open() {
  boost::asio::local::stream_protocol::stream_protocol::endpoint endpoint(((EndpointUnixDomain*) _endpoint)->path());
  _acceptor.open(endpoint.protocol());
  _acceptor.bind(endpoint);
  _acceptor.listen();
}

void AcceptorUnixDomain::asyncAccept(AcceptHandler const& handler) {
  createPeer();
  auto peer = dynamic_cast<SocketUnixDomain*>(_peer.get());
  _acceptor.async_accept(peer->_socket, peer->_peerEndpoint, handler);
}

void AcceptorUnixDomain::createPeer() {
  if (_endpoint->encryption() == Endpoint::EncryptionType::SSL) {
    _peer.reset(new SocketUnixDomain(_ioService, SslServerFeature::SSL->createSslContext(), true));
  } else {
    _peer.reset(new SocketUnixDomain(
        _ioService,
        boost::asio::ssl::context(boost::asio::ssl::context::method::sslv23),
        false));
  }
}
