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
#include "debugging.h"

namespace arangodb { namespace fuerte { inline namespace v1 {

namespace {
template <typename SocketT, typename F, typename IsAbortedCb>
void resolveConnect(detail::ConnectionConfiguration const& config,
                    asio_ns::ip::tcp::resolver& resolver, SocketT& socket,
                    F&& done, IsAbortedCb&& isAborted) {
  auto cb = [&socket, 
#ifdef ARANGODB_USE_GOOGLE_TESTS
             fail = config._failConnectAttempts > 0,
#endif  
             done = std::forward<F>(done),
             isAborted = std::forward<IsAbortedCb>(isAborted)](auto ec, auto it) mutable {
#ifdef ARANGODB_USE_GOOGLE_TESTS
    if (fail) {
      // use an error code != operation_aborted
      ec = boost::system::errc::make_error_code(boost::system::errc::not_enough_memory);
    }
#endif

    if (isAborted()) {
      ec = asio_ns::error::operation_aborted;
    }

    if (ec) {  // error in address resolver
      FUERTE_LOG_DEBUG << "received error during address resolving: " << ec.message() << "\n";
      done(ec);
      return;
    }

    try {
      // A successful resolve operation is guaranteed to pass a
      // non-empty range to the handler.
      asio_ns::async_connect(socket, it,
                             [done](auto ec, auto it) mutable {
                               if (ec) {
                                 FUERTE_LOG_DEBUG << "executing async connect callback, error: " << ec.message() << "\n";
                               } else {
                                 FUERTE_LOG_DEBUG << "executing async connect callback, no error\n";
                               }
                               std::forward<F>(done)(ec);
                             });
    } catch (std::bad_alloc const&) {
      // definitely an OOM error
      done(boost::system::errc::make_error_code(boost::system::errc::not_enough_memory));
    } catch (...) {
      // probably not an OOM error, but we don't know what it actually is.
      // there is no code for a generic error that we could use here
      done(boost::system::errc::make_error_code(boost::system::errc::not_enough_memory));
    }
  };

  // windows does not like async_resolve
#ifdef _WIN32
  asio_ns::error_code ec;
  auto it = resolver.resolve(config._host, config._port, ec);
  cb(ec, it);
#else
  // Resolve the host asynchronously into a series of endpoints
  FUERTE_LOG_DEBUG << "scheduled callback to resolve host " << config._host << ":" << config._port << "\n";
  resolver.async_resolve(config._host, config._port, std::move(cb));
#endif
}
}  // namespace

enum class ConnectTimerRole {
  kConnect = 1,
  kReconnect = 2,
};

template <SocketType T>
struct Socket {};

template <>
struct Socket<SocketType::Tcp> {
  Socket(EventLoopService&, asio_ns::io_context& ctx)
      : resolver(ctx), socket(ctx), timer(ctx) {}

  ~Socket() { 
    try {
      this->cancel(); 
    } catch (std::exception const& ex) {
      FUERTE_LOG_ERROR << "caught exception during tcp socket shutdown: " << ex.what() << "\n";
    }
  }

  template <typename F>
  void connect(detail::ConnectionConfiguration const& config, F&& done) {
    resolveConnect(config, resolver, socket, [this, done = std::forward<F>(done)](asio_ns::error_code ec) mutable {
      FUERTE_LOG_DEBUG << "executing tcp connect callback, ec: " << ec.message() << ", canceled: " << this->canceled << "\n";
      if (canceled) {
        // cancel() was already called on this socket
        FUERTE_ASSERT(socket.is_open() == false);
        ec = asio_ns::error::operation_aborted;
      }
      if (!ec) {
        // set TCP_NODELAY option on socket to disable Nagle's algorithm.
        socket.set_option(asio_ns::ip::tcp::no_delay(true), ec);
        if (ec) {
          FUERTE_LOG_ERROR << "error setting no_delay option on socket: " << ec.message() << "\n";
          FUERTE_ASSERT(false);
        }
      }
      done(ec);
    }, [this]() {
      return canceled;
    });
  }

  bool isOpen() const {
    return socket.is_open();
  }

  void rearm() {
    canceled = false;
  }  

  void cancel() {
    canceled = true;
    try {
      timer.cancel();
      resolver.cancel();
      if (socket.is_open()) {  // non-graceful shutdown
        asio_ns::error_code ec;
        socket.close(ec);
      }
    } catch (std::exception const& ex) {
      FUERTE_LOG_ERROR << "caught exception during tcp socket cancelation: " << ex.what() << "\n";
    }
  }

