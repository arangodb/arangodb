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

#include "GeneralServer/AcceptorTcp.h"

#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Endpoint/EndpointIp.h"
#include "GeneralServer/SocketSslTcp.h"
#include "GeneralServer/SocketTcp.h"

using namespace arangodb;

void AcceptorTcp::open() {
  std::unique_ptr<asio_ns::ip::tcp::resolver> resolver(_context.newResolver());

  std::string hostname = _endpoint->host();
  int portNumber = _endpoint->port();

  asio_ns::ip::tcp::endpoint asioEndpoint;
  asio_ns::error_code err;
  auto address = asio_ns::ip::address::from_string(hostname, err);
  if (!err) {
    asioEndpoint = asio_ns::ip::tcp::endpoint(address, portNumber);
  } else {  // we need to resolve the string containing the ip
    std::unique_ptr<asio_ns::ip::tcp::resolver::query> query;
    if (_endpoint->domain() == AF_INET6) {
      query.reset(new asio_ns::ip::tcp::resolver::query(asio_ns::ip::tcp::v6(), hostname,
                                                        std::to_string(portNumber)));
    } else if (_endpoint->domain() == AF_INET) {
      query.reset(new asio_ns::ip::tcp::resolver::query(asio_ns::ip::tcp::v4(), hostname,
                                                        std::to_string(portNumber)));
    } else {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_IP_ADDRESS_INVALID);
    }

    asio_ns::ip::tcp::resolver::iterator iter = resolver->resolve(*query, err);
    if (err) {
      LOG_TOPIC("383bc", ERR, Logger::COMMUNICATION)
          << "unable to to resolve endpoint ' " << _endpoint->specification()
          << "': " << err.message();
      throw std::runtime_error(err.message());
    }

    if (asio_ns::ip::tcp::resolver::iterator{} == iter) {
      LOG_TOPIC("05077", ERR, Logger::COMMUNICATION)
          << "unable to to resolve endpoint: endpoint is default constructed";
    }

    asioEndpoint = iter->endpoint();  // function not documented in boost?!
  }
  _acceptor->open(asioEndpoint.protocol());

#ifdef _WIN32
  // on Windows everything is different of course:
  // we need to set SO_EXCLUSIVEADDRUSE to prevent others from binding to our
  // ip/port.
  // https://msdn.microsoft.com/en-us/library/windows/desktop/ms740621(v=vs.85).aspx
  BOOL trueOption = 1;

  if (::setsockopt(_acceptor->native_handle(), SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                   (char const*)&trueOption, sizeof(BOOL)) != 0) {
    LOG_TOPIC("1bcff", ERR, Logger::COMMUNICATION)
        << "unable to set acceptor socket option: " << WSAGetLastError();
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FAILED,
                                   "unable to set acceptor socket option");
  }
#else
  _acceptor->set_option(asio_ns::ip::tcp::acceptor::reuse_address(
      ((EndpointIp*)_endpoint)->reuseAddress()));
#endif

  _acceptor->bind(asioEndpoint, err);
  if (err) {
    LOG_TOPIC("874fa", ERR, Logger::COMMUNICATION)
        << "unable to bind to endpoint '" << _endpoint->specification()
        << "': " << err.message();
    throw std::runtime_error(err.message());
  }

  TRI_ASSERT(_endpoint->listenBacklog() > 8);
  _acceptor->listen(_endpoint->listenBacklog(), err);
  if (err) {
    LOG_TOPIC("c487e", ERR, Logger::COMMUNICATION)
        << "unable to listen to endpoint '" << _endpoint->specification()
        << ": " << err.message();
    throw std::runtime_error(err.message());
  }
}

void AcceptorTcp::asyncAccept(AcceptHandler const& handler) {
  TRI_ASSERT(!_peer);

  // select the io context for this socket
  auto& context = _server.selectIoContext();

  if (_endpoint->encryption() == Endpoint::EncryptionType::SSL) {
    auto sslContext = SslServerFeature::SSL->createSslContext();
    _peer.reset(new SocketSslTcp(context, std::move(sslContext)));
    SocketSslTcp* peer = static_cast<SocketSslTcp*>(_peer.get());
    _acceptor->async_accept(peer->_socket, peer->_peerEndpoint, handler);
  } else {
    _peer.reset(new SocketTcp(context));
    SocketTcp* peer = static_cast<SocketTcp*>(_peer.get());
    _acceptor->async_accept(*peer->_socket, peer->_peerEndpoint, handler);
  }
}
