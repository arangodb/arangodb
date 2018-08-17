////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "SocketTask.h"

#include <thread>

#include "Basics/MutexLocker.h"
#include "Basics/StringBuffer.h"
#include "Basics/socket-utils.h"
#include "Endpoint/ConnectionInfo.h"
#include "Logger/Logger.h"
#include "Scheduler/EventLoop.h"
#include "Scheduler/JobGuard.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Statistics/ConnectionStatistics.h"
#include "Statistics/StatisticsFeature.h"

using namespace arangodb::basics;
using namespace arangodb::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

SocketTask::SocketTask(arangodb::EventLoop loop,
                       std::unique_ptr<arangodb::Socket> socket,
                       arangodb::ConnectionInfo&& connectionInfo,
                       double keepAliveTimeout, bool skipInit = false)
    : Task(loop, "SocketTask"),
      _peer(std::move(socket)),
      _connectionInfo(std::move(connectionInfo)),
      _connectionStatistics(nullptr),
      _readBuffer(READ_BLOCK_SIZE + 1, false),
      _stringBuffers{_stringBuffersArena},
      _writeBuffer(nullptr, nullptr),
      _keepAliveTimeout(static_cast<long>(keepAliveTimeout * 1000)),
      _keepAliveTimer(*_loop.ioContext, _keepAliveTimeout),
      _useKeepAliveTimer(keepAliveTimeout > 0.0),
      _keepAliveTimerActive(false),
      _closeRequested(false),
      _abandoned(false),
      _closedSend(false),
      _closedReceive(false) {
  _connectionStatistics = ConnectionStatistics::acquire();
  ConnectionStatistics::SET_START(_connectionStatistics);

  if (!skipInit) {
    TRI_ASSERT(_peer != nullptr);
    _peer->setNonBlocking(true);
    if (!_peer->handshake()) {
      _closedSend = true;
      _closedReceive = true;
    }
  }
}

