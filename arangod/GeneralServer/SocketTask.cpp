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
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Statistics/ConnectionStatistics.h"
#include "Statistics/StatisticsFeature.h"

using namespace arangodb::basics;
using namespace arangodb::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

SocketTask::SocketTask(GeneralServer& server, 
                       GeneralServer::IoContext& context,
                       char const* name,
                       std::unique_ptr<arangodb::Socket> socket,
                       arangodb::ConnectionInfo&& connectionInfo,
                       double keepAliveTimeout, 
                       bool skipInit = false)
    : IoTask(server, context, name),
      _peer(std::move(socket)),
      _connectionInfo(std::move(connectionInfo)),
      _connectionStatistics(nullptr),
      _readBuffer(READ_BLOCK_SIZE + 1, false),
      _stringBuffers{_stringBuffersArena},
      _writeBuffer(nullptr, nullptr),
      _keepAliveTimeout(static_cast<long>(keepAliveTimeout * 1000)),
      _keepAliveTimer(context.newDeadlineTimer(_keepAliveTimeout)),
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
  LOG_TOPIC("28f00", DEBUG, Logger::COMMUNICATION)
      << "Shutting down connection " << (_peer ? _peer->peerPort() : 0);

  if (_connectionStatistics != nullptr) {
    _connectionStatistics->release();
    _connectionStatistics = nullptr;
  }

  cancelKeepAlive();

  // _peer could be nullptr if it was moved out of a HttpCommTask, during
  // upgrade to a VstCommTask.
  if (_peer) {
    asio_ns::error_code err;
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
    LOG_TOPIC("91b78", DEBUG, Logger::COMMUNICATION) << "cannot start, channel closed";
    return false;
  }

  if (_closeRequested.load(std::memory_order_acquire)) {
    LOG_TOPIC("47145", DEBUG, Logger::COMMUNICATION)
        << "cannot start, close already in progress";
    return false;
  }

  LOG_TOPIC("556fd", DEBUG, Logger::COMMUNICATION)
      << "starting communication between server <-> client on socket";
  LOG_TOPIC("68744", DEBUG, Logger::COMMUNICATION)
      << _connectionInfo.serverAddress << ":" << _connectionInfo.serverPort << " <-> "
      << _connectionInfo.clientAddress << ":" << _connectionInfo.clientPort;

  _peer->post([self = shared_from_this(), this]() { asyncReadSome(); });

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

