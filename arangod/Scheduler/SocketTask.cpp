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

#include <thread>

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
      _connectionStatistics(nullptr),
      _connectionInfo(std::move(connectionInfo)),
      _readBuffer(TRI_UNKNOWN_MEM_ZONE, READ_BLOCK_SIZE + 1, false),
      _stringBuffers{_stringBuffersArena},
      _writeBuffer(nullptr, nullptr),
      _peer(std::move(socket)),
      _keepAliveTimeout(static_cast<long>(keepAliveTimeout * 1000)),
      _keepAliveTimer(_peer->_ioService, _keepAliveTimeout),
      _useKeepAliveTimer(keepAliveTimeout > 0.0),
      _keepAliveTimerActive(false),
      _closeRequested(false),
      _abandoned(false) {
  _connectionStatistics = ConnectionStatistics::acquire();
  ConnectionStatistics::SET_START(_connectionStatistics);

  if (!skipInit) {
    _peer->setNonBlocking(true);
    if (!_peer->handshake()) {
      _closedSend = true;
      _closedReceive = true;
    }
  }
}

SocketTask::~SocketTask() {
  if (_connectionStatistics != nullptr) {
    _connectionStatistics->release();
    _connectionStatistics = nullptr;
  }

  MUTEX_LOCKER(locker, _lock);
  boost::system::error_code err;
      
  if (_keepAliveTimerActive) {
    _keepAliveTimer.cancel(err);
  }

  if (err) {
    LOG_TOPIC(ERR, Logger::COMMUNICATION) << "unable to cancel _keepAliveTimer";
  }

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

void SocketTask::start() {
  if (_closedSend || _closedReceive) {
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "cannot start, channel closed";
    return;
  }

  if (_closeRequested) {
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
        << "cannot start, close alread in progress";
    return;
  }

  LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
      << "starting communication between server <-> client on socket";
  LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
      << _connectionInfo.serverAddress << ":" << _connectionInfo.serverPort
      << " <-> " << _connectionInfo.clientAddress << ":"
      << _connectionInfo.clientPort;

  auto self = shared_from_this();
  _loop._scheduler->post([self, this]() { asyncReadSome(); });
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

// caller must hold the _lock
void SocketTask::addWriteBuffer(WriteBuffer& buffer) {
  _lock.assertLockedByCurrentThread();

  if (_closedSend || _abandoned) {
    buffer.release();
    return;
  }

  {
    auto self = shared_from_this();

    _loop._scheduler->post([self, this]() {
      MUTEX_LOCKER(locker, _lock);
      processAll();
    });
  }

  if (!buffer.empty()) {
    if (!_writeBuffer.empty()) {
      _writeBuffers.emplace_back(std::move(buffer));
      return;
    }

    _writeBuffer = std::move(buffer);
  }

  writeWriteBuffer();
}

// caller must hold the _lock
void SocketTask::writeWriteBuffer() {
  _lock.assertLockedByCurrentThread();

  if (_writeBuffer.empty()) {
    return;
  }

  size_t total = _writeBuffer._buffer->length();
  size_t written = 0;

  if (!_peer->isEncrypted()) {
    boost::system::error_code err;
    err.clear();

    while (true) {
      RequestStatistics::SET_WRITE_START(_writeBuffer._statistics);
      written = _peer->write(_writeBuffer._buffer, err);

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
    if (err && err != ::boost::asio::error::would_block) {
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "write on stream failed with: "
        << err.message();
      closeStreamNoLock();
      return;
    }
  }

  // so the code could have blocked at this point or not all data
  // was written in one go, begin writing at offset (written)
  auto self = shared_from_this();
  _peer->asyncWrite(boost::asio::buffer(_writeBuffer._buffer->begin() + written,
                                        total - written),
                    [self, this](const boost::system::error_code& ec,
                                 std::size_t transferred) {
                      MUTEX_LOCKER(locker, _lock);

                      if (_abandoned) {
                        return;
                      }

                      RequestStatistics::ADD_SENT_BYTES(
                          _writeBuffer._statistics, transferred);

                      if (ec) {
                        LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
                            << "write on stream failed with: " << ec.message();
                        closeStreamNoLock();
                      } else {
                        if (completedWriteBuffer()) {
                          _loop._scheduler->post([self, this]() {
                            MUTEX_LOCKER(locker, _lock);
                            writeWriteBuffer();
                          });
                        }
                      }
                    });
}
    
StringBuffer* SocketTask::leaseStringBuffer(size_t length) {
  _lock.assertLockedByCurrentThread();

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
    buffer = new StringBuffer(TRI_UNKNOWN_MEM_ZONE, length, false);
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
  _lock.assertLockedByCurrentThread();

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

// caller must hold the _lock
bool SocketTask::completedWriteBuffer() {
  _lock.assertLockedByCurrentThread();

  RequestStatistics::SET_WRITE_END(_writeBuffer._statistics);
  // try to recycle the string buffer
  _writeBuffer.release(this);

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
  MUTEX_LOCKER(locker, _lock);
  closeStreamNoLock();
}

// caller must hold the _lock
void SocketTask::closeStreamNoLock() {
  _lock.assertLockedByCurrentThread();

  boost::system::error_code err;

  bool closeSend = !_closedSend;
  bool closeReceive = !_closedReceive;
  
  _peer->shutdown(err, closeSend, closeReceive);
  
  _closedSend = true;
  _closedReceive = true;
  _closeRequested = false;
  _keepAliveTimer.cancel();
  _keepAliveTimerActive = false;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

// will acquire the _lock
void SocketTask::addToReadBuffer(char const* data, std::size_t len) {
  MUTEX_LOCKER(locker, _lock);

  LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << std::string(data, len);
  _readBuffer.appendText(data, len);
}

// caller must hold the _lock
void SocketTask::resetKeepAlive() {
  _lock.assertLockedByCurrentThread();

  if (_useKeepAliveTimer) {
    boost::system::error_code err;
    _keepAliveTimer.expires_from_now(_keepAliveTimeout, err);

    if (err) {
      closeStreamNoLock();
      return;
    }

    _keepAliveTimerActive = true;
    auto self = shared_from_this();

    _keepAliveTimer.async_wait(
        [self, this](const boost::system::error_code& error) {
          LOG_TOPIC(TRACE, Logger::COMMUNICATION)
              << "keepAliveTimerCallback - called with: " << error.message();
          if (!error) {
            LOG_TOPIC(TRACE, Logger::COMMUNICATION)
                << "keep alive timout - closing stream!";
            closeStream();
          }
        });
  }
}

// caller must hold the _lock
// abandon the task. if the task was already abandoned, this
// method returns false. if abandoing was successful, this
// method returns true
bool SocketTask::abandon() {
  _lock.assertLockedByCurrentThread();

  bool old = _abandoned;
  _abandoned = true;
  return !old;
}

// caller must hold the _lock
void SocketTask::cancelKeepAlive() {
  _lock.assertLockedByCurrentThread();

  if (_useKeepAliveTimer && _keepAliveTimerActive) {
    boost::system::error_code err;
    _keepAliveTimer.cancel(err);
    _keepAliveTimerActive = false;
  }
}

// caller must hold the _lock
bool SocketTask::reserveMemory() {
  _lock.assertLockedByCurrentThread();

  if (_readBuffer.reserve(READ_BLOCK_SIZE + 1) == TRI_ERROR_OUT_OF_MEMORY) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "out of memory while reading from client";
    closeStreamNoLock();
    return false;
  }

  return true;
}

// caller must hold the _lock
bool SocketTask::trySyncRead() {
  _lock.assertLockedByCurrentThread();

  boost::system::error_code err;

  if (_abandoned) {
    return false;
  }

  if (0 == _peer->available(err)) {
    return false;
  }

  if (err) {
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "read failed with "
                                            << err.message();
    return false;
  }

  size_t bytesRead = _peer->read(boost::asio::buffer(_readBuffer.end(), READ_BLOCK_SIZE), err);

  if (0 == bytesRead) {
    return false;  // should not happen
  }

  _readBuffer.increaseLength(bytesRead);

  if (err) {
    if (err == boost::asio::error::would_block) {
      return false;
    } else {
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
          << "trySyncRead failed with: " << err.message();
      return false;
    }
  }

  return true;
}

// caller must hold the _lock
bool SocketTask::processAll() {
  _lock.assertLockedByCurrentThread();

  double startTime = StatisticsFeature::time();

  while (processRead(startTime)) {
    if (_abandoned) {
      return false;
    }

    if (_closeRequested) {
      break;
    }
  }

  if (_closeRequested) {
    // it is too early to close the stream here, as there may
    // be some writeBuffers which still need to be sent to the client
    return false;
  }
  
  return true;
}

// will acquire the _lock
void SocketTask::asyncReadSome() {
  MUTEX_LOCKER(locker, _lock);
    
  if (_abandoned) {
    return;
  }

  if (!_peer->isEncrypted()) {
    try {
      size_t const MAX_DIRECT_TRIES = 2;
      size_t n = 0;

      while (++n <= MAX_DIRECT_TRIES) {
        if (!reserveMemory()) {
          LOG_TOPIC(TRACE, Logger::COMMUNICATION) << "failed to reserve memory";
          return;
        }

        if (!trySyncRead()) {
          if (n < MAX_DIRECT_TRIES) {
            std::this_thread::yield();
          }

          continue;
        }

        // ignore the result of processAll, try to read more bytes down below
        processAll();
        compactify();
      }
    } catch (boost::system::system_error& err) {
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "i/o stream failed with: "
        << err.what();

      closeStreamNoLock();
      return;
    } catch (...) {
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "general error on stream";

      closeStreamNoLock();
      return;
    }
  }

  // try to read more bytes
  if (!_abandoned) {
    if (!reserveMemory()) {
      LOG_TOPIC(TRACE, Logger::COMMUNICATION) << "failed to reserve memory";
      return;
    }

    auto self = shared_from_this();

    // WARNING: the _readBuffer MUST NOT be changed until the callback
    // has been called! Otherwise ASIO will get confused and write to
    // the wrong position.

    _peer->asyncRead(
        boost::asio::buffer(_readBuffer.end(), READ_BLOCK_SIZE),
        [self, this](const boost::system::error_code& ec,
                     std::size_t transferred) {
          JobGuard guard(_loop);
          guard.work();

          MUTEX_LOCKER(locker, _lock);

          if (_abandoned) {
            return;
          }

          if (ec) {
            LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
                << "read on stream failed with: " << ec.message();
            closeStreamNoLock();
          } else {
            _readBuffer.increaseLength(transferred);

            if (processAll()) {
              _loop._scheduler->post([self, this]() { asyncReadSome(); });
            }

            compactify();
          }
        });
  }
}
