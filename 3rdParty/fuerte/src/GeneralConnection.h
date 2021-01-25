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

using Clock = std::chrono::steady_clock;

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
    terminateActivity(fuerte::Error::ConnectionCanceled);
  }

  /// @brief connection state
  Connection::State state() const override final {
    return _state.load(std::memory_order_acquire);
  }

  /// The following public methods can be called from any thread:

  // Start an asynchronous request.
  void sendRequest(std::unique_ptr<Request> req, RequestCallback cb) override {
    // construct RequestItem
    auto item = this->createRequest(std::move(req), std::move(cb));
    // set the point-in-time when this request expires
    if (item->request->timeout().count() > 0) {
      item->expires = Clock::now() + item->request->timeout();
    } else {
      item->expires = Clock::time_point::max();
    }

    // Don't send once in Closed state:
    if (this->_state.load(std::memory_order_relaxed) ==
        Connection::State::Closed) {
      item->invokeOnError(Error::ConnectionClosed);
      return;
    }

    // Prepare a new request
    this->_numQueued.fetch_add(1, std::memory_order_relaxed);
    if (!this->_queue.push(item.get())) {
      FUERTE_LOG_ERROR << "connection queue capacity exceeded\n";
      uint32_t q = this->_numQueued.fetch_sub(1, std::memory_order_relaxed);
      FUERTE_ASSERT(q > 0);
      item->invokeOnError(Error::QueueCapacityExceeded);
      return;
    }
    item.release();  // queue owns this now

    FUERTE_LOG_DEBUG << "queued item: this=" << this << "\n";

    // Note that we have first posted on the queue with
    // std::memory_order_seq_cst and now we check _active
    // std::memory_order_seq_cst. This prevents a sleeping barber with the
    // check-set-check combination in `asyncWriteNextRequest`. If we are the
    // ones to exchange the value to `true`, then we post on the `_io_context`
    // to activate the connection. Note that the connection can be in the
    // `Disconnected` or `Connected` or `Failed` state, but not in the
    // `Connecting` state in this case.
    if (!this->_active.exchange(true)) {
      this->_io_context->post([self(Connection::shared_from_this())] {
        static_cast<GeneralConnection<ST, RT>&>(*self).activate();
      });
    }
  }

  /// @brief cancel the connection, unusable afterwards
  void cancel() override {
    FUERTE_LOG_DEBUG << "cancel: this=" << this << "\n";
    asio_ns::post(*_io_context, [self(weak_from_this()), this] {
      auto s = self.lock();
      if (s) {
        shutdownConnection(Error::ConnectionCanceled);
      }
    });
  }

  /// All protected or private methods below here must only be called on the
  /// IO thread.
 protected:
  void startConnection() {
    // start connecting only if state is disconnected
    FUERTE_ASSERT(this->_active.load());
    Connection::State exp = Connection::State::Created;
    if (_state.compare_exchange_strong(exp, Connection::State::Connecting)) {
      FUERTE_LOG_DEBUG << "startConnection: this=" << this << "\n";
      FUERTE_ASSERT(_config._maxConnectRetries > 0);
      tryConnect(_config._maxConnectRetries, Clock::now(),
                 asio_ns::error_code());
    } else {
      FUERTE_LOG_DEBUG << "startConnection: this=" << this
                       << " found unexpected state " << static_cast<int>(exp)
                       << " not equal to 'Created'";
      FUERTE_ASSERT(false);
    }
  }

  // shutdown connection, cancel async operations
  void shutdownConnection(fuerte::Error err, std::string const& msg = "") {
    FUERTE_LOG_DEBUG << "shutdownConnection: err = '" << to_string(err) << "' ";
    if (!msg.empty()) {
      FUERTE_LOG_DEBUG << ", msg = '" << msg << "' ";
    }
    FUERTE_LOG_DEBUG << "this=" << this << "\n";

    auto exp = _state.load();
    if (exp == Connection::State::Closed ||
        !_state.compare_exchange_strong(exp, Connection::State::Closed)) {
      FUERTE_LOG_DEBUG << "connection is already shutdown this=" << this
                       << "\n";
      return;
    }

    abortRequests(err, /*now*/ Clock::time_point::max());

    _proto.shutdown([=, self(shared_from_this())](auto const& ec) {
      terminateActivity(err);
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
    setIOTimeout();  // must be after setting _reading
    _proto.socket.async_read_some(
        mutableBuff, [self = shared_from_this()](auto const& ec, size_t nread) {
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
    FUERTE_LOG_DEBUG << "drain queue this=" << this << "\n";
    RT* item = nullptr;
    while (_queue.pop(item)) {
      FUERTE_ASSERT(item);
      std::unique_ptr<RT> guard(item);
      uint32_t q = this->_numQueued.fetch_sub(1, std::memory_order_relaxed);
      FUERTE_ASSERT(q > 0);
      guard->invokeOnError(ec);
    }
  }

  void activate() {
    Connection::State state = this->_state.load();
    FUERTE_ASSERT(state != Connection::State::Connecting);
    if (state == Connection::State::Connected) {
      FUERTE_LOG_DEBUG << "activate: connected\n";
      this->doWrite();
    } else if (state == Connection::State::Created) {
      FUERTE_LOG_DEBUG << "activate: not connected\n";
      this->startConnection();
    } else if (state == Connection::State::Closed) {
      FUERTE_LOG_ERROR << "activate: queued request on failed connection\n";
      this->drainQueue(fuerte::Error::ConnectionClosed);
      this->_active.store(false);  // No more activity from our side
    }
    // If the state is `Connecting`, we do not need to do anything, this can
    // happen if this `activate` was posted when the connection was still
    // `Disconnected`, but in the meantime the connect has started.
  }

  /// The following is called when the connection is permanently failed. It is
  /// used to shut down any activity in the derived classes in a way that avoids
  /// sleeping barbers
  void terminateActivity(const fuerte::Error err) {
    // Usually, we are `active == true` when we get here, except for the
    // following case: If we are inactive but the connection is still open and
    // then the idle timeout goes off, then we shutdownConnection and in the TLS
    // case we call this method here. In this case it is OK to simply proceed,
    // therefore no assertion on `_active`.
    FUERTE_ASSERT(this->_state.load() == Connection::State::Closed);
    FUERTE_LOG_DEBUG << "terminateActivity: active=true, this=" << this << "\n";
    while (true) {
      this->drainQueue(err);
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

  /// abort ongoing / unfinished requests expiring before given timpoint
  virtual void abortRequests(fuerte::Error, Clock::time_point now) = 0;

  virtual void setIOTimeout() = 0;

  virtual std::unique_ptr<RT> createRequest(std::unique_ptr<Request>&& req,
                                            RequestCallback&& cb) = 0;

 private:
  // Connect with a given number of retries
  void tryConnect(unsigned retries, Clock::time_point start,
                  asio_ns::error_code const& ec) {
    if (_state.load() != Connection::State::Connecting) {
      return;
    }

    if (retries == 0) {
      std::string msg("connecting failed: '");
      msg.append((ec != asio_ns::error::operation_aborted) ? ec.message()
                                                           : "timeout");
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
      // case its closure might already be queued right after ourselves!
      // However, we now quickly set the state to `Connected` in which case the
      // closure will no longer shut down the socket and ruin our success.
      if (!ec) {
        me.finishConnect();
        return;
      }
      FUERTE_LOG_DEBUG << "connecting failed: " << ec.message() << "\n";
      if (retries > 0 && ec != asio_ns::error::operation_aborted) {
        auto end = std::min(Clock::now() + me._config._connectRetryPause,
                            start + me._config._connectTimeout);
        me._proto.timer.expires_at(end);
        me._proto.timer.async_wait(
            [self(std::move(self)), start, retries](auto ec) mutable {
              auto& me = static_cast<GeneralConnection<ST, RT>&>(*self);
              me.tryConnect(!ec ? retries - 1 : 0, start, ec);
            });
      } else {
        me.tryConnect(0, start, ec);  // <- handles errors
      }
    });
  }

 protected:
  /// @brief io context to use
  std::shared_ptr<asio_ns::io_context> _io_context;
  /// @brief underlying socket
  Socket<ST> _proto;

  /// elements to send out
  boost::lockfree::queue<RT*, boost::lockfree::capacity<32>> _queue;

  /// default max chunksize is 30kb in arangodb
  static constexpr size_t READ_BLOCK_SIZE = 1024 * 32;
  ::asio_ns::streambuf _receiveBuffer;

  std::atomic<unsigned> _numQueued{0};  /// queued items

  /// @brief is the connection established, do not synchronize on that
  std::atomic<Connection::State> _state{Connection::State::Created};

  std::atomic<bool> _active{false};

  bool _reading = false;  // set to true while an async_read is ongoing
  bool _writing = false;  // set between starting an asyncWrite operation and
                          // executing the completion handler};
  bool _timeoutOnReadWrite = false;
};

// common superclass VST / HTTP2
template <SocketType ST, typename RT>
struct MultiConnection : public GeneralConnection<ST, RT> {
  explicit MultiConnection(EventLoopService& loop,
                           detail::ConnectionConfiguration const& config)
      : GeneralConnection<ST, RT>(loop, config) {}

  virtual ~MultiConnection() = default;

 public:
  // Return the number of unfinished requests.
  std::size_t requestsLeft() const override {
    uint32_t qd = this->_numQueued.load(std::memory_order_relaxed);
    qd += _streamCount.load(std::memory_order_relaxed);
    return qd;
  }

 protected:
  void setIOTimeout() override {
    setTimeout(/*setIOTimeout*/ this->_reading || this->_writing);
  }

  std::unique_ptr<RT> createRequest(std::unique_ptr<Request>&& req,
                                    RequestCallback&& cb) override {
    return std::make_unique<RT>(std::move(req), std::move(cb));
  }

  fuerte::Error translateError(asio_ns::error_code const& e,
                               fuerte::Error c) const {
    if (e == asio_ns::error::misc_errors::eof ||
        e == asio_ns::error::connection_reset) {
      return fuerte::Error::ConnectionClosed;
    } else if (e == asio_ns::error::operation_aborted ||
               e == asio_ns::error::connection_aborted) {
      // keepalive timeout may have expired
      return this->_timeoutOnReadWrite ? fuerte::Error::ConnectionClosed
                                       : fuerte::Error::ConnectionCanceled;
    }

    return c;
  }

 private:
  void setTimeout(bool setIOBegin) {
    const bool wasIdle = _streams.empty();
    if (wasIdle && !this->_config._useIdleTimeout) {
      asio_ns::error_code ec;
      this->_proto.timer.cancel(ec);
      return;
    }

    auto const now = Clock::now();
    if (setIOBegin) {
      _lastIO = Clock::now();
    }

    auto tp = Clock::time_point::max();
    if (wasIdle) {  // use default connection timeout
      FUERTE_LOG_HTTPTRACE << "setting idle keep alive timer, this=" << this
                           << "\n";
      tp = now + this->_config._idleTimeout;
    } else {
      for (auto const& pair : _streams) {
        tp = std::min(tp, pair.second->expires);
      }
    }

    const bool wasReading = this->_writing;
    const bool wasWriting = this->_writing;
    if (wasReading || wasWriting) {
      FUERTE_ASSERT(_lastIO + kIOTimeout > now);
      tp = std::min(tp, _lastIO + kIOTimeout);
    }

    // expires_after cancels pending ops
    this->_proto.timer.expires_at(tp);
    this->_proto.timer.async_wait(
        [=, self = Connection::weak_from_this()](auto const& ec) {
          std::shared_ptr<Connection> s;
          if (ec || !(s = self.lock())) {  // was canceled / deallocated
            return;
          }

          auto& me = static_cast<MultiConnection<ST, RT>&>(*s);
          auto now = Clock::now();
          bool isIdle = me._streams.empty();
          me.abortRequests(Error::RequestTimeout, now);

          if (wasReading && me._reading) {
            // reading may take longer than kIOTimeout
            // users may adjust request timeout accordingly
            if (now - _lastIO > kIOTimeout && me._streams.empty()) {
              FUERTE_LOG_DEBUG << "IO read timeout"
                               << " this=" << &me << "\n";
              me._timeoutOnReadWrite = true;
              me._proto.cancel();
              // We simply cancel all ongoing asynchronous operations, the
              // completion handlers will do the rest.
            } else {
              // eventually all request expire, then cancel connection
              setTimeout(/*setIOBegin*/ false);
            }
          } else if (wasWriting && me._writing && now - _lastIO > kIOTimeout) {

            FUERTE_LOG_DEBUG << "IO write timeout"
                             << " this=" << &me << "\n";
            me._timeoutOnReadWrite = true;
            me._proto.cancel();
            // We simply cancel all ongoing asynchronous operations, the
            // completion handlers will do the rest.

          } else if (wasIdle && isIdle) {
            if (!me._active && me._state == Connection::State::Connected) {
              FUERTE_LOG_DEBUG << "HTTP-Request idle timeout"
                               << " this=" << &me << "\n";
              me.shutdownConnection(Error::CloseRequested);
            }
          }
          // In all other cases we do nothing, since we have been posted to the
          // iocontext but the thing we should be timing out has already
          // completed.
        });
  }

 protected:
  static constexpr std::chrono::seconds kIOTimeout{300};

  std::map<uint64_t, std::unique_ptr<RT>> _streams;
  std::atomic<unsigned> _streamCount{0};

 private:
  Clock::time_point _lastIO;
};

}}  // namespace arangodb::fuerte

#endif
