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
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#include "AcceptorTcp.h"

#include "Basics/Exceptions.h"
#include "Endpoint/ConnectionInfo.h"
#include "Endpoint/EndpointIp.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/H2CommTask.h"
#include "GeneralServer/HttpCommTask.h"
#include "Logger/LogContext.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

using namespace arangodb;
using namespace arangodb::rest;

namespace arangodb {
namespace rest {

template<SocketType T>
void AcceptorTcp<T>::open() {
  asio_ns::ip::tcp::resolver resolver(_ctx.io_context);

  std::string hostname = _endpoint->host();
  int portNumber = _endpoint->port();

  asio_ns::ip::tcp::endpoint asioEndpoint;
  asio_ns::error_code ec;
  auto address = asio_ns::ip::address::from_string(hostname, ec);
  if (!ec) {
    asioEndpoint = asio_ns::ip::tcp::endpoint(address, portNumber);
  } else {  // we need to resolve the string containing the ip
    std::unique_ptr<asio_ns::ip::tcp::resolver::query> query;
    if (_endpoint->domain() == AF_INET6) {
      query = std::make_unique<asio_ns::ip::tcp::resolver::query>(
          asio_ns::ip::tcp::v6(), hostname, std::to_string(portNumber));
    } else if (_endpoint->domain() == AF_INET) {
      query = std::make_unique<asio_ns::ip::tcp::resolver::query>(
          asio_ns::ip::tcp::v4(), hostname, std::to_string(portNumber));
    } else {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_IP_ADDRESS_INVALID);
    }

    asio_ns::ip::tcp::resolver::iterator iter = resolver.resolve(*query, ec);
    if (ec) {
      LOG_TOPIC("383bc", ERR, Logger::COMMUNICATION)
          << "unable to to resolve endpoint ' " << _endpoint->specification()
          << "': " << ec.message();
      throw std::runtime_error(ec.message());
    }

    if (asio_ns::ip::tcp::resolver::iterator{} == iter) {
      LOG_TOPIC("05077", ERR, Logger::COMMUNICATION)
          << "unable to to resolve endpoint: endpoint is default constructed";
    }

    asioEndpoint = iter->endpoint();  // function not documented in boost?!
  }
  _acceptor.open(asioEndpoint.protocol());

  _acceptor.set_option(asio_ns::ip::tcp::acceptor::reuse_address(
      ((EndpointIp*)_endpoint)->reuseAddress()));

  _acceptor.bind(asioEndpoint, ec);
  if (ec) {
    LOG_TOPIC("874fa", ERR, Logger::COMMUNICATION)
        << "unable to bind to endpoint '" << _endpoint->specification()
        << "': " << ec.message();
    throw std::runtime_error(ec.message());
  }

  TRI_ASSERT(_endpoint->listenBacklog() > 8);
  _acceptor.listen(_endpoint->listenBacklog(), ec);
  if (ec) {
    LOG_TOPIC("c487e", ERR, Logger::COMMUNICATION)
        << "unable to listen to endpoint '" << _endpoint->specification()
        << ": " << ec.message();
    throw std::runtime_error(ec.message());
  }
  _open = true;

  LOG_TOPIC("853a9", DEBUG, arangodb::Logger::COMMUNICATION)
      << "successfully opened acceptor TCP";

