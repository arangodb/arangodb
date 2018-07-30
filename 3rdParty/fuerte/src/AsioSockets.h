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

namespace arangodb { namespace fuerte { inline namespace v1 {
  
template<SocketType T>
struct Socket {};

template<>
struct Socket<SocketType::Tcp>  {
  Socket(asio_ns::io_context& ctx)
    : resolver(ctx), socket(ctx) {}
  
  ~Socket() {
    resolver.cancel();
    shutdown();
  }
  
  void connect(detail::ConnectionConfiguration const& config,
               std::function<void(asio_ns::error_code const&)> done) {
    auto cb = [this, done](asio_ns::error_code const& ec,
                     asio_ns::ip::tcp::resolver::iterator it) {
      if (ec) { // error
        done(ec);
        return;
      }
      // A successful resolve operation is guaranteed to pass a
      // non-empty range to the handler.
      // Start the asynchronous connect operation.
      asio_ns::async_connect(socket, it,
                             [done](asio_ns::error_code const& ec,
                                    asio_ns::ip::tcp::resolver::iterator const&) {
                               done(ec);
                             });
    };
    // Resolve the host asynchronous into a series of endpoints
    resolver.async_resolve({config._host, config._port}, cb);
  }
  void shutdown() {
    if (socket.is_open()) {
      asio_ns::error_code error; // prevents exceptions
      socket.cancel(error);
      socket.shutdown(asio_ns::ip::tcp::socket::shutdown_both, error);
      socket.close(error);
    }
  }
  
  asio_ns::ip::tcp::resolver resolver;
  asio_ns::ip::tcp::socket socket;
};

template<>
struct Socket<fuerte::SocketType::Ssl> {
  Socket(asio_ns::io_context& ctx)
  : context(asio_ns::ssl::context::sslv23), resolver(ctx), socket(ctx, context) {}
  
  ~Socket() {
    resolver.cancel();
    shutdown();
  }
  
  void connect(detail::ConnectionConfiguration const& config,
               std::function<void(asio_ns::error_code const&)> done) {
    auto rcb = [this, done, &config](asio_ns::error_code const& ec,
                            asio_ns::ip::tcp::resolver::iterator it) {
      if (ec) { // error
        done(ec);
        return;
      }
      // A successful resolve operation is guaranteed to pass a
      // non-empty range to the handler.
      auto cbc = [this, done, &config](asio_ns::error_code const& ec,
                                       asio_ns::ip::tcp::resolver::iterator const&) {
        if (ec) {
          done(ec);
          return;
        }
        
        /*if (config._verifyHost) {
          // Perform SSL handshake and verify the remote host's certificate.
          socket.lowest_layer().set_option(asio_ns::ip::tcp::no_delay(true));
          socket.set_verify_mode(asio_ns::ssl::verify_peer);
          socket.set_verify_callback(asio_ns::ssl::rfc2818_verification(config._host));
        } else {*/
          socket.set_verify_mode(asio_ns::ssl::verify_none);
        //}
        
        //FUERTE_LOG_CALLBACKS << "starting ssl handshake " << std::endl;
        socket.async_handshake(asio_ns::ssl::stream_base::client,
                               [done](asio_ns::error_code const& ec) {
                                 done(ec);
                               });
      };
      
      // Start the asynchronous connect operation.
      asio_ns::async_connect(socket.lowest_layer(), it, cbc);
    };
    // Resolve the host asynchronous into a series of endpoints
    resolver.async_resolve({config._host, config._port}, rcb);
  }
  void shutdown() {
    //socket.cancel();
    if (socket.lowest_layer().is_open()) {
      asio_ns::error_code error;
      socket.shutdown(error);
      socket.lowest_layer().shutdown(asio_ns::ip::tcp::socket::shutdown_both, error);
      socket.lowest_layer().close(error);
    }
  }
  
  // TODO move to EventLoop
  asio_ns::ssl::context context;
  asio_ns::ip::tcp::resolver resolver;
  asio_ns::ssl::stream<asio_ns::ip::tcp::socket> socket;
};

template<>
struct Socket<fuerte::SocketType::Unix> {
  
  Socket(asio_ns::io_context& ctx) : socket(ctx) {}
  ~Socket() {
    shutdown();
  }
  
  void connect(detail::ConnectionConfiguration const& config,
               std::function<void(asio_ns::error_code const&)> done) {
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

}}}  // namespace arangodb::fuerte::v1
#endif
