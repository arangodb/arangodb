////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include "GeneralConnection.h"

#include <fuerte/FuerteLogger.h>

namespace arangodb { namespace fuerte {

template <SocketType ST>
GeneralConnection<ST>::GeneralConnection(
    EventLoopService& loop, detail::ConnectionConfiguration const& config)
    : Connection(config),
      _io_context(loop.nextIOContext()),
      _proto(loop, *_io_context),
      _timeout(*_io_context),
      _state(Connection::State::Disconnected),
      _numQueued(0) {}

/// @brief cancel the connection, unusable afterwards
template <SocketType ST>
void GeneralConnection<ST>::cancel() {
  FUERTE_LOG_DEBUG << "cancel: this=" << this << "\n";
  _state.store(State::Failed);
  asio_ns::post(*_io_context, [self(weak_from_this()), this] {
    auto s = self.lock();
    if (s) {
      shutdownConnection(Error::Canceled);
      drainQueue(Error::Canceled);
    }
  });
}

// Activate this connection.
template <SocketType ST>
void GeneralConnection<ST>::startConnection() {
  // start connecting only if state is disconnected
  Connection::State exp = Connection::State::Disconnected;
  if (_state.compare_exchange_strong(exp, Connection::State::Connecting)) {
    FUERTE_LOG_DEBUG << "startConnection: this=" << this << "\n";
    auto cb = [self = Connection::shared_from_this()] {
      auto* thisPtr = static_cast<GeneralConnection<ST>*>(self.get());
      thisPtr->tryConnect(thisPtr->_config._maxConnectRetries);
    };
    asio_ns::post(*this->_io_context, std::move(cb));
  }
}

// shutdown the connection and cancel all pending messages.
template <SocketType ST>
void GeneralConnection<ST>::shutdownConnection(const Error err,
                                               std::string const& msg) {
  FUERTE_LOG_DEBUG << "shutdownConnection: '" << msg
                   << "' this=" << this << "\n";

  auto state = _state.load();
  if (state != Connection::State::Failed) {
    state = Connection::State::Disconnected;
    // hack to fix SSL streams
    if constexpr (ST == SocketType::Ssl) {
      state = Connection::State::Failed;
    }
    _state.store(state);
  }

  asio_ns::error_code ec;
  _timeout.cancel(ec);
  if (ec) {
    FUERTE_LOG_ERROR << "error on timeout cancel: " << ec.message();
  }

  try {
    _proto.shutdown();  // Close socket
  } catch (...) { }

  abortOngoingRequests(err);
  
  // cancel was triggered externally, no need to notify
  if (err != Error::Canceled) {
    onFailure(err, msg);
  }

  // clear buffer of received messages
  _receiveBuffer.consume(_receiveBuffer.size());
}

// Connect with a given number of retries
template <SocketType ST>
void GeneralConnection<ST>::tryConnect(unsigned retries) {
  assert(_state.load() == Connection::State::Connecting);
  FUERTE_LOG_DEBUG << "tryConnect (" << retries << ") this=" << this << "\n";

  asio_ns::error_code ec;
  _timeout.cancel(ec);
  if (ec) {
    FUERTE_LOG_ERROR << "error on timeout cancel: " << ec.message();
  }

  auto self = Connection::shared_from_this();
  if (_config._connectTimeout.count() > 0) {
    _timeout.expires_after(_config._connectTimeout);
    _timeout.async_wait([self, this](asio_ns::error_code const& ec) {
      if (!ec) { // someone else will retry
        _proto.shutdown();
      }
    });
  }
  
  _proto.connect(_config, [self, this, retries](auto const& ec) {
    _timeout.cancel();
    if (!ec) {
      finishConnect();
      return;
    }
    FUERTE_LOG_DEBUG << "connecting failed: " << ec.message() << "\n";
    if (retries > 0 && ec != asio_ns::error::operation_aborted) {
      tryConnect(retries - 1);
    } else {
      _state.store(Connection::State::Failed, std::memory_order_release);
      drainQueue(Error::CouldNotConnect);
      shutdownConnection(Error::CouldNotConnect,
                         "connecting failed: " + ec.message());
    }
  });
}

template <SocketType ST>
void GeneralConnection<ST>::restartConnection(const Error error) {
  // restarting needs to be an exclusive operation
  Connection::State exp = Connection::State::Connected;
  if (_state.compare_exchange_strong(exp, Connection::State::Disconnected)) {
    FUERTE_LOG_DEBUG << "restartConnection this=" << this << "\n";
    shutdownConnection(error);  // Terminate connection
    if (error != Error::Canceled && requestsLeft() > 0) {
      startConnection();  // switches state to Conneccting
    }
  }
}

// asyncReadSome reads the next bytes from the server.
template <SocketType ST>
void GeneralConnection<ST>::asyncReadSome() {
  FUERTE_LOG_TRACE << "asyncReadSome: this=" << this << "\n";

  // TODO perform a non-blocking read

  // Start reading data from the network.

  // reserve 32kB in output buffer
  auto mutableBuff = _receiveBuffer.prepare(READ_BLOCK_SIZE);
  
  _proto.socket.async_read_some(mutableBuff, [self = shared_from_this()]
                                 (auto const& ec, size_t nread) {
    FUERTE_LOG_TRACE << "received " << nread << " bytes\n";
    
    // received data is "committed" from output sequence to input sequence
    auto* thisPtr = static_cast<GeneralConnection<ST>*>(self.get());
    thisPtr->_receiveBuffer.commit(nread);
    thisPtr->asyncReadCallback(ec);
  });
}

template class arangodb::fuerte::GeneralConnection<SocketType::Tcp>;
template class arangodb::fuerte::GeneralConnection<SocketType::Ssl>;
#ifdef ASIO_HAS_LOCAL_SOCKETS
template class arangodb::fuerte::GeneralConnection<SocketType::Unix>;
#endif

}}  // namespace arangodb::fuerte