  template <typename F>
  void shutdown(F&& cb) {
    // ec is an out parameter here that is passed to the methods so they
    // can fill in whatever error happened. we ignore it here anyway. we
    // use the ec-variants of the methods here to prevent exceptions.
    asio_ns::error_code ec;
    try {
      timer.cancel(ec);
      if (socket.is_open()) {
        socket.cancel(ec);
        socket.shutdown(asio_ns::ip::tcp::socket::shutdown_both, ec);
        socket.close(ec);
      }
    } catch (std::exception const& ex) {
      // an exception is unlikely to occur here, as we are using the error-code
      // variants of cancel/shutdown/close above 
      FUERTE_LOG_ERROR << "caught exception during tcp socket shutdown: " << ex.what() << "\n";
    }
    std::forward<F>(cb)(ec);
  }

  asio_ns::ip::tcp::resolver resolver;
  asio_ns::ip::tcp::socket socket;
  asio_ns::steady_timer timer;
  ConnectTimerRole connectTimerRole = ConnectTimerRole::kConnect;
  bool canceled = false;
};

template <>
struct Socket<fuerte::SocketType::Ssl> {
  Socket(EventLoopService& loop, asio_ns::io_context& ctx)
    : resolver(ctx), socket(ctx, loop.sslContext()), timer(ctx), ctx(ctx),
      sslContext(loop.sslContext()), cleanupDone(false) {}

  ~Socket() { 
    try {
      this->cancel(); 
    } catch (std::exception const& ex) {
      FUERTE_LOG_ERROR << "caught exception during ssl socket shutdown: " << ex.what() << "\n";
    }
  }

  template <typename F>
  void connect(detail::ConnectionConfiguration const& config, F&& done) {
    bool verify = config._verifyHost;
    resolveConnect(
        config, resolver, socket.next_layer(),
        [=, this](asio_ns::error_code ec) mutable {
          FUERTE_LOG_DEBUG << "executing ssl connect callback, ec: " << ec.message() << ", canceled: " << this->canceled << "\n";
          if (canceled) {
            // cancel() was already called on this socket
            FUERTE_ASSERT(socket.lowest_layer().is_open() == false);
            ec = asio_ns::error::operation_aborted;
          }
          if (ec) {
            done(ec);
            return;
          }

          // Perform SSL handshake and verify the remote host's certificate.
          try {
            // set TCP_NODELAY option on socket to disable Nagle's algorithm.
            socket.next_layer().set_option(asio_ns::ip::tcp::no_delay(true));

            // Set SNI Hostname (many hosts need this to handshake successfully)
            if (!SSL_set_tlsext_host_name(socket.native_handle(), config._host.c_str())) {
              boost::system::error_code ec{
                static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
              done(ec);
              return;
            }

            if (verify) {
              socket.set_verify_mode(asio_ns::ssl::verify_peer);
              socket.set_verify_callback(
                  asio_ns::ssl::rfc2818_verification(config._host));
            } else {
              socket.set_verify_mode(asio_ns::ssl::verify_none);
            }
          } catch (std::bad_alloc const&) {
            // definitely an OOM error
            done(boost::system::errc::make_error_code(boost::system::errc::not_enough_memory));
            return;
          } catch (boost::system::system_error const& exc) {
            // probably not an OOM error, but we don't know what it actually is.
            // there is no code for a generic error that we could use here
            done(exc.code());
            return;
          }
          socket.async_handshake(asio_ns::ssl::stream_base::client,
                                 std::move(done));
        }, [this]() { 
          return canceled; 
        });
  }
  
  bool isOpen() const {
    return socket.lowest_layer().is_open();
  }

  void rearm() {
    // create a new socket and declare it ready
    socket = asio_ns::ssl::stream<asio_ns::ip::tcp::socket>(this->ctx, this->sslContext);
    canceled = false;
  }
  
  void cancel() {
    canceled = true;
    try {
      timer.cancel();
      resolver.cancel();
      if (socket.lowest_layer().is_open()) {  // non-graceful shutdown
        asio_ns::error_code ec;
        socket.lowest_layer().shutdown(asio_ns::ip::tcp::socket::shutdown_both, ec);
        socket.lowest_layer().close(ec);
      }
    } catch (std::exception const& ex) {
      FUERTE_LOG_ERROR << "caught exception during ssl socket cancelation: " << ex.what() << "\n";
    }
  }

  template <typename F>
  void shutdown(F&& cb) {
    // The callback cb plays two roles here:
    //   1. As a callback to be called back.
    //   2. It captures by value a shared_ptr to the connection object of which this
    //      socket is a member. This means that the allocation of the connection and
    //      this of the socket is kept until all asynchronous operations are completed
    //      (or aborted).
    
    // ec is an out parameter here that is passed to the methods so they
    // can fill in whatever error happened. we ignore it here anyway. we
    // use the ec-variants of the methods here to prevent exceptions.
    asio_ns::error_code ec; 

    if (!socket.lowest_layer().is_open()) {
      timer.cancel(ec);
      std::forward<F>(cb)(ec);
      return;
    }
      
    socket.lowest_layer().cancel(ec);
    cleanupDone = false;
    // implicitly cancels any previous timers
    timer.expires_after(std::chrono::seconds(3));

    socket.async_shutdown([cb, this](asio_ns::error_code ec) {
      timer.cancel();
      if (!cleanupDone) {
        socket.lowest_layer().shutdown(asio_ns::ip::tcp::socket::shutdown_both, ec);
        socket.lowest_layer().close(ec);
        cleanupDone = true;
      }
      cb(ec);
    });
    timer.async_wait([cb(std::forward<F>(cb)), this](asio_ns::error_code ec) {
      // Copy in callback such that the connection object is kept alive long
      // enough, please do not delete, although it is not used here!
      if (!ec && !cleanupDone) {
        socket.lowest_layer().shutdown(asio_ns::ip::tcp::socket::shutdown_both, ec);
        socket.lowest_layer().close(ec);
        cleanupDone = true;
      }
    });
  }

  asio_ns::ip::tcp::resolver resolver;
  asio_ns::ssl::stream<asio_ns::ip::tcp::socket> socket;
  asio_ns::steady_timer timer;
  asio_ns::io_context& ctx;
  asio_ns::ssl::context& sslContext;
  std::atomic<bool> cleanupDone;
  ConnectTimerRole connectTimerRole = ConnectTimerRole::kConnect;
  bool canceled = false;
};

#ifdef ASIO_HAS_LOCAL_SOCKETS
template <>
struct Socket<fuerte::SocketType::Unix> {
  Socket(EventLoopService&, asio_ns::io_context& ctx)
      : socket(ctx), timer(ctx) {}