SocketTask::~SocketTask() {
  LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "Shutting down connection "
                                          << (_peer ? _peer->peerPort() : 0);

  if (_connectionStatistics != nullptr) {
    _connectionStatistics->release();
    _connectionStatistics = nullptr;
  }

  asio_ns::error_code err;
  if (_keepAliveTimerActive.load(std::memory_order_relaxed)) {
    _keepAliveTimer.cancel(err);
  }

  if (err) {
    LOG_TOPIC(ERR, Logger::COMMUNICATION) << "unable to cancel _keepAliveTimer";
  }

  // _peer could be nullptr if it was moved out of a HttpCommTask, during
  // upgrade to a VstCommTask.
  if (_peer) {
    _peer->close(err);
  }

  // delete all string buffers we have allocated
  for (auto& it : _stringBuffers) {
    delete it;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

bool SocketTask::start() {
  if (_closedSend.load(std::memory_order_acquire) ||
      _closedReceive.load(std::memory_order_acquire)) {
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "cannot start, channel closed";
    return false;
  }

  if (_closeRequested.load(std::memory_order_acquire)) {
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
        << "cannot start, close alread in progress";
    return false;
  }

  LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
      << "starting communication between server <-> client on socket";
  LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
      << _connectionInfo.serverAddress << ":" << _connectionInfo.serverPort
      << " <-> " << _connectionInfo.clientAddress << ":"
      << _connectionInfo.clientPort;

  auto self = shared_from_this();
  _peer->strand.post([self, this]() {
    JobGuard guard(_loop);
    guard.work();
    asyncReadSome();
  });
  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

// must run in strand
void SocketTask::addWriteBuffer(WriteBuffer&& buffer) {
  TRI_ASSERT(_peer->strand.running_in_this_thread());

  if (_closedSend.load(std::memory_order_acquire) ||
      _abandoned.load(std::memory_order_acquire)) {
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "Connection abandoned or closed";
    buffer.release();
    return;
  }

  TRI_ASSERT(!buffer.empty());
  if (!buffer.empty()) {
    if (!_writeBuffer.empty()) {
      _writeBuffers.emplace_back(std::move(buffer));
      return;
    }
    _writeBuffer = std::move(buffer);
  }

  asyncWriteSome();
}

// must run in strand
bool SocketTask::completedWriteBuffer() {
  TRI_ASSERT(_peer != nullptr);
  TRI_ASSERT(_peer->strand.running_in_this_thread());

  RequestStatistics::SET_WRITE_END(_writeBuffer._statistics);
  _writeBuffer.release(this);  // try to recycle the string buffer
  if (_writeBuffers.empty()) {
    if (_closeRequested) {
      closeStreamNoLock();
    }
    return false;
  }

  _writeBuffer = std::move(_writeBuffers.front());
  _writeBuffers.pop_front();

  return true;
}

// must run in strand
void SocketTask::closeStream() {
  if (_abandoned.load(std::memory_order_acquire)) {
    return;
  }
  // strand::dispatch may execute this immediately if this
  // is called on a thread inside the same strand
  auto self = shared_from_this();
  _peer->strand.post([self, this] {
    JobGuard guard(_loop);
    guard.work();
    closeStreamNoLock();
  });
}

// must run in strand
void SocketTask::closeStreamNoLock() {
  TRI_ASSERT(_peer != nullptr);
  TRI_ASSERT(_peer->strand.running_in_this_thread());

  bool mustCloseSend = !_closedSend.load(std::memory_order_acquire);
  bool mustCloseReceive = !_closedReceive.load(std::memory_order_acquire);

  if (_peer != nullptr) {
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "closing stream";
    asio_ns::error_code err;  // an error we do not care about
    _peer->shutdown(err, mustCloseSend, mustCloseReceive);
  }

  _closedSend.store(true, std::memory_order_release);
  _closedReceive.store(true, std::memory_order_release);
  _closeRequested.store(false, std::memory_order_release);
  _keepAliveTimer.cancel();
  _keepAliveTimerActive.store(false, std::memory_order_relaxed);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

// will acquire the _lock
void SocketTask::addToReadBuffer(char const* data, std::size_t len) {
  TRI_ASSERT(_peer != nullptr);
  TRI_ASSERT(_peer->strand.running_in_this_thread());
  _readBuffer.appendText(data, len);
}

// does not need lock
void SocketTask::resetKeepAlive() {
  if (_useKeepAliveTimer) {
    asio_ns::error_code err;
    _keepAliveTimer.expires_from_now(_keepAliveTimeout, err);
    if (err) {
      closeStream();
      return;
    }

    _keepAliveTimerActive.store(true, std::memory_order_relaxed);
    auto self = shared_from_this();
    _keepAliveTimer.async_wait([self, this](const asio_ns::error_code& error) {
      if (!error) {  // error will be true if timer was canceled
        LOG_TOPIC(ERR, Logger::COMMUNICATION)
            << "keep alive timeout - closing stream!";
        closeStream();
      }
    });
  }
}

// must run in strand
void SocketTask::cancelKeepAlive() {
  if (_useKeepAliveTimer &&
      _keepAliveTimerActive.load(std::memory_order_relaxed)) {
    asio_ns::error_code err;
    _keepAliveTimer.cancel(err);
    _keepAliveTimerActive.store(false, std::memory_order_relaxed);
  }
}

// must run in strand
bool SocketTask::reserveMemory() {
  TRI_ASSERT(_peer != nullptr);
  TRI_ASSERT(_peer->strand.running_in_this_thread());
  if (_readBuffer.reserve(READ_BLOCK_SIZE + 1) == TRI_ERROR_OUT_OF_MEMORY) {
    LOG_TOPIC(WARN, arangodb::Logger::COMMUNICATION)
        << "out of memory while reading from client";
    closeStreamNoLock();
    return false;
  }

  return true;
}

// caller must be on _peer->strand()
bool SocketTask::trySyncRead() {
  if (_abandoned.load(std::memory_order_acquire)) {
    return false;
  }

  TRI_ASSERT(_peer != nullptr);
  TRI_ASSERT(_peer->strand.running_in_this_thread());

  asio_ns::error_code err;
  TRI_ASSERT(_peer != nullptr);
  if (0 == _peer->available(err)) {
    return false;
  }

  if (err) {
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "read failed with "
                                            << err.message();
    return false;
  }

  if (!reserveMemory()) {
    LOG_TOPIC(TRACE, Logger::COMMUNICATION) << "failed to reserve memory";
    return false;
  }

  size_t bytesRead =
      _peer->readSome(asio_ns::buffer(_readBuffer.end(), READ_BLOCK_SIZE), err);

  if (0 == bytesRead) {
    return false;  // should not happen
  }

  _readBuffer.increaseLength(bytesRead);

  if (err) {
    if (err == asio_ns::error::would_block) {
      return false;
    } else {
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "trySyncRead failed with: "
                                              << err.message();
      return false;
    }
  }

  return true;
}

// must run in strand
// runs until _closeRequested or ProcessRead Returns false is true or task
// becomes abandoned
// returns bool - true value signals that processRead should continue to run
// (new read)
bool SocketTask::processAll() {
  TRI_ASSERT(_peer != nullptr);
  TRI_ASSERT(_peer->strand.running_in_this_thread());

  double startTime = StatisticsFeature::time();
  Result res;
  bool rv = true;
  while (rv) {
    Result result{TRI_ERROR_NO_ERROR};
    try {
      rv = processRead(startTime);
    } catch (arangodb::basics::Exception const& e) {
      res.reset(e.code(), e.message());
    } catch (std::bad_alloc const&) {
      res.reset(TRI_ERROR_OUT_OF_MEMORY);
    } catch (std::exception const& e) {
      res.reset(TRI_ERROR_INTERNAL, e.what());
    } catch (...) {
      res.reset(TRI_ERROR_INTERNAL);
    }

    if (_abandoned.load(std::memory_order_acquire)) {
      return false;
    }

    if (res.fail()) {
      LOG_TOPIC(ERR, Logger::COMMUNICATION) << res.errorMessage();
      _closeRequested.store(true, std::memory_order_release);
      break;
    }

    if (_closeRequested.load(std::memory_order_acquire)) {
      break;
    }
  }

  // it is too early to close the stream here, as there may
  // be some writeBuffers which still need to be sent to the client
  return !_closeRequested;
}

// must be invoked on strand
void SocketTask::asyncReadSome() {
  TRI_ASSERT(_peer != nullptr);
  TRI_ASSERT(_peer->strand.running_in_this_thread());

  try {
    size_t const MAX_DIRECT_TRIES = 2;
    size_t n = 0;

    while (++n <= MAX_DIRECT_TRIES &&
           !_abandoned.load(std::memory_order_acquire)) {
      if (!trySyncRead()) {
        if (n < MAX_DIRECT_TRIES) {
          std::this_thread::yield();
        }
        continue;
      }

      if (_abandoned.load(std::memory_order_acquire)) {
        return;
      }

      // ignore the result of processAll, try to read more bytes down below
      processAll();
      compactify();
    }
  } catch (asio_ns::system_error const& err) {
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "sync read failed with: "
                                            << err.what();
    closeStreamNoLock();
    return;
  } catch (...) {
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "general error on stream";

    closeStreamNoLock();
    return;
  }

  // try to read more bytes
  if (_abandoned.load(std::memory_order_acquire)) {
    return;
  } else if (!reserveMemory()) {
    LOG_TOPIC(TRACE, Logger::COMMUNICATION) << "failed to reserve memory";
    return;
  }

  auto self = shared_from_this();

  // WARNING: the _readBuffer MUST NOT be changed until the callback
  // has been called! Otherwise ASIO will get confused and write to
  // the wrong position.

  TRI_ASSERT(_peer != nullptr);
  _peer->asyncRead(
      asio_ns::buffer(_readBuffer.end(), READ_BLOCK_SIZE),
      [self, this](const asio_ns::error_code& ec, std::size_t transferred) {
        JobGuard guard(_loop);
        guard.work();

        if (_abandoned.load(std::memory_order_acquire)) {
          return;
        } else if (ec) {
          LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
              << "read on stream failed with: " << ec.message();
          closeStream();
          return;
        }

        _readBuffer.increaseLength(transferred);
        if (processAll()) {
          _peer->strand.post([self, this]() {
            JobGuard guard(_loop);
            guard.work();
            asyncReadSome();
          });
        }
        compactify();
      });
}

void SocketTask::asyncWriteSome() {
  TRI_ASSERT(_peer != nullptr);
  TRI_ASSERT(_peer->strand.running_in_this_thread());

  if (_writeBuffer.empty()) {
    return;
  }
  size_t total = _writeBuffer._buffer->length();
  size_t written = 0;

  TRI_ASSERT(!_abandoned);
  TRI_ASSERT(_peer != nullptr);

  asio_ns::error_code err;
  err.clear();
  while (true) {
    RequestStatistics::SET_WRITE_START(_writeBuffer._statistics);
    written = _peer->writeSome(_writeBuffer._buffer, err);

    if (err) {
      break;
    }

    RequestStatistics::ADD_SENT_BYTES(_writeBuffer._statistics, written);

    if (written != total) {
      // unable to write everything at once, might be a lot of data
      // above code does not update the buffer positon
      break;
    }

    if (!completedWriteBuffer()) {
      return;
    }

    // try to send next buffer
    total = _writeBuffer._buffer->length();
    written = 0;
  }

  // write could have blocked which is the only acceptable error
  if (err && err != ::asio_ns::error::would_block) {
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "sync write on failed with: "
                                            << err.message();
    closeStreamNoLock();
    return;
  }

  if (_abandoned.load(std::memory_order_acquire)) {
    return;
  }

  // so the code could have blocked at this point or not all data
  // was written in one go, begin writing at offset (written)
  auto self = shared_from_this();
  _peer->asyncWrite(
      asio_ns::buffer(_writeBuffer._buffer->begin() + written, total - written),
      [self, this](const asio_ns::error_code& ec, std::size_t transferred) {
        JobGuard guard(_loop);
        guard.work();

        if (_abandoned.load(std::memory_order_acquire)) {
          return;
        } else if (ec) {
          LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "write on failed with: "
                                                  << ec.message();
          closeStream();
          return;
        }

        RequestStatistics::ADD_SENT_BYTES(_writeBuffer._statistics,
                                          transferred);

        if (completedWriteBuffer()) {
          _peer->strand.post([self, this] {
            JobGuard guard(_loop);
            guard.work();
            if (!_abandoned.load(std::memory_order_acquire)) {
              asyncWriteSome();
            }
          });
        }
      });
}

StringBuffer* SocketTask::leaseStringBuffer(size_t length) {
  MUTEX_LOCKER(guard, _bufferLock);

  StringBuffer* buffer = nullptr;
  if (!_stringBuffers.empty()) {
    buffer = _stringBuffers.back();
    TRI_ASSERT(buffer != nullptr);
    TRI_ASSERT(buffer->length() == 0);

    size_t const n = buffer->capacity();
    if (n < length) {
      if (buffer->reserve(length) != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }
    }
    _stringBuffers.pop_back();
  } else {
    buffer = new StringBuffer(length, false);
  }

  TRI_ASSERT(buffer != nullptr);

  // still check for safety reasons
  if (buffer->capacity() >= length) {
    return buffer;
  }

  delete buffer;
  THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
}

void SocketTask::returnStringBuffer(StringBuffer* buffer) {
  TRI_ASSERT(buffer != nullptr);
  MUTEX_LOCKER(guard, _bufferLock);

  if (_stringBuffers.size() > 4 || buffer->capacity() >= 4 * 1024 * 1024) {
    // don't keep too many buffers around and don't hog too much memory
    delete buffer;
    return;
  }

  try {
    buffer->reset();
    _stringBuffers.emplace_back(buffer);
  } catch (...) {
    delete buffer;
  }
}

void SocketTask::triggerProcessAll() {
  // try to process remaining request data
  auto self = shared_from_this();
  _peer->strand.post([self, this] {
    JobGuard guard(_loop);
    guard.work();
    processAll();
  });
}