// caller must hold the _lock
void SocketTask::addWriteBuffer(WriteBuffer&& buffer) {
  TRI_ASSERT(_peer->runningInThisThread());

  if (_closedSend.load(std::memory_order_acquire) ||
      _abandoned.load(std::memory_order_acquire)) {
    LOG_TOPIC("01285", DEBUG, Logger::COMMUNICATION) << "Connection abandoned or closed";
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

// caller must hold the _lock
bool SocketTask::completedWriteBuffer() {
  TRI_ASSERT(_peer != nullptr);
  TRI_ASSERT(_peer->runningInThisThread());

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

// caller must not hold the _lock
void SocketTask::closeStream() {
  if (_abandoned.load(std::memory_order_acquire)) {
    _server.unregisterTask(this->id());
    return;
  }

  // strand::dispatch may execute this immediately if this
  // is called on a thread inside the same strand
  _peer->post([self = shared_from_this(), this] { closeStreamNoLock(); });
}

// caller must hold the _lock
void SocketTask::closeStreamNoLock() {
  TRI_ASSERT(_peer != nullptr);
  TRI_ASSERT(_peer->runningInThisThread());

  bool mustCloseSend = !_closedSend.load(std::memory_order_acquire);
  bool mustCloseReceive = !_closedReceive.load(std::memory_order_acquire);

  if (_peer != nullptr) {
    LOG_TOPIC("f0947", DEBUG, Logger::COMMUNICATION) << "closing stream";
    asio_ns::error_code err;  // an error we do not care about
    _peer->shutdown(err, mustCloseSend, mustCloseReceive);
  }

  _closedSend.store(true, std::memory_order_release);
  _closedReceive.store(true, std::memory_order_release);
  _closeRequested.store(false, std::memory_order_release);
  cancelKeepAlive();
  
  _server.unregisterTask(this->id());
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

// will acquire the _lock
void SocketTask::addToReadBuffer(char const* data, std::size_t len) {
  TRI_ASSERT(_peer != nullptr);
  TRI_ASSERT(_peer->runningInThisThread());

  _readBuffer.appendText(data, len);
}

// does not need lock
void SocketTask::resetKeepAlive() {
  if (_useKeepAliveTimer) {
    asio_ns::error_code err;
    _keepAliveTimer->expires_from_now(_keepAliveTimeout, err);
    if (err) {
      closeStream();
      return;
    }

    _keepAliveTimerActive.store(true, std::memory_order_relaxed);
    _keepAliveTimer->async_wait([self = shared_from_this(), this](const asio_ns::error_code& error) {
      if (!error) {  // error will be true if timer was canceled
        LOG_TOPIC("5c1e0", ERR, Logger::COMMUNICATION)
            << "keep alive timout - closing stream!";
        closeStream();
      }
    });
  }
}

// caller must hold the _lock
void SocketTask::cancelKeepAlive() {
  if (_useKeepAliveTimer && _keepAliveTimerActive.load(std::memory_order_relaxed)) {
    asio_ns::error_code err;
    _keepAliveTimer->cancel(err);
    _keepAliveTimerActive.store(false, std::memory_order_relaxed);
  }
}

// caller must hold the _lock
bool SocketTask::reserveMemory() {
  TRI_ASSERT(_peer != nullptr);
  TRI_ASSERT(_peer->runningInThisThread());

  if (_readBuffer.reserve(READ_BLOCK_SIZE + 1) == TRI_ERROR_OUT_OF_MEMORY) {
    LOG_TOPIC("1997b", WARN, arangodb::Logger::COMMUNICATION)
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
  TRI_ASSERT(_peer->runningInThisThread());

  asio_ns::error_code err;
  if (0 == _peer->available(err)) {
    return false;
  }

  if (err) {
    LOG_TOPIC("62289", DEBUG, Logger::COMMUNICATION) << "read failed with " << err.message();
    return false;
  }

  if (!reserveMemory()) {
    LOG_TOPIC("dd32f", TRACE, Logger::COMMUNICATION) << "failed to reserve memory";
    return false;
  }

  size_t bytesRead =
      _peer->readSome(asio_ns::buffer(_readBuffer.end(), READ_BLOCK_SIZE), err);

  if (0 == bytesRead) {
    return false;  // should not happen
  }

  _readBuffer.increaseLength(bytesRead);

  if (!err) {
    return true;
  }

  if (err != asio_ns::error::would_block && err != asio_ns::error::try_again) {
    LOG_TOPIC("91480", DEBUG, Logger::COMMUNICATION)
        << "trySyncRead failed with: " << err.message();
  }

  return false;
}

// caller must hold the _lock
// runs until _closeRequested or ProcessRead Returns false is true or task
// becomes abandoned
// returns bool - true value signals that processRead should continue to run
// (new read)
bool SocketTask::processAll() {
  TRI_ASSERT(_peer != nullptr);
  TRI_ASSERT(_peer->runningInThisThread());

  double startTime = StatisticsFeature::time();
  Result res;
  bool rv = true;
  while (rv) {
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
      LOG_TOPIC("a3c44", ERR, Logger::COMMUNICATION) << res.errorMessage();
      _closeRequested.store(true, std::memory_order_release);
      return false;
    }

    if (_closeRequested.load(std::memory_order_acquire)) {
      return false;
    }
  }

  // it is too early to close the stream here, as there may
  // be some writeBuffers which still need to be sent to the client
  return !_closeRequested;
}

// must be invoked on strand
void SocketTask::asyncReadSome() {
  TRI_ASSERT(_peer != nullptr);
  TRI_ASSERT(_peer->runningInThisThread());

  if (this->canUseMixedIO()) {
    // try some direct read only for non-SSL mode
    // in SSL mode it will fall apart when mixing direct reads and async
    // reads later
    try {
      size_t const MAX_DIRECT_TRIES = 2;
      size_t n = 0;

      while (++n <= MAX_DIRECT_TRIES && !_abandoned.load(std::memory_order_acquire)) {
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
      LOG_TOPIC("d5bb6", DEBUG, Logger::COMMUNICATION) << "sync read failed with: " << err.what();
      closeStreamNoLock();
      return;
    } catch (...) {
      LOG_TOPIC("00e8a", DEBUG, Logger::COMMUNICATION) << "general error on stream";

      closeStreamNoLock();
      return;
    }
  }

  // try to read more bytes
  if (_abandoned.load(std::memory_order_acquire)) {
    return;
  }
  if (!reserveMemory()) {
    LOG_TOPIC("fcd45", TRACE, Logger::COMMUNICATION) << "failed to reserve memory";
    return;
  }

  // WARNING: the _readBuffer MUST NOT be changed until the callback
  // has been called! Otherwise ASIO will get confused and write to
  // the wrong position.

  TRI_ASSERT(_peer != nullptr);
  _peer->asyncRead(asio_ns::buffer(_readBuffer.end(), READ_BLOCK_SIZE),
                   [self = shared_from_this(), this](const asio_ns::error_code& ec, std::size_t transferred) {
                     if (_abandoned.load(std::memory_order_acquire)) {
                       return;
                     } else if (ec) {
                       LOG_TOPIC("29dca", DEBUG, Logger::COMMUNICATION)
                           << "read on stream failed with: " << ec.message();
                       closeStream();
                       return;
                     }

                     _readBuffer.increaseLength(transferred);

                     if (processAll()) {
                       _peer->post([self, this]() { asyncReadSome(); });
                     }
                     compactify();
                   });
}

void SocketTask::asyncWriteSome() {
  TRI_ASSERT(_peer != nullptr);
  TRI_ASSERT(_peer->runningInThisThread());

  if (_writeBuffer.empty()) {
    return;
  }

  TRI_ASSERT(_writeBuffer._buffer != nullptr);
  size_t total = _writeBuffer._buffer->length();
  size_t written = 0;

  TRI_ASSERT(!_abandoned);

  asio_ns::error_code err;

  if (this->canUseMixedIO()) {
    // try some direct writes only for non-SSL mode
    // in SSL mode it will fall apart when mixing direct writes and async
    // writes later
    while (true) {
      TRI_ASSERT(_writeBuffer._buffer != nullptr);

      // we can directly skip sending empty buffers
      if (_writeBuffer._buffer->length() > 0) {
        RequestStatistics::SET_WRITE_START(_writeBuffer._statistics);
        written = _peer->writeSome(_writeBuffer._buffer, err);

        RequestStatistics::ADD_SENT_BYTES(_writeBuffer._statistics, written);

        if (err || written != total) {
          // unable to write everything at once, might be a lot of data
          // above code does not update the buffer positon
          break;
        }

        TRI_ASSERT(written > 0);
      }

      if (!completedWriteBuffer()) {
        return;
      }

      // try to send next buffer
      TRI_ASSERT(_writeBuffer._buffer != nullptr);
      total = _writeBuffer._buffer->length();
    }

    // write could have blocked which is the only acceptable error
    if (err && err != asio_ns::error::would_block && err != asio_ns::error::try_again) {
      LOG_TOPIC("e25ec", DEBUG, Logger::COMMUNICATION)
          << "sync write on stream failed with: " << err.message();
      closeStreamNoLock();
      return;
    }
  }  // !_peer->isEncrypted

  // we will be getting here in the following cases
  // - encrypted mode (SSL)
  // - we send only parts of the write buffer, but have more to send
  // - we got the error would_block/try_again when sending data
  // in this case we dispatch an async write

  if (_abandoned.load(std::memory_order_acquire)) {
    return;
  }

  TRI_ASSERT(_writeBuffer._buffer != nullptr);

  // so the code could have blocked at this point or not all data
  // was written in one go, begin writing at offset (written)
  _peer->asyncWrite(asio_ns::buffer(_writeBuffer._buffer->begin() + written, total - written),
                    [self = shared_from_this(), this](const asio_ns::error_code& ec, std::size_t transferred) {
                      if (_abandoned.load(std::memory_order_acquire)) {
                        return;
                      }
                      if (ec) {
                        LOG_TOPIC("8ed36", DEBUG, Logger::COMMUNICATION)
                            << "write failed with: " << ec.message();
                        closeStream();
                        return;
                      }

                      RequestStatistics::ADD_SENT_BYTES(_writeBuffer._statistics, transferred);

                      if (completedWriteBuffer()) {
                        if (!_abandoned.load(std::memory_order_acquire)) {
                          asyncWriteSome();
                        }
                      }
                    });
}

StringBuffer* SocketTask::leaseStringBuffer(size_t length) {
  std::unique_ptr<StringBuffer> buffer;
  
  MUTEX_LOCKER(guard, _bufferLock);

  if (!_stringBuffers.empty()) {
    buffer.reset(_stringBuffers.back());
    _stringBuffers.pop_back();
    // we can release the lock here already
    guard.unlock();

    TRI_ASSERT(buffer != nullptr);
    TRI_ASSERT(buffer->length() == 0);

    size_t const n = buffer->capacity();
    if (n < length) {
      if (buffer->reserve(length) != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }
    }
  } else {
    buffer.reset(new StringBuffer(length, false));
  }

  TRI_ASSERT(buffer != nullptr);

  // still check for safety reasons
  if (buffer->capacity() >= length) {
    return buffer.release();
  }

  THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
}

void SocketTask::returnStringBuffer(StringBuffer* buffer) {
  TRI_ASSERT(buffer != nullptr);
  MUTEX_LOCKER(guard, _bufferLock);

  if (_stringBuffers.size() > 4 || buffer->capacity() >= 4 * 1024 * 1024) {
    // don't keep too many buffers around and don't hog too much memory
    guard.unlock();

    delete buffer;
  } else {
    try {
      buffer->reset();
      _stringBuffers.emplace_back(buffer);
    } catch (...) {
      delete buffer;
    }
  }
}

void SocketTask::triggerProcessAll() {
  // try to process remaining request data
  _peer->post([self = shared_from_this(), this] { processAll(); });
}
