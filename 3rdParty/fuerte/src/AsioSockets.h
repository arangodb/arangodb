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
             done = std::forward<F>(done),
             isAborted = std::forward<IsAbortedCb>(isAborted)](auto ec, auto it) mutable {
    if (isAborted()) {
      ec = asio_ns::error::operation_aborted;
    }

    if (ec) {  // error in address resolver
      FUERTE_LOG_DEBUG << "received error during address resolving: " << ec.message();
      done(ec);
      return;
    }

    try {
      // A successful resolve operation is guaranteed to pass a
      // non-empty range to the handler.
      asio_ns::async_connect(socket, it,
                             [done](auto ec, auto it) mutable {
                               if (ec) {
                                 FUERTE_LOG_DEBUG << "executing async connect callback, error: " << ec.message();
                               } else {
                                 FUERTE_LOG_DEBUG << "executing async connect callback, no error";
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
  FUERTE_LOG_DEBUG << "scheduled callback to resolve host " << config._host << ":" << config._port;
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

  ~Socket() { 
    try {
      this->cancel(); 
    } catch (std::exception const& ex) {
      FUERTE_LOG_ERROR << "caught exception during tcp socket shutdown: " << ex.what();
    }
  }

  template <typename F>
  void connect(detail::ConnectionConfiguration const& config, F&& done) {
    resolveConnect(config, resolver, socket, [this, done = std::forward<F>(done)](asio_ns::error_code ec) mutable {
      FUERTE_LOG_DEBUG << "executing tcp connect callback, ec: " << ec.message() << ", canceled: " << this->canceled;
      if (canceled) {
        // cancel() was already called on this socket
        FUERTE_ASSERT(socket.is_open() == false);
        ec = asio_ns::error::operation_aborted;
      }
      done(ec);
    }, [this]() {
      return canceled;
    });
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
      FUERTE_LOG_ERROR << "caught exception during tcp socket cancelation: " << ex.what();
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
    } catch (std::exception const& ex) {
      // an exception is unlikely to occur here, as we are using the error-code
      // variants of cancel/shutdown/close above 
      FUERTE_LOG_ERROR << "caught exception during tcp socket shutdown: " << ex.what();
    }
    std::forward<F>(cb)(ec);
  }

  asio_ns::ip::tcp::resolver resolver;
  asio_ns::ip::tcp::socket socket;
  asio_ns::steady_timer timer;
  bool canceled = false;
};

template <>
struct Socket<fuerte::SocketType::Ssl> {
  Socket(EventLoopService& loop, asio_ns::io_context& ctx)
    : resolver(ctx), socket(ctx, loop.sslContext()), timer(ctx), cleanupDone(false) {}

  ~Socket() { 
    try {
      this->cancel(); 
    } catch (std::exception const& ex) {
      FUERTE_LOG_ERROR << "caught exception during ssl socket shutdown: " << ex.what();
    }
  }

  template <typename F>
  void connect(detail::ConnectionConfiguration const& config, F&& done) {
    bool verify = config._verifyHost;
    resolveConnect(
        config, resolver, socket.next_layer(),
        [=, this](asio_ns::error_code ec) mutable {
          FUERTE_LOG_DEBUG << "executing ssl connect callback, ec: " << ec.message() << ", canceled: " << this->canceled;
          if (canceled) {
            // cancel() was already called on this socket
            FUERTE_ASSERT(socket.lowest_layer().is_open() == false);
            ec = asio_ns::error::operation_aborted;
          }
          if (ec) {
            done(ec);
            return;
          }

          try {
            // Perform SSL handshake and verify the remote host's certificate.
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
  
  void cancel() {
    canceled = true;
    try {
      timer.cancel();
      resolver.cancel();
      if (socket.lowest_layer().is_open()) {  // non-graceful shutdown
        asio_ns::error_code ec;
        socket.lowest_layer().shutdown(asio_ns::ip::tcp::socket::shutdown_both, ec);
        ec.clear();
        socket.lowest_layer().close(ec);
      }
    } catch (std::exception const& ex) {
      FUERTE_LOG_ERROR << "caught exception during ssl socket cancelation: " << ex.what();
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
    asio_ns::error_code ec;  // prevents exceptions
    socket.lowest_layer().cancel(ec);

    if (!socket.lowest_layer().is_open()) {
      timer.cancel(ec);
      std::forward<F>(cb)(ec);
      return;
    }
    cleanupDone = false;
    socket.async_shutdown([cb(std::forward<F>(cb)), this](auto const& ec) {
      timer.cancel();
#ifndef _WIN32
      if (!cleanupDone && (!ec || ec == asio_ns::error::basic_errors::not_connected)) {
        asio_ns::error_code ec2;
        socket.lowest_layer().shutdown(asio_ns::ip::tcp::socket::shutdown_both, ec2);
        ec2.clear();
        socket.lowest_layer().close(ec2);
        cleanupDone = true;
      }
#endif
      cb(ec);
    });
    timer.expires_from_now(std::chrono::seconds(3));
    timer.async_wait([cb, this](asio_ns::error_code ec) {
      // Copy in callback such that the connection object is kept alive long
      // enough, please do not delete, although it is not used here!
      if (!cleanupDone && !ec) {
        socket.lowest_layer().shutdown(asio_ns::ip::tcp::socket::shutdown_both, ec);
        ec.clear();
        socket.lowest_layer().close(ec);
        cleanupDone = true;
      }
    });
  }

  asio_ns::ip::tcp::resolver resolver;
  asio_ns::ssl::stream<asio_ns::ip::tcp::socket> socket;
  asio_ns::steady_timer timer;
  std::atomic<bool> cleanupDone;
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
      FUERTE_LOG_ERROR << "caught exception during unix socket shutdown: " << ex.what();
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
  
  void cancel() {
    canceled = true;
    try {
      timer.cancel();
      if (socket.is_open()) {  // non-graceful shutdown
        asio_ns::error_code ec;
        socket.close(ec);
      }
    } catch (std::exception const& ex) {
      FUERTE_LOG_ERROR << "caught exception during unix socket cancelation: " << ex.what();
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
  bool canceled = false;
};
#endif  // ASIO_HAS_LOCAL_SOCKETS

}}}  // namespace arangodb::fuerte::v1
#endif
