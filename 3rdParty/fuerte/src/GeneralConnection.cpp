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
  asio_ns::post(*_io_context, [self(weak_from_this()), this] {
    auto s = self.lock();
    if (s) {
      _state.store(State::Failed);
      drainQueue(Error::Canceled);
      shutdownConnection(Error::Canceled);
    }
  });
}

// Lease this connection (cancel idle alarm)
template <SocketType ST>
bool GeneralConnection<ST>::lease() {
  return true;   // this is a noop, derived classes can override
}

// Unlease this connection (give lease back), necessary if no sendRequest was called
template <SocketType ST>
void GeneralConnection<ST>::unlease() {
  // this is a noop, derived classes can override
}

// Activate this connection.
template <SocketType ST>
void GeneralConnection<ST>::start() {
  asio_ns::post(*this->_io_context, [self = Connection::shared_from_this()] {
    static_cast<GeneralConnection<ST>&>(*self).startConnection();
  });
}

template <SocketType ST>
void GeneralConnection<ST>::startConnection() {
  // start connecting only if state is disconnected
  Connection::State exp = Connection::State::Disconnected;
  if (_state.compare_exchange_strong(exp, Connection::State::Connecting)) {
    FUERTE_LOG_DEBUG << "startConnection: this=" << this << "\n";
    FUERTE_ASSERT(_config._maxConnectRetries > 0);
    tryConnect(_config._maxConnectRetries, std::chrono::steady_clock::now(),
               asio_ns::error_code());
  } else {
    FUERTE_LOG_DEBUG << "startConnection: this=" << this << " found unexpected state "
      << static_cast<int>(exp) << " not equal to 'Disconnected'";
    FUERTE_ASSERT(false);
  }
}

// shutdown the connection and cancel all pending messages.
template <SocketType ST>
void GeneralConnection<ST>::shutdownConnection(const Error err, std::string const& msg,
                                               bool mayRestart) {
  FUERTE_LOG_DEBUG << "shutdownConnection: err = '" << to_string(err) << "' ";
  if (!msg.empty()) {
    FUERTE_LOG_DEBUG << ", msg = '" << msg<< "' ";
  }
  FUERTE_LOG_DEBUG<< "this=" << this << "\n";

  bool mustShutdown = false;

  auto state = _state.load();
  if (state != Connection::State::Failed) {
    state = Connection::State::Disconnected;
    // hack to fix SSL streams
    if constexpr (ST == SocketType::Ssl) {
      state = Connection::State::Failed;
      mayRestart = false;
      mustShutdown = true;
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
  if (mustShutdown) {
    terminateActivity();
  }
}

// Connect with a given number of retries
template <SocketType ST>
void GeneralConnection<ST>::tryConnect(unsigned retries,
                                       std::chrono::steady_clock::time_point start,
                                       asio_ns::error_code const& ec) {
  if (_state.load() != Connection::State::Connecting) {
    return;
  }

  if (retries == 0) {
    _state.store(Connection::State::Failed, std::memory_order_release);
    drainQueue(Error::CouldNotConnect);
    std::string msg("connecting failed: '");
    msg.append((ec != asio_ns::error::operation_aborted) ? ec.message() : "timeout");
    msg.push_back('\'');
    shutdownConnection(Error::CouldNotConnect, msg);
    return;
  }
  
  FUERTE_LOG_DEBUG << "tryConnect (" << retries << ") this=" << this << "\n";
  auto self = Connection::shared_from_this();

  _proto.timer.expires_at(start + _config._connectTimeout);
  _proto.timer.async_wait([self](asio_ns::error_code const& ec) {
    if (!ec && self->state() == Connection::State::Connecting) {
      // the connect handler below gets 'operation_aborted' error
      static_cast<GeneralConnection<ST>&>(*self)._proto.cancel();
    }
  });
  
  _proto.connect(_config, [self, start, retries](auto const& ec) mutable {
    GeneralConnection<ST>& me = static_cast<GeneralConnection<ST>&>(*self);
    me._proto.timer.cancel();
    // Note that is is possible that the alarm has already gone off, in which
    // case its closure might already be queued right after ourselves! However,
    // we now quickly set the state to `Connected` in which case the closure will
    // no longer shut down the socket and ruin our success.
    if (!ec) {
      me.finishConnect();
      return;
    }
    FUERTE_LOG_DEBUG << "connecting failed: " << ec.message() << "\n";
    if (retries > 0 && ec != asio_ns::error::operation_aborted) {
      auto end = std::min(std::chrono::steady_clock::now() + me._config._connectRetryPause,
                          start + me._config._connectTimeout);
      me._proto.timer.expires_at(end);
      me._proto.timer.async_wait([self(std::move(self)), start, retries](auto ec) mutable {
        GeneralConnection<ST>& me = static_cast<GeneralConnection<ST>&>(*self);
        me.tryConnect(!ec ? retries - 1 : 0, start, ec);
      });
    } else {
      me.tryConnect(0, start, ec); // <- handles errors
    }
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
