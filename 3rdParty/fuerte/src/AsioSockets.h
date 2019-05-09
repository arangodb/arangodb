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
  
  template<typename CT>
  void connect(detail::ConnectionConfiguration const& config, CT&& done) {
    auto cb = [this, done = std::forward<CT>(done)]
    (asio_ns::error_code const& ec,
     asio_ns::ip::tcp::resolver::iterator it) {
      if (ec) { // error
        done(ec);
        return;
      }
      // A successful resolve operation is guaranteed to pass a
      // non-empty range to the handler.
      asio_ns::async_connect(socket, it, [done](asio_ns::error_code const& ec,
                                                asio_ns::ip::tcp::resolver::iterator const&) {
        done(ec);
      });
    };
    // Resolve the host asynchronous into a series of endpoints
//    resolver.async_resolve(config._host, config._port, std::move(cb));
    asio_ns::error_code ec;
    auto it = resolver.resolve(config._host, config._port, ec);
    cb(ec, it);
  }
  void shutdown() {
    if (socket.is_open()) {
      asio_ns::error_code ec; // prevents exceptions
      socket.cancel(ec);
      socket.shutdown(asio_ns::ip::tcp::socket::shutdown_both, ec);
      socket.close(ec);
    }
  }
  
  asio_ns::ip::tcp::resolver resolver;
  asio_ns::ip::tcp::socket socket;
};

template<>
struct Socket<fuerte::SocketType::Ssl> {
  Socket(EventLoopService& loop,
         asio_ns::io_context& ctx)
  : resolver(ctx), socket(ctx, loop.sslContext()) {}
  
  ~Socket() {
    try {
      resolver.cancel();
      shutdown();
    } catch(...) {}
  }
  
  template<typename CT>
  void connect(detail::ConnectionConfiguration const& config, CT&& done) {
    auto rcb = [this, &config, done = std::forward<CT>(done)]
        (asio_ns::error_code const& ec,
         asio_ns::ip::tcp::resolver::iterator it) {
      if (ec) { // error
        done(ec);
        return;
      }
      // A successful resolve operation is guaranteed to pass a
      // non-empty range to the handler.
      auto cbc = [this, done, &config]
          (asio_ns::error_code const& ec,
           asio_ns::ip::tcp::resolver::iterator const&) {
        if (ec) {
          done(ec);
          return;
        }
        
        // Perform SSL handshake and verify the remote host's certificate.
        socket.lowest_layer().set_option(asio_ns::ip::tcp::no_delay(true));
        if (config._verifyHost) {
          socket.set_verify_mode(asio_ns::ssl::verify_peer);
          socket.set_verify_callback(asio_ns::ssl::rfc2818_verification(config._host));
        } else {
          socket.set_verify_mode(asio_ns::ssl::verify_none);
        }
        
        socket.async_handshake(asio_ns::ssl::stream_base::client, std::move(done));
      };
      
      // Start the asynchronous connect operation.
      asio_ns::async_connect(socket.lowest_layer(), it, std::move(cbc));
    };
    // Resolve the host asynchronous into a series of endpoints
    //resolver.async_resolve(config._host, config._port, std::move(rcb));
    asio_ns::error_code ec;
    auto it = resolver.resolve(config._host, config._port, ec);
    rcb(ec, it);
  }
  void shutdown() {
    if (socket.lowest_layer().is_open()) {
      asio_ns::error_code ec;
      socket.lowest_layer().cancel(ec);
      socket.shutdown(ec);
      socket.lowest_layer().shutdown(asio_ns::ip::tcp::socket::shutdown_both, ec);
      socket.lowest_layer().close(ec);
    }
  }
  
  asio_ns::ip::tcp::resolver resolver;
  asio_ns::ssl::stream<asio_ns::ip::tcp::socket> socket;
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
  void shutdown() {
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
  
inline ErrorCondition checkEOFError(asio_ns::error_code e, ErrorCondition c) {
  return e == asio_ns::error::misc_errors::eof ? ErrorCondition::ConnectionClosed : c;
}

}}}  // namespace arangodb::fuerte::v1
#endif
