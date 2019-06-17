////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018-2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GENERAL_SERVER_ASIOSOCKET_H
#define ARANGOD_GENERAL_SERVER_ASIOSOCKET_H 1

#include "GeneralServer/IoContext.h"

namespace arangodb {
namespace rest {
  
enum class SocketType { Tcp = 1, Ssl = 2, Unix = 3 };

/// Wrapper class that contains sockets / ssl-stream
/// and the corrsponding peer endpoint
template<SocketType T>
struct AsioSocket {};

template<>
struct AsioSocket<SocketType::Tcp> {
  AsioSocket(arangodb::rest::IoContext& ctx)
    : context(ctx), socket(ctx.io_context) {
      context.incClients();
    }
  
  ~AsioSocket() {
    try {
      asio_ns::error_code ec;
      shutdown(ec);
    } catch(...) {}
    context.decClients();
  }
  
  void setNonBlocking(bool v) { socket.non_blocking(v); }
  
  void shutdown(asio_ns::error_code& ec) {
    if (socket.is_open()) {
#ifndef _WIN32
      socket.cancel(ec);
#endif
      if (!ec) {
        socket.shutdown(asio_ns::ip::tcp::socket::shutdown_both, ec);
      }
      if (!ec || ec == asio_ns::error::basic_errors::not_connected) {
        ec.clear();
        socket.close(ec);
      }
    }
  }
  
  arangodb::rest::IoContext& context;
  asio_ns::ip::tcp::socket socket;
  asio_ns::ip::tcp::acceptor::endpoint_type peer;
};

template<>
struct AsioSocket<SocketType::Ssl> {
  AsioSocket(arangodb::rest::IoContext& ctx,
             asio_ns::ssl::context& sslContext)
  : context(ctx), socket(ctx.io_context, sslContext) {
    context.incClients();
  }
  
  ~AsioSocket() {
    try {
      asio_ns::error_code ec;
      shutdown(ec);
    } catch(...) {}
    context.decClients();
  }
  
  void setNonBlocking(bool v) { socket.lowest_layer().non_blocking(v); }
  
  template<typename F>
  void handshake(F&& cb) {
    // Perform SSL handshake and verify the remote host's certificate.
    socket.next_layer().set_option(asio_ns::ip::tcp::no_delay(true));
//    if (config._verifyHost) {
//      socket.set_verify_mode(asio_ns::ssl::verify_peer);
//      socket.set_verify_callback(asio_ns::ssl::rfc2818_verification(config._host));
//    } else {
//      socket.set_verify_mode(asio_ns::ssl::verify_none);
//    }
//    timer.expires_from_now(std::chrono::seconds(30)); // TODO configure ?
//    timer.async_wait([this](asio_ns::error_code const& ec) {
//      if (!ec) {  // was not canceled
//        asio_ns::error_code innerEc;
//        this->shutdown(innerEc);
//      }
//    });
    socket.async_handshake(asio_ns::ssl::stream_base::client, std::forward<F>(cb));
  }
  
  void shutdown(asio_ns::error_code& ec) {
    if (socket.lowest_layer().is_open()) {
#ifndef _WIN32
      socket.lowest_layer().cancel(ec);
#endif
      if (!ec) {
        socket.shutdown(ec);
      }
//      if (!ec) {
//        socket.lowest_layer().shutdown(asio_ns::ip::tcp::socket::shutdown_both, ec);
//      }
#ifndef _WIN32
      if (!ec || ec == asio_ns::error::basic_errors::not_connected) {
        ec.clear();
        socket.lowest_layer().close(ec);
      }
#endif
    }
  }
  
  arangodb::rest::IoContext& context;
  asio_ns::ssl::stream<asio_ns::ip::tcp::socket> socket;
  asio_ns::ip::tcp::acceptor::endpoint_type peer;
};

#if defined(BOOST_ASIO_HAS_LOCAL_SOCKETS) || defined(ASIO_HAS_LOCAL_SOCKETS)
template<>
struct AsioSocket<SocketType::Unix> {
  
  AsioSocket(arangodb::rest::IoContext& ctx)
  : context(ctx), socket(ctx.io_context) {
    context.incClients();
  }
  ~AsioSocket() {
    asio_ns::error_code ec;
    shutdown(ec);
    context.decClients();
  }
  
  void setNonBlocking(bool v) { socket.non_blocking(v); }
  
  void shutdown(asio_ns::error_code& ec) {
    if (socket.is_open()) {
      socket.cancel(ec);
      if (!ec) {
        socket.shutdown(asio_ns::ip::tcp::socket::shutdown_both, ec);
      }
      if (!ec) {
        socket.close(ec);
      }
    }
  }
  
  arangodb::rest::IoContext& context;
  asio_ns::local::stream_protocol::socket socket;
  asio_ns::local::stream_protocol::acceptor::endpoint_type peer;
};
#endif // ASIO_HAS_LOCAL_SOCKETS

} // namespace rest
} // namespace arangodb
#endif