  ~Socket() { 
    canceled = true;
    try {
      this->cancel(); 
    } catch (std::exception const& ex) {
      FUERTE_LOG_ERROR << "caught exception during unix socket shutdown: " << ex.what() << "\n";
    }
  }

  template <typename F>
  void connect(detail::ConnectionConfiguration const& config, F&& done) {
    if (canceled) {
      // cancel() was already called on this socket
      done(asio_ns::error::operation_aborted);
      return;
    }
        
    asio_ns::local::stream_protocol::endpoint ep(config._host);
    socket.async_connect(ep, std::forward<F>(done));
  }
  
  bool isOpen() const {
    return socket.is_open();
  }

  void rearm() {
    canceled = false;
  }
  
  void cancel() {
    canceled = true;
    try {
      timer.cancel();
      if (socket.is_open()) {  // non-graceful shutdown
        asio_ns::error_code ec;
        socket.close(ec);
      }
    } catch (std::exception const& ex) {
      FUERTE_LOG_ERROR << "caught exception during unix socket cancelation: " << ex.what() << "\n";
    }
  }

  template <typename F>
  void shutdown(F&& cb) {
    // ec is an out parameter here that is passed to the methods so they
    // can fill in whatever error happened. we ignore it here anyway. we
    // use the ec-variants of the methods here to prevent exceptions.
    asio_ns::error_code ec; 
    try {
      timer.cancel(ec);
      if (socket.is_open()) {
        socket.cancel(ec);
        socket.shutdown(asio_ns::ip::tcp::socket::shutdown_both, ec);
        socket.close(ec);
      }
    } catch (std::exception const& ex) {
      // an exception is unlikely to occur here, as we are using the error-code
      // variants of cancel/shutdown/close above 
      FUERTE_LOG_ERROR << "caught exception during unix socket shutdown: " << ex.what() << "\n";
    }
    std::forward<F>(cb)(ec);
  }

  asio_ns::local::stream_protocol::socket socket;
  asio_ns::steady_timer timer;
  ConnectTimerRole connectTimerRole = ConnectTimerRole::kConnect;
  bool canceled = false;
};
#endif  // ASIO_HAS_LOCAL_SOCKETS

}}}  // namespace arangodb::fuerte::v1
#endif
