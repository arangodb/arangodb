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

#pragma once
#ifndef ARANGO_CXX_DRIVER_GENERAL_CONNECTION_H
#define ARANGO_CXX_DRIVER_GENERAL_CONNECTION_H 1

#include <fuerte/connection.h>
#include <fuerte/types.h>

#include <boost/lockfree/queue.hpp>

#include "AsioSockets.h"

#include "debugging.h"

namespace arangodb { namespace fuerte {

// Reason for timeout:
enum class TimeoutType : uint8_t {
  Idle = 0,
  ReadWrite = 1
};

// GeneralConnection implements shared code between all protocol implementations
template <SocketType ST, typename RT>
class GeneralConnection : public fuerte::Connection {
 public:
  explicit GeneralConnection(EventLoopService& loop,
                             detail::ConnectionConfiguration const& config)
  : Connection(config),
       _io_context(loop.nextIOContext()),
       _proto(loop, *_io_context) {}
  
  virtual ~GeneralConnection() noexcept {
    _state.store(Connection::State::Closed);
    terminateActivity();
  }

  /// @brief connection state
  Connection::State state() const override final {
    return _state.load(std::memory_order_acquire);
  }

  /// The following public methods can be called from any thread:

  /// @brief cancel the connection, unusable afterwards
  void cancel() override {
    FUERTE_LOG_DEBUG << "cancel: this=" << this << "\n";
    asio_ns::post(*_io_context, [self(weak_from_this()), this] {
      auto s = self.lock();
      if (s) {
        _state.store(State::Closed);
        shutdownConnection(Error::Canceled);
      }
    });
  }

  /// All protected or private methods below here must only be called on the
  /// IO thread.
 protected:
  