  asyncAccept();
}

template<SocketType T>
void AcceptorTcp<T>::close() {
  if (_open) {
    _open = false;  // make sure the _open flag is `false` before we
                    // cancel/close the acceptor, since otherwise the
                    // handleError method will restart async_accept.
    _ctx.io_context.wrap([this]() { _acceptor.close(); });
  }
}

template<SocketType T>
void AcceptorTcp<T>::cancel() {
  _ctx.io_context.wrap([this]() { _acceptor.cancel(); });
}

template<>
void AcceptorTcp<SocketType::Tcp>::asyncAccept() {
  TRI_ASSERT(_endpoint->encryption() == Endpoint::EncryptionType::NONE);

  auto asioSocket =
      std::make_shared<AsioSocket<SocketType::Tcp>>(_server.selectIoContext());
  auto& socket = asioSocket->socket;
  auto& peer = asioSocket->peer;
  auto handler = [this, asioSocket = std::move(asioSocket)](
                     asio_ns::error_code const& ec) mutable {
    if (ec) {
      handleError(ec);
      return;
    }

    // set the endpoint
    ConnectionInfo info;
    info.endpoint = _endpoint->specification();
    info.endpointType = _endpoint->domainType();
    info.encryptionType = _endpoint->encryption();
    info.serverAddress = _endpoint->host();
    info.serverPort = _endpoint->port();

    info.clientAddress = asioSocket->peer.address().to_string();
    info.clientPort = asioSocket->peer.port();

    LOG_TOPIC("853aa", DEBUG, arangodb::Logger::COMMUNICATION)
        << "accepted connection from " << info.clientAddress << ":"
        << info.clientPort;

    auto commTask = std::make_shared<HttpCommTask<SocketType::Tcp>>(
        _server, std::move(info), std::move(asioSocket));
    _server.registerTask(std::move(commTask));
    this->asyncAccept();
  };

  // cppcheck-suppress accessMoved
  _acceptor.async_accept(socket, peer, withLogContext(std::move(handler)));
}

template<>
void AcceptorTcp<SocketType::Tcp>::performHandshake(
    std::shared_ptr<AsioSocket<SocketType::Tcp>> proto) {
  TRI_ASSERT(false);  // MSVC requires the implementation to exist
}

namespace {
bool tls_h2_negotiated(SSL* ssl) {
  const unsigned char* next_proto = nullptr;
  unsigned int next_proto_len = 0;

  SSL_get0_alpn_selected(ssl, &next_proto, &next_proto_len);

  // allowed value is "h2"
  // http://www.iana.org/assignments/tls-extensiontype-values/tls-extensiontype-values.xhtml
  if (next_proto != nullptr && next_proto_len == 2 &&
      memcmp(next_proto, "h2", 2) == 0) {
    return true;
  }
  return false;
}
}  // namespace

template<>
void AcceptorTcp<SocketType::Ssl>::performHandshake(
    std::shared_ptr<AsioSocket<SocketType::Ssl>> proto) {
  // io_context is single-threaded, no sync needed
  auto* ptr = proto.get();
  proto->timer.expires_from_now(std::chrono::seconds(60));
  proto->timer.async_wait([ptr](asio_ns::error_code const& ec) {
    if (ec) {  // canceled
      return;
    }
    ptr->shutdown([](asio_ns::error_code const&) {});  // ignore error
  });

  auto cb = [this,
             as = std::move(proto)](asio_ns::error_code const& ec) mutable {
    as->timer.cancel();
    if (ec) {
      LOG_TOPIC("4c6b4", DEBUG, arangodb::Logger::COMMUNICATION)
          << "error during TLS handshake: '" << ec.message() << "'";
      as.reset();  // ungraceful shutdown
      return;
    }

    // set the endpoint
    ConnectionInfo info;
    info.endpoint = _endpoint->specification();
    info.endpointType = _endpoint->domainType();
    info.encryptionType = _endpoint->encryption();
    info.serverAddress = _endpoint->host();
    info.serverPort = _endpoint->port();

    info.clientAddress = as->peer.address().to_string();
    info.clientPort = as->peer.port();

    std::shared_ptr<CommTask> task;
    if (tls_h2_negotiated(as->socket.native_handle())) {
      task = std::make_shared<H2CommTask<SocketType::Ssl>>(
          _server, std::move(info), std::move(as));
    } else {
      task = std::make_shared<HttpCommTask<SocketType::Ssl>>(
          _server, std::move(info), std::move(as));
    }

    _server.registerTask(std::move(task));
  };
  ptr->handshake(withLogContext(std::move(cb)));
}

template<>
void AcceptorTcp<SocketType::Ssl>::asyncAccept() {
  TRI_ASSERT(_endpoint->encryption() == Endpoint::EncryptionType::SSL);

  // select the io context for this socket
  auto& ctx = _server.selectIoContext();

  auto asioSocket =
      std::make_shared<AsioSocket<SocketType::Ssl>>(ctx, _server.sslContexts());
  auto& socket = asioSocket->socket.lowest_layer();
  auto& peer = asioSocket->peer;
  auto handler = [this, asioSocket = std::move(asioSocket)](
                     asio_ns::error_code const& ec) mutable {
    if (ec) {
      handleError(ec);
      return;
    }

    performHandshake(std::move(asioSocket));
    this->asyncAccept();
  };

  // cppcheck-suppress accessMoved
  _acceptor.async_accept(socket, peer, withLogContext(std::move(handler)));
}
}  // namespace rest
}  // namespace arangodb

template class arangodb::rest::AcceptorTcp<SocketType::Tcp>;
template class arangodb::rest::AcceptorTcp<SocketType::Ssl>;
