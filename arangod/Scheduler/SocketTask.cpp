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
      _connectionStatistics(nullptr),
      _connectionInfo(connectionInfo),
      _readBuffer(TRI_UNKNOWN_MEM_ZONE, READ_BLOCK_SIZE + 1, false),
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

  boost::system::error_code err;

  if (_keepAliveTimerActive) {
    _keepAliveTimer.cancel(err);
  }

  if (err) {
    LOG_TOPIC(ERR, Logger::COMMUNICATION) << "unable to cancel _keepAliveTimer";
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
  _loop._ioService->post([self, this]() { asyncReadSome(); });
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

void SocketTask::addWriteBuffer(WriteBuffer& buffer) {
  if (_closedSend) {
    buffer.release();
    return;
  }

  {
    auto self = shared_from_this();

    _loop._ioService->post([self, this]() {
      MUTEX_LOCKER(locker, _readLock);
      processAll();
    });
  }

  MUTEX_LOCKER(locker, _writeLock);

  if (!buffer.empty()) {
    if (!_writeBuffer.empty()) {
      _writeBuffers.emplace_back(std::move(buffer));
      return;
    }

    _writeBuffer = std::move(buffer);
  }

  writeWriteBuffer();
}

void SocketTask::writeWriteBuffer() {
  if (_writeBuffer.empty()) {
    return;
  }

  size_t total = _writeBuffer._buffer->length();
  size_t written = 0;

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
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
        << "SocketTask::addWriteBuffer (write_some) - write on stream "
        << " failed with: " << err.message();
    closeStream();
    return;
  }

  // so the code could have blocked at this point or not all data
  // was written in one go, begin writing at offset (written)
  auto self = shared_from_this();
  _peer->asyncWrite(
      boost::asio::buffer(_writeBuffer._buffer->begin() + written, total - written),
      [self, this](const boost::system::error_code& ec,
                   std::size_t transferred) {
        MUTEX_LOCKER(locker, _writeLock);

        RequestStatistics::ADD_SENT_BYTES(_writeBuffer._statistics, transferred);

        if (ec) {
          LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
              << "SocketTask::addWriteBuffer(async_write) - write on stream "
              << " failed with: " << ec.message();
          closeStream();
        } else {
          if (completedWriteBuffer()) {
            _loop._ioService->post([self, this]() { writeWriteBuffer(); });
          }
        }
      });
}

bool SocketTask::completedWriteBuffer() {
  RequestStatistics::SET_WRITE_END(_writeBuffer._statistics);
  _writeBuffer.release();

  if (_writeBuffers.empty()) {
    if (_closeRequested) {
      closeStream();
    }

    return false;
  }

  _writeBuffer = std::move(_writeBuffers.front());
  _writeBuffers.pop_front();

  return true;
}

