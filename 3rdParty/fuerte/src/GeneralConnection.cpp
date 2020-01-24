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

#include "debugging.h"

namespace arangodb { namespace fuerte {

template <SocketType ST>
GeneralConnection<ST>::GeneralConnection(
    EventLoopService& loop, detail::ConnectionConfiguration const& config)
    : Connection(config),
      _io_context(loop.nextIOContext()),
      _proto(loop, *_io_context),
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
      drainQueue(Error::Canceled);
      shutdownConnection(Error::Canceled);
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
      auto* me = static_cast<GeneralConnection<ST>*>(self.get());
      me->tryConnect(me->_config._maxConnectRetries, std::chrono::steady_clock::now());
    };
    asio_ns::post(*this->_io_context, std::move(cb));
  }
}

// shutdown the connection and cancel all pending messages.
template <SocketType ST>
void GeneralConnection<ST>::shutdownConnection(const Error err, std::string const& msg,
                                               bool mayRestart) {
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
  
  // clear buffer of received messages
  _receiveBuffer.consume(_receiveBuffer.size());
  abortOngoingRequests(err);

  _proto.shutdown([=, self(shared_from_this())](auto const& ec) {
    if (mayRestart && requestsLeft() > 0) {
      startConnection();
      // cancel was triggered externally, no need to notify
      onFailure(err, msg);
    }
  });  // Close socket
}

// Connect with a given number of retries
template <SocketType ST>
void GeneralConnection<ST>::tryConnect(unsigned retries, std::chrono::steady_clock::time_point start) {
  FUERTE_ASSERT(_state.load() == Connection::State::Connecting);
  FUERTE_LOG_DEBUG << "tryConnect (" << retries << ") this=" << this << "\n";

  auto self = Connection::shared_from_this();
  if (_config._connectTimeout.count() > 0) {
    _proto.timer.expires_after(_config._connectTimeout);
    _proto.timer.async_wait([self, this](asio_ns::error_code const& ec) {
      if (!ec) {
        _proto.cancel();
        shutdownConnection(Error::CouldNotConnect);
      }
    });
  } else {
    _proto.timer.cancel();
  }
  
  _proto.connect(_config, [self, this, retries, start](auto const& ec) {
    _proto.timer.cancel();
    if (!ec) {
      finishConnect();
      return;
    }
    FUERTE_LOG_DEBUG << "connecting failed: " << ec.message() << "\n";
    if (retries > 0 && ec != asio_ns::error::operation_aborted) {
      auto const now = std::chrono::steady_clock::now();
      if ((now - start) < _config._connectTimeout) {
        _proto.timer.expires_after(std::min(
              std::chrono::duration_cast<std::chrono::milliseconds>(_config._connectRetryPause), 
              std::chrono::duration_cast<std::chrono::milliseconds>(_config._connectTimeout - (now - start))));
        _proto.timer.async_wait([self, this, retries, start](auto ec) {
          if (!ec) {
            tryConnect(retries - 1, start);
          }
        });
        return;
      }
    }
    // error
    _state.store(Connection::State::Failed, std::memory_order_release);
    drainQueue(Error::CouldNotConnect);
    shutdownConnection(Error::CouldNotConnect,
                       "connecting failed: " + ec.message());
  });
}

template <SocketType ST>
void GeneralConnection<ST>::restartConnection(const Error err) {
  // restarting needs to be an exclusive operation
  Connection::State exp = Connection::State::Connected;
  if (_state.compare_exchange_strong(exp, Connection::State::Disconnected)) {
    FUERTE_LOG_DEBUG << "restartConnection this=" << this << "\n";
    // Terminate connection, restarts if required
    shutdownConnection(err, /*msg*/ "", /*mayRestart*/err != Error::Canceled);
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
    auto& me = static_cast<GeneralConnection<ST>&>(*self);
    me._receiveBuffer.commit(nread);
    me.asyncReadCallback(ec);
  });
}

template class arangodb::fuerte::GeneralConnection<SocketType::Tcp>;
template class arangodb::fuerte::GeneralConnection<SocketType::Ssl>;
#ifdef ASIO_HAS_LOCAL_SOCKETS
template class arangodb::fuerte::GeneralConnection<SocketType::Unix>;
#endif

}}  // namespace arangodb::fuerte
