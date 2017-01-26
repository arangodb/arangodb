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

//#define DEBUG_STATISTICS

#include "SocketTask.h"

#include "Basics/MutexLocker.h"
#include "Basics/StringBuffer.h"
#include "Basics/socket-utils.h"
#include "Endpoint/ConnectionInfo.h"
#include "Logger/Logger.h"
#include "Scheduler/EventLoop.h"
#include "Scheduler/JobGuard.h"
#include "Scheduler/Scheduler.h"

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
      _connectionInfo(connectionInfo),
      _readBuffer(TRI_UNKNOWN_MEM_ZONE, READ_BLOCK_SIZE + 1, false),
      _peer(std::move(socket)),
      _keepAliveTimeout(static_cast<long>(keepAliveTimeout * 1000)),
      _keepAliveTimer(_peer->_ioService, _keepAliveTimeout),
      _useKeepAliveTimer(keepAliveTimeout > 0.0),
      _keepAliveTimerActive(false),
      _closeRequested(false),
      _abandoned(false) {
  ConnectionStatisticsAgent::acquire();
  connectionStatisticsAgentSetStart();

  if (!skipInit) {
    _peer->setNonBlocking(true);
    if (!_peer->handshake()) {
      _closedSend = true;
      _closedReceive = true;
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

SocketTask::~SocketTask() {
  boost::system::error_code err;
  if (_keepAliveTimerActive) {
    _keepAliveTimer.cancel(err);
  }

  if (err) {
    LOG_TOPIC(ERR, Logger::COMMUNICATION) << "unable to cancel _keepAliveTimer";
  }
}

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

void SocketTask::addWriteBuffer(std::unique_ptr<StringBuffer> buffer,
                                RequestStatisticsAgent* statistics) {
  TRI_request_statistics_t* stat =
      statistics == nullptr ? nullptr : statistics->steal();

  addWriteBuffer(buffer.release(), stat);
}

void SocketTask::addWriteBuffer(StringBuffer* buffer,
                                TRI_request_statistics_t* stat) {
  if (_closedSend) {
    delete buffer;

    if (stat) {
      TRI_ReleaseRequestStatistics(stat);
    }

    return;
  }

  MUTEX_LOCKER(locker, _writeLock);

  // buffer and stats will be NULL when called from async handler
  if (buffer) {
    if (_writeBuffer != nullptr) {
      _writeBuffers.push_back(buffer);
      _writeBuffersStats.push_back(stat);
      return;
    }

    _writeBuffer = buffer;
    _writeBufferStatistics = stat;
  }

  if (_writeBuffer != nullptr) {
    size_t total = _writeBuffer->length();
    size_t written = 0;

    boost::system::error_code err;
    err.clear();

    while (true) {
      if (_writeBufferStatistics) {
        _writeBufferStatistics->_writeStart = TRI_StatisticsTime();
      }

      written = _peer->write(_writeBuffer, err);

      if (err) {
        break;
      }

      if (_writeBufferStatistics) {
        _writeBufferStatistics->_sentBytes += written;
      }

      if (written != total) {
        // unable to write everything at once, might be a lot of data
        // above code does not update the buffer positon
        break;
      }

      if (!completedWriteBuffer()) {
        return;
      }

      total = _writeBuffer->length();
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
    // was written in one go

    auto self = shared_from_this();

    // begin writing at offset (written)
    _peer->asyncWrite(
        boost::asio::buffer(_writeBuffer->begin() + written, total - written),
        [self, this](const boost::system::error_code& ec,
                     std::size_t transferred) {
          MUTEX_LOCKER(locker, _writeLock);

          if (_writeBufferStatistics) {
            _writeBufferStatistics->_sentBytes += transferred;
          }

          if (ec) {
            LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
                << "SocketTask::addWriteBuffer(async_write) - write on stream "
                << " failed with: " << ec.message();
            closeStream();
          } else {
            if (completedWriteBuffer()) {
              // completedWriteBuffer already advanced _writeBuffer and
              // _writeBufferStatistics
              locker.unlock();  // avoid recursive locking
              addWriteBuffer(nullptr, nullptr);
            }
          }
        });
  }
}

bool SocketTask::completedWriteBuffer() {
  delete _writeBuffer;
  _writeBuffer = nullptr;

  if (_writeBufferStatistics != nullptr) {
    _writeBufferStatistics->_writeEnd = TRI_StatisticsTime();
    TRI_ReleaseRequestStatistics(_writeBufferStatistics);
    _writeBufferStatistics = nullptr;
  }

  if (_writeBuffers.empty()) {
    if (_closeRequested) {
      closeStream();
    }

    return false;
  }

  _writeBuffer = _writeBuffers.front();
  _writeBuffers.pop_front();

  _writeBufferStatistics = _writeBuffersStats.front();
  _writeBuffersStats.pop_front();

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
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "SocketTask::trySyncRead "
                                            << "- peer disappeared ";
    return false;
  }

  if (0 == _peer->available(err)) {
    return false;
  }

  if (err) {
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "SocketTask::trySyncRead "
                                            << "- failed with "
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

void SocketTask::asyncReadSome() {
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
      double start_time = TRI_StatisticsTime();
      while (processRead(start_time)) {
        if (_abandoned) {
          return;
        }
        if (_closeRequested) {
          break;
        }
      }

      if (_closeRequested) {
        LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
            << "close requested, closing receive stream ";

        closeReceiveStream();
        return;
      }
    }
  } catch (boost::system::system_error& err) {
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
        << "SocketTask::asyncReadSome - i/o stream "
        << " failed with: " << err.what();

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
    if (ec) {
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
          << "SocketTask::asyncReadSome (async_read_some) - read on stream "
          << " failed with: " << ec.message();
      closeStream();
    } else {
      JobGuard guard(_loop);
      guard.busy();

      _readBuffer.increaseLength(transferred);

      double start_time = TRI_StatisticsTime();
      while (processRead(start_time)) {
        if (_closeRequested) {
          break;
        }
      }

      if (_closeRequested) {
        LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
            << "close requested, closing receive stream";

        closeReceiveStream();
      } else if (_abandoned) {
        return;
      } else {
        asyncReadSome();
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
