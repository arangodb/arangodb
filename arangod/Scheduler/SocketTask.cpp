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
////////////////////////////////////////////////////////////////////////////////

#include "SocketTask.h"

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
                       double keepAliveTimeout)
    : Task(loop, "SocketTask"),
      _connectionInfo(connectionInfo),
      _readBuffer(TRI_UNKNOWN_MEM_ZONE, READ_BLOCK_SIZE + 1, false),
      _peer(std::move(socket)),
      _keepAliveTimeout(static_cast<long>(keepAliveTimeout * 1000)),
      _useKeepAliveTimeout(static_cast<long>(keepAliveTimeout * 1000) > 0),
      _keepAliveTimer(_peer->_socket.get_io_service(), _keepAliveTimeout) {
  ConnectionStatisticsAgent::acquire();
  connectionStatisticsAgentSetStart();

  _peer->_socket.non_blocking(true);

  boost::system::error_code ec;

  if (ec) {
    LOG_TOPIC(ERR, Logger::COMMUNICATION)
        << "SocketTask:SocketTask - cannot create stream from socket: "
        << ec.message();
    _closedSend = true;
    _closedReceive = true;
  }

  if (_peer->_encrypted) {
    do {
      ec.assign(boost::system::errc::success,
                boost::system::generic_category());
      _peer->_sslSocket.handshake(
          boost::asio::ssl::stream_base::handshake_type::server, ec);
    } while (ec.value() == boost::asio::error::would_block);

    if (ec) {
      LOG_TOPIC(ERR, Logger::COMMUNICATION)
          << "SocketTask::SocketTask - unable to perform ssl handshake: "
          << ec.message() << " : " << ec.value();
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
  _keepAliveTimer.cancel(err);

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
      << "starting communication between server <-> client on socket: "
      << _peer->_socket.native_handle();
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
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "SocketTask::addWriteBuffer - "
                                               "dropping output, send stream "
                                               "already closed";

    delete buffer;
    if (stat) {
      LOG_TOPIC(TRACE, Logger::REQUESTS)
          << "SocketTask::addWriteBuffer - Statistics release: "
          << stat->to_string();
      TRI_ReleaseRequestStatistics(stat);
    } else {
      LOG_TOPIC(TRACE, Logger::REQUESTS) << "SocketTask::addWriteBuffer - "
                                            "Statistics release: nullptr - "
                                            "nothing to realease";
    }

    return;
  }

  if (_writeBuffer != nullptr) {
    _writeBuffers.push_back(buffer);
    _writeBuffersStats.push_back(stat);
    return;
  }

  _writeBuffer = buffer;
  _writeBufferStatistics = stat;  // threadsafe? why not pass to
                                  // completedWriteBuffer does this work with
                                  // async?

  if (_writeBuffer != nullptr) {
    boost::system::error_code ec;
    size_t total = _writeBuffer->length();
    size_t written = 0;

    boost::system::error_code err;
    do {
      ec.assign(boost::system::errc::success,
                boost::system::generic_category());
      if (!_peer->_encrypted) {
        written = _peer->_socket.write_some(
            boost::asio::buffer(_writeBuffer->begin(), _writeBuffer->length()),
            err);
      } else {
        written = _peer->_sslSocket.write_some(
            boost::asio::buffer(_writeBuffer->begin(), _writeBuffer->length()),
            err);
      }
      if (written == total) {
        completedWriteBuffer();
        return;
      }
    } while (err == boost::asio::error::would_block);

    if (err != boost::system::errc::success) {
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
          << "SocketTask::addWriteBuffer (write_some) - write on stream "
          << _peer->_socket.native_handle()
          << " failed with: " << err.message();
      closeStream();
      return;
    }

    auto self = shared_from_this();
    auto handler = [self, this](const boost::system::error_code& ec,
                                std::size_t transferred) {
      if (ec) {
        LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
            << "SocketTask::addWriterBuffer(async_write) - write on stream "
            << _peer->_socket.native_handle()
            << " failed with: " << ec.message();
        closeStream();
      } else {
        completedWriteBuffer();
      }
    };

    if (!_peer->_encrypted) {
      boost::asio::async_write(
          _peer->_socket,
          boost::asio::buffer(_writeBuffer->begin() + written, total - written),
          handler);
    } else {
      boost::asio::async_write(
          _peer->_sslSocket,
          boost::asio::buffer(_writeBuffer->begin() + written, total - written),
          handler);
    }
  }
}

void SocketTask::completedWriteBuffer() {
  delete _writeBuffer;
  _writeBuffer = nullptr;

  if (_writeBufferStatistics != nullptr) {
    LOG_TOPIC(TRACE, Logger::REQUESTS)
        << "SocketTask::addWriteBuffer - Statistics release: "
        << _writeBufferStatistics->to_string();
    _writeBufferStatistics->_writeEnd = TRI_StatisticsTime();
    TRI_ReleaseRequestStatistics(_writeBufferStatistics);
    _writeBufferStatistics = nullptr;
  } else {
    LOG_TOPIC(TRACE, Logger::REQUESTS) << "SocketTask::addWriteBuffer - "
                                          "Statistics release: nullptr - "
                                          "nothing to realease";
  }

  if (_writeBuffers.empty()) {
    if (_closeRequested) {
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "SocketTask::"
                                                 "completedWriteBuffer - close "
                                                 "requested, closing stream";

      closeStream();
    }

    return;
  }

  StringBuffer* buffer = _writeBuffers.front();
  _writeBuffers.pop_front();

  TRI_request_statistics_t* statistics = _writeBuffersStats.front();
  _writeBuffersStats.pop_front();

  addWriteBuffer(buffer, statistics);
}

void SocketTask::closeStream() {
  boost::system::error_code err;

  if (!_closedSend) {
    _peer->_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, err);

    if (err && err != boost::asio::error::not_connected) {
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
          << "SocketTask::closeStream - shutdown send stream "
          << _peer->_socket.native_handle()
          << " failed with: " << err.message();
    }

    _closedSend = true;
  }

  if (!_closedReceive) {
    _peer->_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_receive,
                            err);
    if (err && err != boost::asio::error::not_connected) {
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
          << "SocketTask::CloseStream - shutdown send stream "
          << _peer->_socket.native_handle()
          << " failed with: " << err.message();
    }

    _closedReceive = true;
  }

  _peer->_socket.close(err);

  if (err && err != boost::asio::error::not_connected) {
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
        << "SocketTask::CloseStream - shutdown send stream "
        << _peer->_socket.native_handle() << " failed with: " << err.message();
  }

  _closeRequested = false;
  _keepAliveTimer.cancel();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