  void startConnection() {
    // start connecting only if state is disconnected
    Connection::State exp = Connection::State::Created;
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
  
  // shutdown connection, cancel async operations
  void shutdownConnection(fuerte::Error err, std::string const& msg = "") {
    // keepalive timeout may have expired
    if (_timeoutOnReadWrite && err == fuerte::Error::Canceled) {
      err = fuerte::Error::Timeout;
    }
    
    FUERTE_LOG_DEBUG << "shutdownConnection: err = '" << to_string(err) << "' ";
    if (!msg.empty()) {
      FUERTE_LOG_DEBUG << ", msg = '" << msg<< "' ";
    }
    FUERTE_LOG_DEBUG<< "this=" << this << "\n";

    _state.store(Connection::State::Closed);
    
    abortOngoingRequests(err);
    drainQueue(err);

    _proto.shutdown([=, self(shared_from_this())](auto const& ec) {
      terminateActivity();
      onFailure(err, msg);
    });  // Close socket
  }

  // Call on IO-Thread: read from socket
  void asyncReadSome() {
    FUERTE_LOG_TRACE << "asyncReadSome: this=" << this << "\n";

    // TODO perform a non-blocking read on linux

    // Start reading data from the network.

    // reserve 32kB in output buffer
    auto mutableBuff = _receiveBuffer.prepare(READ_BLOCK_SIZE);
    
    _reading = true;
    _proto.socket.async_read_some(mutableBuff, [self = shared_from_this()]
                                   (auto const& ec, size_t nread) {
      FUERTE_LOG_TRACE << "received " << nread << " bytes\n";
      
      // received data is "committed" from output sequence to input sequence
      auto& me = static_cast<GeneralConnection<ST, RT>&>(*self);
      me._reading = false;
      me._receiveBuffer.commit(nread);
      me.asyncReadCallback(ec);
    });
  }
  
  /// abort all requests lingering in the queue
  void drainQueue(const fuerte::Error ec) {
    RT* item = nullptr;
    while (_queue.pop(item)) {
      FUERTE_ASSERT(item);
      std::unique_ptr<RT> guard(item);
      uint32_t q = this->_numQueued.fetch_sub(1, std::memory_order_relaxed);
      FUERTE_ASSERT(q > 0);
      guard->invokeOnError(ec);
    }
  }
  
  /// Set timeout accordingly
  void setTimeout() {
    bool isIdle = false;
    auto const timepoint = getTimeout(isIdle);
    if (timepoint.time_since_epoch().count() == 0) {
      this->_proto.timer.cancel();
      return; // danger zone
    }

    _timeoutOnReadWrite = false;

    // expires_after cancels pending ops
    this->_proto.timer.expires_at(timepoint);
    this->_proto.timer.async_wait(
        [isIdle, self = Connection::weak_from_this()](auto const& ec) {
      std::shared_ptr<Connection> s;
      if (ec || !(s = self.lock())) {  // was canceled / deallocated
        return;
      }
      auto& me = static_cast<GeneralConnection<ST, RT>&>(*s);
      me.abortExpiredRequests();
      
      if (!isIdle && (me._writing ||  me._reading)) {
        me._timeoutOnReadWrite = true;
        me._proto.cancel();
        // We simply cancel all ongoing asynchronous operations,
        // completion handlers will do the rest.
        return;
      } else if (isIdle) {
        me.handleIdleTimeout();
      }
      // In all other cases we do nothing, since we have been posted to the
      // iocontext but the thing we should be timing out has already completed.
    });
  }
  
  void activate() {
    Connection::State state = this->_state.load();
    FUERTE_ASSERT(state != Connection::State::Connecting);
    if (state == Connection::State::Connected) {
      FUERTE_LOG_HTTPTRACE << "activate: connected\n";
      this->doWrite();
    } else if (state == Connection::State::Created) {
      FUERTE_LOG_HTTPTRACE << "activate: not connected\n";
      this->startConnection();
    } else if (state == Connection::State::Closed) {
      FUERTE_LOG_ERROR << "activate: queued request on failed connection\n";
      this->drainQueue(fuerte::Error::ConnectionClosed);
      this->_active.store(false);    // No more activity from our side
    }
    // If the state is `Connecting`, we do not need to do anything, this can
    // happen if this `activate` was posted when the connection was still
    // `Disconnected`, but in the meantime the connect has started.
  }
  
  /// The following is called when the connection is permanently failed. It is
  /// used to shut down any activity in the derived classes in a way that avoids
  /// sleeping barbers
  void terminateActivity() {
    // Usually, we are `active == true` when we get here, except for the following
    // case: If we are inactive but the connection is still open and then the
    // idle timeout goes off, then we shutdownConnection and in the TLS case we
    // call this method here. In this case it is OK to simply proceed, therefore
    // no assertion on `_active`.
    FUERTE_ASSERT(this->_state.load() == Connection::State::Closed);
    FUERTE_LOG_HTTPTRACE << "terminateActivity: active=true, this=" << this << "\n";
    while (true) {
      this->drainQueue(Error::Canceled);
      this->_active.store(false);
      // Now need to check again:
      if (this->_queue.empty()) {
        return;
      }
      this->_active.store(true);
    }
  }
  
protected:
  
  virtual void finishConnect() = 0;

  /// perform writes
  virtual void doWrite() = 0;
  
  // called by the async_read handler (called from IO thread)
  virtual void asyncReadCallback(asio_ns::error_code const&) = 0;

  /// abort ongoing / unfinished requests (locally)
  virtual void abortOngoingRequests(const fuerte::Error) = 0;
  // abort all expired requests
  virtual void abortExpiredRequests() = 0;
  // calculate smallest timeout
  virtual std::chrono::steady_clock::time_point getTimeout(bool& isIdle) = 0;
  // subclasses may override this for a gracefuly shutdown
  
  virtual void handleIdleTimeout() {
    if (!_active.load() && _state.load() == Connection::State::Connected) {
      shutdownConnection(Error::CloseRequested);
    }
  }
  
 private:
  
  // Connect with a given number of retries
  void tryConnect(unsigned retries, std::chrono::steady_clock::time_point start,
                  asio_ns::error_code const& ec) {

    if (_state.load() != Connection::State::Connecting) {
      return;
    }

    if (retries == 0) {
      _state.store(Connection::State::Closed, std::memory_order_release);
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
        static_cast<GeneralConnection<ST, RT>&>(*self)._proto.cancel();
      }
    });
    
    _proto.connect(_config, [self, start, retries](auto const& ec) mutable {
      auto& me = static_cast<GeneralConnection<ST, RT>&>(*self);
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
          auto& me = static_cast<GeneralConnection<ST, RT>&>(*self);
          me.tryConnect(!ec ? retries - 1 : 0, start, ec);
        });
      } else {
        me.tryConnect(0, start, ec); // <- handles errors
      }
    });
  }

 protected:
  /// @brief io context to use
  std::shared_ptr<asio_ns::io_context> _io_context;
  /// @brief underlying socket
  Socket<ST> _proto;
  
  /// elements to send out
  boost::lockfree::queue<RT*, boost::lockfree::capacity<64>> _queue;

  /// default max chunksize is 30kb in arangodb
  static constexpr size_t READ_BLOCK_SIZE = 1024 * 32;
  ::asio_ns::streambuf _receiveBuffer;
  
  std::atomic<uint32_t> _numQueued{0}; /// queued items

  /// @brief is the connection established, do not synchronize on that
  std::atomic<Connection::State> _state{Connection::State::Created};
  
  std::atomic<bool> _active{false};
  
  bool _reading = false;    // set to true while an async_read is ongoing
  bool _writing = false;    // set between starting an asyncWrite operation and executing
                            // the completion handler};
  bool _timeoutOnReadWrite = false;
};

}}  // namespace arangodb::fuerte

#endif