void SocketTask::closeStream() {
  boost::system::error_code err;

  if (!_closedSend) {
    _peer->shutdownSend(err);

    if (err && err != boost::asio::error::not_connected) {
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
          << "SocketTask::closeStream - shutdown send stream "
          << " failed with: " << err.message();
    }

    _closedSend = true;
  }

  if (!_closedReceive) {
    _peer->shutdownReceive(err);

    if (err && err != boost::asio::error::not_connected) {
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
          << "SocketTask::CloseStream - shutdown send stream "
          << " failed with: " << err.message();
    }

    _closedReceive = true;
  }

  _peer->close(err);

  if (err && err != boost::asio::error::not_connected) {
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
        << "SocketTask::CloseStream - shutdown send stream "
        << "failed with: " << err.message();
  }

  _closeRequested = false;
  _keepAliveTimer.cancel();
  _keepAliveTimerActive = false;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

void SocketTask::addToReadBuffer(char const* data, std::size_t len) {
  LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << std::string(data, len);
  _readBuffer.appendText(data, len);
}

void SocketTask::resetKeepAlive() {
  if (_useKeepAliveTimer) {
    boost::system::error_code err;
    _keepAliveTimer.expires_from_now(_keepAliveTimeout, err);

    if (err) {
      closeStream();
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

void SocketTask::cancelKeepAlive() {
  if (_useKeepAliveTimer && _keepAliveTimerActive) {
    boost::system::error_code err;
    _keepAliveTimer.cancel(err);
    _keepAliveTimerActive = false;
  }
}

bool SocketTask::reserveMemory() {
  if (_readBuffer.reserve(READ_BLOCK_SIZE + 1) == TRI_ERROR_OUT_OF_MEMORY) {
    LOG(WARN) << "out of memory while reading from client";
    closeStream();
    return false;
  }

  return true;
}

bool SocketTask::trySyncRead() {
  boost::system::error_code err;

  if (_abandoned) {
    return false;
  }

  if (!_peer) {
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "peer disappeared ";
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

  size_t bytesRead = 0;

  bytesRead =
      _peer->read(boost::asio::buffer(_readBuffer.end(), READ_BLOCK_SIZE), err);

  if (0 == bytesRead) {
    return false;  // should not happen
  }

  _readBuffer.increaseLength(bytesRead);

  if (err) {
    if (err == boost::asio::error::would_block) {
      return false;
    } else {
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
          << "SocketTask::trySyncRead "
          << "- failed with: " << err.message();
      return false;
    }
  }

  return true;
}

bool SocketTask::processAll() {
  double start_time = StatisticsFeature::time();

  while (processRead(start_time)) {
    if (_abandoned) {
      return false;
    }

    if (_closeRequested) {
      break;
    }
  }

  if (_closeRequested) {
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
        << "close requested, closing receive stream ";

    closeReceiveStream();
    return false;
  }

  return true;
}

void SocketTask::asyncReadSome() {
  MUTEX_LOCKER(locker, _readLock);

  try {
    if (_abandoned) {
      return;
    }

    JobGuard guard(_loop);
    guard.busy();

    size_t const MAX_DIRECT_TRIES = 2;
    size_t n = 0;

    while (++n <= MAX_DIRECT_TRIES) {
      if (!reserveMemory()) {
        LOG_TOPIC(TRACE, Logger::COMMUNICATION) << "failed to reserve memory";
        return;
      }

      if (!trySyncRead()) {
        if (n < MAX_DIRECT_TRIES) {
#ifdef TRI_HAVE_SCHED_H
          sched_yield();
#endif
        }

        continue;
      }

      if (!processAll()) {
        return;
      }
    }
  } catch (boost::system::system_error& err) {
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "i/o stream failed with: "
                                            << err.what();

    closeStream();
    return;
  } catch (...) {
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "general error on stream";

    closeStream();
    return;
  }

  // try to read more bytes
  if (!reserveMemory()) {
    LOG_TOPIC(TRACE, Logger::COMMUNICATION) << "failed to reserve memory";
    return;
  }

  auto self = shared_from_this();
  auto handler = [self, this](const boost::system::error_code& ec,
                              std::size_t transferred) {
    MUTEX_LOCKER(locker, _readLock);

    if (ec) {
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "read on stream failed with: "
                                              << ec.message();
      closeStream();
    } else {
      JobGuard guard(_loop);
      guard.busy();

      _readBuffer.increaseLength(transferred);

      if (processAll()) {
        _loop._ioService->post([self, this]() { asyncReadSome(); });
      }
    }
  };

  if (!_abandoned && _peer) {
    _peer->asyncRead(boost::asio::buffer(_readBuffer.end(), READ_BLOCK_SIZE),
                     handler);
  }
}

void SocketTask::closeReceiveStream() {
  if (!_closedReceive) {
    try {
      _peer->shutdownReceive();
    } catch (boost::system::system_error& err) {
      LOG(WARN) << "shutdown receive stream "
                << " failed with: " << err.what();
    }

    _closedReceive = true;
  }
}