void SocketTask::resetKeepAlive() {
  if (_useKeepAliveTimeout) {
    boost::system::error_code err;
    _keepAliveTimer.expires_from_now(_keepAliveTimeout, err);

    if (err) {
      closeStream();
      return;
    }

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
  if (_useKeepAliveTimeout) {
    boost::system::error_code err;
    _keepAliveTimer.cancel(err);
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

  if (0 == _peer->_socket.available(err)) {
    return false;
  }

  if (err) {
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "SocketTask::trySyncRead "
                                            << "- failed with "
                                            << err.message();
    return false;
  }

  size_t bytesRead = 0;

  if (!_peer->_encrypted) {
    bytesRead = _peer->_socket.read_some(
        boost::asio::buffer(_readBuffer.end(), READ_BLOCK_SIZE), err);
  } else {
    bytesRead = _peer->_sslSocket.read_some(
        boost::asio::buffer(_readBuffer.end(), READ_BLOCK_SIZE), err);
  }

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
  auto info = _peer->_socket.native_handle();

  try {
    JobGuard guard(_loop);
    guard.enterLoop();

    size_t const MAX_DIRECT_TRIES = 2;
    size_t n = 0;

    while (++n <= MAX_DIRECT_TRIES) {
      if (!reserveMemory()) {
        LOG_TOPIC(TRACE, Logger::COMMUNICATION)
            << "failed to reserve memory for " << info;
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

      while (processRead()) {
        if (_closeRequested) {
          break;
        }
      }

      if (_closeRequested) {
        LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
            << "close requested, closing receive stream " << info;

        closeReceiveStream();
        return;
      }
    }
  } catch (boost::system::system_error& err) {
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
        << "SocketTask::asyncReadSome - i/o stream " << info
        << " failed with: " << err.what();

    closeStream();
    return;
  } catch (...) {
    LOG_TOPIC(DEBUG, Logger::COMMUNICATION) << "general error on stream "
                                            << info;

    closeStream();
    return;
  }

  // try to read more bytes
  if (!reserveMemory()) {
    LOG_TOPIC(TRACE, Logger::COMMUNICATION) << "failed to reserve memory for "
                                            << info;
    return;
  }

  auto self = shared_from_this();
  auto handler = [self, this, info](const boost::system::error_code& ec,
                                    std::size_t transferred) {
    if (ec) {
      LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
          << "SocketTask::asyncReadSome (async_read_some) - read on stream "
          << info << " failed with: " << ec.message();
      closeStream();
    } else {
      JobGuard guard(_loop);
      guard.enterLoop();

      _readBuffer.increaseLength(transferred);

      while (processRead()) {
        if (_closeRequested) {
          break;
        }
      }

      if (_closeRequested) {
        LOG_TOPIC(DEBUG, Logger::COMMUNICATION)
            << "close requested, closing receive stream";

        closeReceiveStream();
      } else {
        asyncReadSome();
      }
    }
  };

  if (!_peer->_encrypted) {
    _peer->_socket.async_read_some(
        boost::asio::buffer(_readBuffer.end(), READ_BLOCK_SIZE), handler);
  } else {
    _peer->_sslSocket.async_read_some(
        boost::asio::buffer(_readBuffer.end(), READ_BLOCK_SIZE), handler);
  }
}

void SocketTask::closeReceiveStream() {
  auto info = _peer->_socket.native_handle();

  if (!_closedReceive) {
    try {
      _peer->_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_receive);
    } catch (boost::system::system_error& err) {
      LOG(WARN) << "shutdown receive stream " << info
                << " failed with: " << err.what();
    }

    _closedReceive = true;
  }
}
