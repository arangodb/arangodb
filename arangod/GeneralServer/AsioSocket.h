////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GENERAL_SERVER_ASIOSOCKET_H
#define ARANGOD_GENERAL_SERVER_ASIOSOCKET_H 1

#include "GeneralServer/IoContext.h"
#include "GeneralServer/SslServerFeature.h"

namespace arangodb {
namespace rest {

enum class SocketType { Tcp = 1, Ssl = 2, Unix = 3 };

/// Wrapper class that contains sockets / ssl-stream
/// and the corrsponding peer endpoint
template <SocketType T>
struct AsioSocket {};

template <>
struct AsioSocket<SocketType::Tcp> {
  explicit AsioSocket(arangodb::rest::IoContext& ctx)
      : context(ctx), socket(ctx.io_context), timer(ctx.io_context) {
    context.incClients();
  }

  ~AsioSocket() {
    timer.cancel();
    try {
      if (socket.is_open()) {  // non-graceful shutdown
        asio_ns::error_code ec;
        socket.close(ec);
      }
    } catch (...) {
    }
    context.decClients();
  }

  void setNonBlocking(bool v) { socket.non_blocking(v); }
  static constexpr bool supportsMixedIO() { return true; }
  std::size_t available(asio_ns::error_code& ec) const {
    return socket.lowest_layer().available(ec);
  }

  template <typename F>
  void shutdown(F&& cb) {
    asio_ns::error_code ec;
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
    std::forward<F>(cb)(ec);
  }

  arangodb::rest::IoContext& context;
  asio_ns::ip::tcp::socket socket;
  asio_ns::ip::tcp::acceptor::endpoint_type peer;
  asio_ns::steady_timer timer;
  asio_ns::streambuf buffer;
};

template <>
struct AsioSocket<SocketType::Ssl> {
  AsioSocket(arangodb::rest::IoContext& ctx, SslServerFeature::SslContextList sslContexts)
      : context(ctx), storedSslContexts(std::move(sslContexts)), socket(ctx.io_context, (*storedSslContexts)[0]), timer(ctx.io_context) {
    context.incClients();
  }

  ~AsioSocket() {
    try {
      timer.cancel();
      if (socket.lowest_layer().is_open()) {  // non-graceful shutdown
        asio_ns::error_code ec;
        socket.lowest_layer().close(ec);
      }
    } catch (...) {
    }
    context.decClients();
  }

  void setNonBlocking(bool v) { socket.lowest_layer().non_blocking(v); }
  static constexpr bool supportsMixedIO() { return false; }
  std::size_t available(asio_ns::error_code& ec) const {
    return 0;  // always disable
  }

  template <typename F>
  void handshake(F&& cb) {
    // Perform SSL handshake and verify the remote host's certificate.
    socket.lowest_layer().set_option(asio_ns::ip::tcp::no_delay(true));
    socket.async_handshake(asio_ns::ssl::stream_base::server, std::forward<F>(cb));
  }

  template <typename F>
  void shutdown(F&& cb) {
    if (socket.lowest_layer().is_open()) {
      // a graceful SSL shutdown performs a write & read
      timer.expires_after(std::chrono::seconds(3));
      timer.async_wait([this](asio_ns::error_code ec) {
        if (!ec) {
          socket.lowest_layer().close(ec);
        }
      });
      socket.async_shutdown([cb(std::forward<F>(cb)), this](auto const& ec) {
        timer.cancel();
#ifndef _WIN32
        if (!ec || ec == asio_ns::error::basic_errors::not_connected) {
          asio_ns::error_code ec2;
          socket.lowest_layer().close(ec2);
        }
#endif
        cb(ec);
      });
    } else {
      std::forward<F>(cb)(asio_ns::error_code{});
    }
  }

  arangodb::rest::IoContext& context;
  SslServerFeature::SslContextList storedSslContexts;
  asio_ns::ssl::stream<asio_ns::ip::tcp::socket> socket;
  asio_ns::ip::tcp::acceptor::endpoint_type peer;
  asio_ns::steady_timer timer;
  asio_ns::streambuf buffer;
};

#if defined(BOOST_ASIO_HAS_LOCAL_SOCKETS) || defined(ASIO_HAS_LOCAL_SOCKETS)
template <>
struct AsioSocket<SocketType::Unix> {
  AsioSocket(arangodb::rest::IoContext& ctx)
      : context(ctx), socket(ctx.io_context), timer(ctx.io_context) {
    context.incClients();
  }
  ~AsioSocket() {
    try {
      timer.cancel();
      if (socket.is_open()) {  // non-graceful shutdown
        asio_ns::error_code ec;
        socket.close(ec);
      }
    } catch (...) {
    }
    context.decClients();
  }

  void setNonBlocking(bool v) { socket.non_blocking(v); }
  static constexpr bool supportsMixedIO() { return true; }
  std::size_t available(asio_ns::error_code& ec) const {
    return socket.lowest_layer().available(ec);
  }

  template <typename F>
  void shutdown(F&& cb) {
    asio_ns::error_code ec;
    if (socket.is_open()) {
      socket.cancel(ec);
      if (!ec) {
        socket.shutdown(asio_ns::ip::tcp::socket::shutdown_both, ec);
      }
      if (!ec) {
        socket.close(ec);
      }
    }
    std::forward<F>(cb)(ec);
  }

  arangodb::rest::IoContext& context;
  asio_ns::local::stream_protocol::socket socket;
  asio_ns::local::stream_protocol::acceptor::endpoint_type peer;
  asio_ns::steady_timer timer;
  asio_ns::streambuf buffer;
};
#endif  // ASIO_HAS_LOCAL_SOCKETS

}  // namespace rest
}  // namespace arangodb
#endif
