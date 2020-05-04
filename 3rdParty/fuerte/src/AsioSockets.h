////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGO_CXX_DRIVER_ASIO_CONNECTION_H
#define ARANGO_CXX_DRIVER_ASIO_CONNECTION_H 1

#include <fuerte/asio_ns.h>
#include <fuerte/loop.h>

namespace arangodb { namespace fuerte { inline namespace v1 {

namespace {
template <typename SocketT, typename F>
void resolveConnect(detail::ConnectionConfiguration const& config,
                    asio_ns::ip::tcp::resolver& resolver, SocketT& socket,
                    F&& done) {
  auto cb = [&socket, done(std::forward<F>(done))](auto ec, auto it) mutable {
    if (ec) {  // error in address resolver
      done(ec);
      return;
    }

    // A successful resolve operation is guaranteed to pass a
    // non-empty range to the handler.
    asio_ns::async_connect(socket, it,
                           [done(std::move(done))](auto ec, auto it) mutable {
                             std::forward<F>(done)(ec);
                           });
  };

  // windows does not like async_resolve
#ifdef _WIN32
  asio_ns::error_code ec;
  auto it = resolver.resolve(config._host, config._port, ec);
  cb(ec, it);
#else
  // Resolve the host asynchronous into a series of endpoints
  resolver.async_resolve(config._host, config._port, std::move(cb));
#endif
}
}  // namespace

template <SocketType T>
struct Socket {};

template <>
struct Socket<SocketType::Tcp> {
  Socket(EventLoopService&, asio_ns::io_context& ctx)
      : resolver(ctx), socket(ctx), timer(ctx) {}

  ~Socket() { this->cancel(); }

  template <typename F>
  void connect(detail::ConnectionConfiguration const& config, F&& done) {
    resolveConnect(config, resolver, socket, std::forward<F>(done));
  }

  void cancel() {
    try {
      timer.cancel();
      resolver.cancel();
      if (socket.is_open()) {  // non-graceful shutdown
        asio_ns::error_code ec;
        socket.close(ec);
      }
    } catch (...) {
    }
  }

  template <typename F>
  void shutdown(F&& cb) {
    asio_ns::error_code ec;  // prevents exceptions
    try {
#ifndef _WIN32
      socket.cancel(ec);
#endif
      if (socket.is_open()) {
        socket.shutdown(asio_ns::ip::tcp::socket::shutdown_both, ec);
        ec.clear();
        socket.close(ec);
      }
    } catch(...) {}
    std::forward<F>(cb)(ec);
  }

  asio_ns::ip::tcp::resolver resolver;
  asio_ns::ip::tcp::socket socket;
  asio_ns::steady_timer timer;
};

template <>
struct Socket<fuerte::SocketType::Ssl> {
  Socket(EventLoopService& loop, asio_ns::io_context& ctx)
      : resolver(ctx), socket(ctx, loop.sslContext()), timer(ctx) {}

  ~Socket() { this->cancel(); }

  template <typename F>
  void connect(detail::ConnectionConfiguration const& config, F&& done) {
    bool verify = config._verifyHost;
    resolveConnect(
        config, resolver, socket.next_layer(),
        [=, done(std::forward<F>(done))](auto const& ec) mutable {
          if (ec) {
            done(ec);
            return;
          }

          // Perform SSL handshake and verify the remote host's certificate.
          socket.next_layer().set_option(asio_ns::ip::tcp::no_delay(true));
          if (verify) {
            socket.set_verify_mode(asio_ns::ssl::verify_peer);
            socket.set_verify_callback(
                asio_ns::ssl::rfc2818_verification(config._host));
          } else {
            socket.set_verify_mode(asio_ns::ssl::verify_none);
          }

          socket.async_handshake(asio_ns::ssl::stream_base::client,
                                 std::move(done));
        });
  }

  void cancel() {
    try {
      timer.cancel();
      resolver.cancel();
      if (socket.lowest_layer().is_open()) {  // non-graceful shutdown
        asio_ns::error_code ec;
        socket.lowest_layer().close(ec);
      }
    } catch (...) {
    }
  }

  template <typename F>
  void shutdown(F&& cb) {
    asio_ns::error_code ec;  // prevents exceptions
    socket.lowest_layer().cancel(ec);
    if (!socket.lowest_layer().is_open()) {
      timer.cancel(ec);
      std::forward<F>(cb)(ec);
      return;
    }
    timer.expires_from_now(std::chrono::seconds(3));
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
  }

  asio_ns::ip::tcp::resolver resolver;
  asio_ns::ssl::stream<asio_ns::ip::tcp::socket> socket;
  asio_ns::steady_timer timer;
};

#ifdef ASIO_HAS_LOCAL_SOCKETS
template <>
struct Socket<fuerte::SocketType::Unix> {
  Socket(EventLoopService&, asio_ns::io_context& ctx)
      : socket(ctx), timer(ctx) {}
  ~Socket() { this->cancel(); }

  template <typename F>
  void connect(detail::ConnectionConfiguration const& config, F&& done) {
    asio_ns::local::stream_protocol::endpoint ep(config._host);
    socket.async_connect(ep, std::forward<F>(done));
  }

  void cancel() {
    timer.cancel();
    if (socket.is_open()) {  // non-graceful shutdown
      asio_ns::error_code ec;
      socket.close(ec);
    }
  }

  template <typename F>
  void shutdown(F&& cb) {
    asio_ns::error_code ec;  // prevents exceptions
    timer.cancel(ec);
    if (socket.is_open()) {
      socket.cancel(ec);
      socket.shutdown(asio_ns::ip::tcp::socket::shutdown_both, ec);
      socket.close(ec);
    }
    std::forward<F>(cb)(ec);
  }

  asio_ns::local::stream_protocol::socket socket;
  asio_ns::steady_timer timer;
};
#endif  // ASIO_HAS_LOCAL_SOCKETS

}}}  // namespace arangodb::fuerte::v1
#endif
