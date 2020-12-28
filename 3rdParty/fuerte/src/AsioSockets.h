////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
                    asio_ns::ip::tcp::resolver& resolver,
                    SocketT& socket,
                    F&& done) {
  auto cb = [&socket, done(std::forward<F>(done))](auto ec, auto it) mutable {
    if (ec) { // error in address resolver
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
}


template<SocketType T>
struct Socket {};

template<>
struct Socket<SocketType::Tcp>  {
  Socket(EventLoopService&,
         asio_ns::io_context& ctx)
    : resolver(ctx), socket(ctx) {}

  ~Socket() {
    try {
      resolver.cancel();
      shutdown();
    } catch(...) {}
  }

  template<typename F>
  void connect(detail::ConnectionConfiguration const& config, F&& done) {
    resolveConnect(config, resolver, socket, std::forward<F>(done));
  }

  void shutdown(std::shared_ptr<fuerte::Connection> const& connection = nullptr) {
    if (socket.is_open()) {
      asio_ns::error_code ec; // prevents exceptions
      socket.cancel(ec);
      ec.clear();
      socket.shutdown(asio_ns::ip::tcp::socket::shutdown_both, ec);
      ec.clear();
      socket.close(ec);
    }
  }

  asio_ns::ip::tcp::resolver resolver;
  asio_ns::ip::tcp::socket socket;
};

template<>
struct Socket<fuerte::SocketType::Ssl> {
  Socket(EventLoopService& loop, asio_ns::io_context& ctx)
  : resolver(ctx), socket(ctx, loop.sslContext()), timer(ctx), cleanupDone(false) {}

  ~Socket() {
    try {
      resolver.cancel();
      // The shutdown method uses asynchronous calls. Therefore, its
      // completion handlers have a copy of a shared_ptr to the connection
      // object, thereby keeping ourselves alive, too. This means that this
      // destructor is only called after the asynchronous completion
      // handlers have all been called. Therefore, it is safe to directly
      // close the lowest_layer socket here and call it a day!
      // It is not allowed, however, to call shutdown from here!
      shutdownTcp();
    } catch(...) {}
  }

  template<typename F>
  void connect(detail::ConnectionConfiguration const& config, F&& done) {
    bool verify = config._verifyHost;
    resolveConnect(config, resolver, socket.next_layer(),
                   [=, done(std::forward<F>(done))](auto const& ec) mutable {
      if (ec) {
        done(ec);
        return;
      }
 
      // Set SNI Hostname (many hosts need this to handshake successfully)
      if (!SSL_set_tlsext_host_name(socket.native_handle(), config._host.c_str())) {
        boost::system::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
        done(ec);
        return;
      }

      try {
        // Perform SSL handshake and verify the remote host's certificate.
        socket.next_layer().set_option(asio_ns::ip::tcp::no_delay(true));
        if (verify) {
          socket.set_verify_mode(asio_ns::ssl::verify_peer);
          socket.set_verify_callback(asio_ns::ssl::rfc2818_verification(config._host));
        } else {
          socket.set_verify_mode(asio_ns::ssl::verify_none);
        }
      } catch(boost::system::system_error const& exc) {
        done(exc.code());
        return;
      }
      
      socket.async_handshake(asio_ns::ssl::stream_base::client, std::move(done));
    });
  }
  
  void shutdown(std::shared_ptr<fuerte::Connection> const& connection) {
    // The connection object is needed to hand it to the completion handlers
    // to ensure that the connection object is still alive as long as
    // asynchronous operations are ongoing. It needs to be a connection
    // object using this socket for communications! This will in turn
    // keep ourselves alive for long enough! Note that both completion
    // handlers below intentionally copy the connection shared_ptr!
    if (socket.lowest_layer().is_open()) {
      asio_ns::error_code ec; // ignored
      socket.lowest_layer().cancel(ec);
      // Set a timeout, since the shutdown could block forever, if we
      // reach the timeout, we simply give up on TLS and close the TCP
      // layer. Note the cleanupDone is necessary, since it is possible
      // that both the completion handler of the timeout and the one
      // of the async_shutdown are called without error!
      timer.expires_from_now(std::chrono::seconds(3));
      timer.async_wait([connection, this](asio_ns::error_code ec) {
        if (!cleanupDone && !ec) {
          this->shutdownTcp();
          cleanupDone = true;
        }
      });
      socket.async_shutdown([connection, this](auto const& ec) {
        timer.cancel();
        // I do not actually know why we do not do this on Windows (Max)
#ifndef _WIN32
        if (!cleanupDone && (!ec || ec == asio_ns::error::basic_errors::not_connected)) {
          this->shutdownTcp();
          cleanupDone = true;
        }
#endif
      });
    }
  }

  void shutdownTcp() {
    if (socket.lowest_layer().is_open()) {
      asio_ns::error_code ec; // ignored
      socket.lowest_layer().cancel(ec);
      ec.clear();
      socket.lowest_layer().shutdown(asio_ns::ip::tcp::socket::shutdown_both, ec);
      ec.clear();
      socket.lowest_layer().close(ec);
    }
  }
  
  asio_ns::ip::tcp::resolver resolver;
  asio_ns::ssl::stream<asio_ns::ip::tcp::socket> socket;
  asio_ns::steady_timer timer;
  std::atomic<bool> cleanupDone;
};

#ifdef ASIO_HAS_LOCAL_SOCKETS
template<>
struct Socket<fuerte::SocketType::Unix> {
  
  Socket(EventLoopService&, asio_ns::io_context& ctx) : socket(ctx) {}
  ~Socket() {
    shutdown();
  }
  
  template<typename CallbackT>
  void connect(detail::ConnectionConfiguration const& config, CallbackT done) {
    asio_ns::local::stream_protocol::endpoint ep(config._host);
    socket.async_connect(ep, [done](asio_ns::error_code const& ec) {
      done(ec);
    });
  }
  void shutdown(std::shared_ptr<fuerte::Connection> const& connection = nullptr) {
    if (socket.is_open()) {
      asio_ns::error_code error; // prevents exceptions
      socket.cancel(error);
      socket.shutdown(asio_ns::ip::tcp::socket::shutdown_both, error);
      socket.close(error);
    }
  }
  
  asio_ns::local::stream_protocol::socket socket;
};
#endif // ASIO_HAS_LOCAL_SOCKETS

}}}  // namespace arangodb::fuerte::v1
#endif
