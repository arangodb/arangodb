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
#include "Basics/Exceptions.h"
#include "Endpoint/EndpointIp.h"
#include "Scheduler/SocketTcp.h"

using namespace arangodb;

void AcceptorTcp::open() {
  boost::asio::ip::tcp::resolver resolver(_ioService);

  std::string hostname = _endpoint->host();
  int portNumber = _endpoint->port();

  boost::asio::ip::tcp::endpoint asioEndpoint;
  boost::system::error_code err;
  auto address = boost::asio::ip::address::from_string(hostname, err);
  if (!err) {
    asioEndpoint = boost::asio::ip::tcp::endpoint(address, portNumber);
  } else {  // we need to resolve the string containing the ip
    std::unique_ptr<boost::asio::ip::tcp::resolver::query> query;
    if (_endpoint->domain() == AF_INET6) {
      query.reset(new boost::asio::ip::tcp::resolver::query(boost::asio::ip::tcp::v6(), hostname,
                                                            std::to_string(portNumber)));
    } else if (_endpoint->domain() == AF_INET) {
      query.reset(new boost::asio::ip::tcp::resolver::query(boost::asio::ip::tcp::v4(), hostname,
                                                            std::to_string(portNumber)));
    } else {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_IP_ADDRESS_INVALID);
    }

    boost::asio::ip::tcp::resolver::iterator iter = resolver.resolve(*query, err);
    if (err) {
      LOG_TOPIC(ERR, Logger::COMMUNICATION)
          << "unable to to resolve endpoint: " << err.message();
      throw std::runtime_error(err.message());
    }

    if (boost::asio::ip::tcp::resolver::iterator{} == iter) {
      LOG_TOPIC(ERR, Logger::COMMUNICATION)
          << "unable to to resolve endpoint: endpoint is default constructed";
    }

    asioEndpoint = iter->endpoint();  // function not documented in boost?!
  }
  _acceptor.open(asioEndpoint.protocol());

#ifdef _WIN32
  // on Windows everything is different of course:
  // we need to set SO_EXCLUSIVEADDRUSE to prevent others from binding to our
  // ip/port.
  // https://msdn.microsoft.com/en-us/library/windows/desktop/ms740621(v=vs.85).aspx
  BOOL trueOption = 1;

  if (::setsockopt(_acceptor.native(), SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                   (char const*)&trueOption, sizeof(BOOL)) != 0) {
    LOG_TOPIC(ERR, Logger::COMMUNICATION)
        << "unable to set acceptor socket option: " << WSAGetLastError();
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FAILED,
                                   "unable to set acceptor socket option");
  }
#else
  _acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(
      ((EndpointIp*)_endpoint)->reuseAddress()));

#endif

  _acceptor.bind(asioEndpoint, err);
  if (err) {
    LOG_TOPIC(ERR, Logger::COMMUNICATION) << "unable to bind endpoint: " << err.message();
    throw std::runtime_error(err.message());
  }
  _acceptor.listen(_endpoint->listenBacklog(), err);
  if (err) {
    LOG_TOPIC(ERR, Logger::COMMUNICATION) << "unable to bind endpoint: " << err.message();
    throw std::runtime_error(err.message());
  }
}

void AcceptorTcp::asyncAccept(AcceptHandler const& handler) {
  createPeer();
  auto peer = dynamic_cast<SocketTcp*>(_peer.get());
  if (peer == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "unexpected socket type");
  }
  _acceptor.async_accept(peer->_socket, peer->_peerEndpoint, handler);
}

void AcceptorTcp::createPeer() {
  if (_endpoint->encryption() == Endpoint::EncryptionType::SSL) {
    _peer.reset(new SocketTcp(_ioService, SslServerFeature::SSL->createSslContext(), true));
  } else {
    _peer.reset(new SocketTcp(_ioService,
                              boost::asio::ssl::context(boost::asio::ssl::context::method::sslv23),
                              false));
  }
}
