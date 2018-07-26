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

#include "AsioConnection.h"

#include <fuerte/FuerteLogger.h>

#include "Basics/cpu-relax.h"
#include "http.h"
#include "vst.h"

namespace arangodb { namespace fuerte { inline namespace v1 {
using bt = ::asio_ns::ip::tcp;
using be = ::asio_ns::ip::tcp::endpoint;

template <typename T>
AsioConnection<T>::AsioConnection(
    std::shared_ptr<asio_ns::io_context> const& ctx,
    detail::ConnectionConfiguration const& config)
    : Connection(config),
      _io_context(ctx),
      _resolver(*ctx),
      _socket(nullptr),
      _timeout(*ctx),
      _sslContext(nullptr),
      _sslSocket(nullptr),
      _state(Connection::State::Disconnected),
      _loopState(0),
      _writeQueue(1024) {}

// Deconstruct.
template <typename T>
AsioConnection<T>::~AsioConnection() {
  _resolver.cancel();
  if (!_messageStore.empty()) {
    FUERTE_LOG_TRACE << "DESTROYING CONNECTION WITH: "
                         << _messageStore.size() << " outstanding requests!"
                         << std::endl;
  }
  shutdownConnection(ErrorCondition::Canceled);
}

// Activate this connection.
template <typename T>
void AsioConnection<T>::startConnection() {
  startResolveHost(); // checks _state
}

// resolve the host into a series of endpoints
template <typename T>
void AsioConnection<T>::startResolveHost() {
  // socket must be empty before. Check that
  assert(!_socket);
  assert(!_sslSocket);
  
  Connection::State exp = Connection::State::Disconnected;
  if (!_state.compare_exchange_strong(exp, Connection::State::Connecting)) {
    FUERTE_LOG_ERROR << "already resolving endpoint";
    return;
  }
  
  // Resolve the host asynchronous.
  auto self = shared_from_this();
  _resolver.async_resolve(
      {_configuration._host, _configuration._port},
      [this, self](const asio_ns::error_code& error,
                   bt::resolver::iterator iterator) {
        if (error) {
          FUERTE_LOG_ERROR << "resolve failed: error=" << error << std::endl;
          onFailure(errorToInt(ErrorCondition::CouldNotConnect),
                    "resolved failed: error" + error.message());
        } else {
          FUERTE_LOG_CALLBACKS << "resolve succeeded" << std::endl;
          _endpoints = iterator;
          if (_endpoints == bt::resolver::iterator()) {
            FUERTE_LOG_ERROR << "unable to resolve endpoints" << std::endl;
            onFailure(errorToInt(ErrorCondition::CouldNotConnect),
                      "unable to resolve endpoints");
          } else {
            initSocket();
          }
        }
      });
}

// CONNECT RECONNECT //////////////////////////////////////////////////////////

template <typename T>
void AsioConnection<T>::initSocket() {
  std::lock_guard<std::mutex> lock(_socket_mutex);
  // socket must be empty before. Check that
  assert(!_socket);
  assert(!_sslSocket);

  FUERTE_LOG_CALLBACKS << "begin init" << std::endl;
  _socket.reset(new bt::socket(*_io_context));
  if (_configuration._socketType == SocketType::Ssl) {
    _sslContext.reset(new asio_ns::ssl::context(
        asio_ns::ssl::context::method::sslv23));
    _sslContext->set_default_verify_paths();
    _sslSocket.reset(
        new asio_ns::ssl::stream<bt::socket&>(*_socket, *_sslContext));
  }

  auto endpoints = _endpoints;  // copy as connect modifies iterator
  startConnect(endpoints);
}

// close the TCP & SSL socket.
template <typename T>
void AsioConnection<T>::shutdownSocket() {
  std::lock_guard<std::mutex> lock(_socket_mutex);

  FUERTE_LOG_CALLBACKS << "begin shutdown socket\n";

  asio_ns::error_code error;
  if (_sslSocket.get() != nullptr) {
    _sslSocket->shutdown(error);
  }
  if (_socket.get() != nullptr) {
    _socket->cancel();
    _socket->shutdown(bt::socket::shutdown_both, error);
    _socket->close(error);
    _socket.reset();
  }
  _sslSocket.reset();
}

// shutdown the connection and cancel all pending messages.
template <typename T>
void AsioConnection<T>::shutdownConnection(const ErrorCondition ec) {
  FUERTE_LOG_CALLBACKS << "shutdownConnection\n";
  
  _state.store(State::Disconnected, std::memory_order_release);
  
  // cancel timeouts
  _timeout.cancel();
  
  // Close socket
  shutdownSocket();
  
  // Stop the read & write loop
  stopIOLoops();
  
  // Cancel all items and remove them from the message store.
  _messageStore.cancelAll(ec);
  
  // clear buffer of received messages
  _receiveBuffer.consume(_receiveBuffer.size());
}

template <typename T>
void AsioConnection<T>::restartConnection(const ErrorCondition error) {
  // Read & write loop must have been reset by now

  FUERTE_LOG_CALLBACKS << "restartConnection" << std::endl;
  
  // only restart connection if it was connected previously
  if (_state.load(std::memory_order_acquire) == Connection::State::Connected) {
    shutdownConnection(error); // Terminate connection
    startResolveHost(); // will check state
  }
}

// Thread-Safe: reset io loop flags
template <typename T>
void AsioConnection<T>::stopIOLoops() {
  uint32_t state = _loopState.load(std::memory_order_seq_cst);
  while (state & LOOP_FLAGS) {
    if (_loopState.compare_exchange_weak(state, state & ~LOOP_FLAGS,
                                         std::memory_order_seq_cst)) {
      FUERTE_LOG_TRACE << "stopIOLoops: stopped" << std::endl;
      return;  // we turned flag off while nothin was queued
    }
    cpu_relax();
  }
}

// ------------------------------------
// Creating a connection
// ------------------------------------

// try to open the socket connection to the first endpoint.
template <typename T>
void AsioConnection<T>::startConnect(bt::resolver::iterator endpointItr) {
  if (endpointItr != asio_ns::ip::tcp::resolver::iterator()) {
    FUERTE_LOG_CALLBACKS << "trying to connect to: " << endpointItr->endpoint()
                         << "..." << std::endl;

    auto self = shared_from_this();
    // Set a deadline for the connect operation.
    _timeout.expires_after(_configuration._connectionTimeout);
    _timeout.async_wait([this, self] (asio_ns::error_code const ec) {
      if (!ec) { // timeout
        shutdownConnection(ErrorCondition::CouldNotConnect);
      }
    });

    // Start the asynchronous connect operation.
    asio_ns::async_connect(
        *_socket, endpointItr,
        std::bind(&AsioConnection<T>::asyncConnectCallback,
                  std::static_pointer_cast<AsioConnection>(self),
                  std::placeholders::_1, endpointItr));
  }
}

// callback handler for async_callback (called in startConnect).
template <typename T>
void AsioConnection<T>::asyncConnectCallback(
   asio_ns::error_code const& ec, bt::resolver::iterator endpointItr) {
  _timeout.cancel();
  
  if (ec) {
    // Connection failed
    FUERTE_LOG_ERROR << ec.message() << std::endl;
    shutdownConnection(ErrorCondition::CouldNotConnect);
    if (endpointItr == bt::resolver::iterator()) {
      FUERTE_LOG_CALLBACKS << "no further endpoint" << std::endl;
    }
    onFailure(errorToInt(ErrorCondition::CouldNotConnect),
              "unable to connect -- " + ec.message());
    _messageStore.cancelAll(ErrorCondition::CouldNotConnect);
  } else {
    // Connection established
    FUERTE_LOG_CALLBACKS << "TCP socket connected" << std::endl;
    if (_configuration._socketType == SocketType::Ssl) {
      startSSLHandshake();
    } else {
      finishInitialization();
    }
  }
}

// start intiating an SSL connection (on top of an established TCP socket)
template <typename T>
void AsioConnection<T>::startSSLHandshake() {
  if (_configuration._socketType != SocketType::Ssl) {
    finishInitialization();
  }
  // https://www.boost.org/doc/libs/1_67_0/doc/html/boost_asio/overview/ssl.html
  // Perform SSL handshake and verify the remote host's certificate.
  _sslSocket->lowest_layer().set_option(asio_ns::ip::tcp::no_delay(true));
  _sslSocket->set_verify_mode(asio_ns::ssl::verify_peer);
  _sslSocket->set_verify_callback(
      asio_ns::ssl::rfc2818_verification(_configuration._host));

  FUERTE_LOG_CALLBACKS << "starting ssl handshake " << std::endl;
  auto self = shared_from_this();
  _sslSocket->async_handshake(
      asio_ns::ssl::stream_base::client,
      [this, self](asio_ns::error_code const& error) {
        if (error) {
          FUERTE_LOG_ERROR << error.message() << std::endl;
          shutdownSocket();
          onFailure(
              errorToInt(ErrorCondition::CouldNotConnect),
              "unable to perform ssl handshake: error=" + error.message());
          _messageStore.cancelAll(ErrorCondition::CouldNotConnect);
        } else {
          FUERTE_LOG_CALLBACKS << "ssl handshake done" << std::endl;
          finishInitialization();
        }
      });
}

// ------------------------------------
// Writing data
// ------------------------------------

// Thread-Safe: queue a new request
template <typename T>
uint32_t AsioConnection<T>::queueRequest(std::unique_ptr<T> item) {
  if (!_writeQueue.push(item.get())) {
    FUERTE_LOG_ERROR << "connection queue capactiy exceeded" << std::endl;
    throw std::length_error("connection queue capactiy exceeded");
  }
  item.release();
  // WRITE_LOOP_ACTIVE, READ_LOOP_ACTIVE are synchronized via cmpxchg
  return _loopState.fetch_add(WRITE_LOOP_QUEUE_INC, std::memory_order_seq_cst);
}

// writes data from task queue to network using asio_ns::async_write
template <typename T>
void AsioConnection<T>::asyncWriteNextRequest() {
  FUERTE_LOG_TRACE << "asyncWrite: preparing to send next" << std::endl;
  if (_state.load(std::memory_order_acquire) != State::Connected) {
    FUERTE_LOG_TRACE << "asyncReadSome: permanent failure\n";
    stopIOLoops();  // will set the flag
    return;
  }

  // reduce queue length and check active flag
#ifndef NDEBUG
  uint32_t state =
#endif
    _loopState.fetch_sub(WRITE_LOOP_QUEUE_INC, std::memory_order_acquire);
  assert((state & WRITE_LOOP_QUEUE_MASK) > 0);

  T* ptr = nullptr;
#ifndef NDEBUG
  bool success =
#endif
    _writeQueue.pop(ptr);
  assert(success); // should never fail here
  std::shared_ptr<T> item(ptr);

  // we stop the write-loop if we stopped it ourselves.
  auto self = shared_from_this();
  auto cb = [this, self, item](asio_ns::error_code const& ec, std::size_t transferred) {
    asyncWriteCallback(ec, transferred, std::move(item));
  };
  auto buffers = this->prepareRequest(item);
  if (_configuration._socketType == SocketType::Ssl) {
    asio_ns::async_write(*_sslSocket, buffers, cb);
    /*asio_ns::async_write(
        *_sslSocket, buffers,
        std::bind(&AsioConnection<T>::asyncWriteCallback,
                  std::static_pointer_cast<AsioConnection<T>>(self),
                  std::placeholders::_1, std::placeholders::_2,
                  std::move(item)));*/
  } else {
    asio_ns::async_write(*_socket, buffers, cb);
    /*asio_ns::async_write(
        *_socket, buffers,
        std::bind(&AsioConnection<T>::asyncWriteCallback,
                  std::static_pointer_cast<AsioConnection<T>>(self),
                  std::placeholders::_1, std::placeholders::_2,
                  std::move(item)));*/
  }

  FUERTE_LOG_TRACE << "asyncWrite: done" << std::endl;
}

// ------------------------------------
// Reading data
// ------------------------------------

// asyncReadSome reads the next bytes from the server.
template <typename T>
void AsioConnection<T>::asyncReadSome() {
  FUERTE_LOG_TRACE << "asyncReadSome: this=" << this << std::endl;

  if (!(_loopState.load(std::memory_order_seq_cst) & READ_LOOP_ACTIVE)) {
    FUERTE_LOG_TRACE << "asyncReadSome: read-loop was stopped\n";
    return;  // just stop
  }
  if (_state.load(std::memory_order_acquire) != State::Connected) {
    FUERTE_LOG_TRACE << "asyncReadSome: permanent failure\n";
    stopIOLoops();  // will set the flags
    return;
  }

  // Start reading data from the network.
  FUERTE_LOG_CALLBACKS << "r";
#if ENABLE_FUERTE_LOG_CALLBACKS > 0
  std::cout << "_messageMap = " << _messageStore.keys() << std::endl;
#endif

  assert(_socket);
  auto self = shared_from_this();
  auto cb = [this, self](asio_ns::error_code const& ec, size_t transferred) {
    // received data is "committed" from output sequence to input sequence
    _receiveBuffer.commit(transferred);
    asyncReadCallback(ec, transferred);
  };

  // reserve 32kB in output buffer
  auto mutableBuff = _receiveBuffer.prepare(READ_BLOCK_SIZE);
  if (_configuration._socketType == SocketType::Ssl) {
    _sslSocket->async_read_some(mutableBuff, std::move(cb));
  } else {
    _socket->async_read_some(mutableBuff, std::move(cb));
  }

  FUERTE_LOG_TRACE << "asyncReadSome: done" << std::endl;
}

template class arangodb::fuerte::v1::AsioConnection<
    arangodb::fuerte::v1::vst::RequestItem>;
template class arangodb::fuerte::v1::AsioConnection<
    arangodb::fuerte::v1::http::RequestItem>;
}}}  // namespace arangodb::fuerte::v1
