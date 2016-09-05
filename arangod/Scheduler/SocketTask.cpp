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
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#include "SocketTask.h"

#include <errno.h>

#include "Basics/StringBuffer.h"
#include "Basics/socket-utils.h"
#include "Logger/Logger.h"
#include "Scheduler/Scheduler.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

SocketTask::SocketTask(TRI_socket_t socket, ConnectionInfo&& info,
                       double keepAliveTimeout)
    : Task("SocketTask"),
      _keepAliveTimeout(keepAliveTimeout),
      _commSocket(socket),
      _connectionInfo(std::move(info)),
      _readBuffer(TRI_UNKNOWN_MEM_ZONE, READ_BLOCK_SIZE + 1, false) {
  LOG(TRACE) << "connection established, client "
             << TRI_get_fd_or_handle_of_socket(socket) << ", server ip "
             << _connectionInfo.serverAddress << ", server port "
             << _connectionInfo.serverPort << ", client ip "
             << _connectionInfo.clientAddress << ", client port "
             << _connectionInfo.clientPort;
}

SocketTask::~SocketTask() {
  cleanup();

  if (TRI_isvalidsocket(_commSocket)) {
    TRI_CLOSE_SOCKET(_commSocket);
    TRI_invalidatesocket(&_commSocket);
  }

  delete _writeBuffer;

  if (_writeBufferStatistics != nullptr) {
    TRI_ReleaseRequestStatistics(_writeBufferStatistics);
  }

  for (auto& it : _writeBuffers) {
    delete it.first;
    TRI_ReleaseRequestStatistics(it.second);
  }

  LOG(TRACE) << "connection closed, client "
             << TRI_get_fd_or_handle_of_socket(_commSocket);
}

void SocketTask::armKeepAliveTimeout() {
  if (_keepAliveWatcher != nullptr && _keepAliveTimeout > 0.0) {
    _scheduler->rearmTimer(_keepAliveWatcher, _keepAliveTimeout);
  }
}

bool SocketTask::handleRead() {
  bool res = true;

  if (_closed) {
    return false;
  }

  if (!_closeRequested) {
    res = fillReadBuffer();

    // process as much data as we got; there might be more than one
    // request in the buffer
    while (processRead()) {
      if (_closeRequested) {
        break;
      }
    }
  } else {
    // if we don't close here, the scheduler thread may fall into a
    // busy wait state, consuming 100% CPU!
    _clientClosed = true;
  }

  if (_clientClosed) {
    res = false;
  } else if (!res) {
    _clientClosed = true;
  }

  return res;
}

bool SocketTask::fillReadBuffer() {
  // reserve some memory for reading
  if (_readBuffer.reserve(READ_BLOCK_SIZE + 1) == TRI_ERROR_OUT_OF_MEMORY) {
    _clientClosed = true;
    LOG(TRACE) << "out of memory";
    return false;
  }

  int nr = TRI_READ_SOCKET(_commSocket, _readBuffer.end(), READ_BLOCK_SIZE, 0);

  if (nr > 0) {
    _readBuffer.increaseLength(nr);
    _readBuffer.ensureNullTerminated();
    return true;
  }

  if (nr == 0) {
    LOG(TRACE) << "read returned 0";
    _clientClosed = true;
    return false;
  }

  int myerrno = errno;

  if (myerrno == EINTR) {
    // read interrupted by signal
    return fillReadBuffer();
  }

  // condition is required like this because g++ 6 will complain about
  //   if (myerrno != EWOULDBLOCK && myerrno != EAGAIN)
  // having two identical branches (because EWOULDBLOCK == EAGAIN on Linux).
  // however, posix states that there may be systems where EWOULDBLOCK !=
  // EAGAIN...
  if (myerrno != EWOULDBLOCK && (EWOULDBLOCK == EAGAIN || myerrno != EAGAIN)) {
    LOG(DEBUG) << "read from socket failed with " << myerrno << ": "
               << strerror(myerrno);
    _clientClosed = true;
    return false;
  }

  TRI_ASSERT(myerrno == EWOULDBLOCK ||
             (EWOULDBLOCK != EAGAIN && myerrno == EAGAIN));

  // from man(2) read:
  // The  file  descriptor  fd  refers  to  a socket and has been marked
  // nonblocking (O_NONBLOCK),
  // and the read would block.  POSIX.1-2001 allows
  // either error to be returned for this case, and does not require these
  // constants to have the same value,
  // so a  portable  application  should check for both possibilities.
  LOG(TRACE) << "read would block with " << myerrno << ": "
             << strerror(myerrno);

  return true;
}

bool SocketTask::handleWrite() {
  size_t len = 0;

  if (nullptr != _writeBuffer) {
    TRI_ASSERT(_writeBuffer->length() >= _writeLength);
    len = _writeBuffer->length() - _writeLength;
  }

  int nr = 0;

  if (0 < len) {
    nr = TRI_WRITE_SOCKET(_commSocket, _writeBuffer->begin() + _writeLength,
                          (int)len, 0);

    if (nr < 0) {
      int myerrno = errno;

      if (myerrno == EINTR) {
        // write interrupted by signal
        return handleWrite();
      }

      if (myerrno != EWOULDBLOCK &&
          (EAGAIN == EWOULDBLOCK || myerrno != EAGAIN)) {
        LOG(DEBUG) << "writing to socket failed with " << myerrno << ": "
                   << strerror(myerrno);

        return false;
      }

      TRI_ASSERT(myerrno == EWOULDBLOCK ||
                 (EWOULDBLOCK != EAGAIN && myerrno == EAGAIN));
      nr = 0;
    }

    TRI_ASSERT(nr >= 0);

    len -= nr;
  }

  if (len == 0) {
    completedWriteBuffer();

    // rearm timer for keep-alive timeout
    armKeepAliveTimeout();
  } else {
    _writeLength += nr;
  }

  if (_clientClosed) {
    return false;
  }

  // we might have a new write buffer or none at all
  if (_writeBuffer == nullptr) {
    _scheduler->stopSocketEvents(_writeWatcher);
  } else {
    _scheduler->startSocketEvents(_writeWatcher);
  }

  return true;
}

void SocketTask::addWriteBuffer(std::unique_ptr<basics::StringBuffer> buffer,
                                RequestStatisticsAgent* statistics) {
  TRI_request_statistics_t* stat =
      statistics == nullptr ? nullptr : statistics->steal();

  addWriteBuffer(buffer.release(), stat);
}

void SocketTask::addWriteBuffer(basics::StringBuffer* buffer,
                                TRI_request_statistics_t* stat) {
  if (_closed) {
    delete buffer;

    if (stat) {
      TRI_ReleaseRequestStatistics(stat);
    }

    return;
  }

  if (_writeBuffer != nullptr) {
    try {
      _writeBuffers.emplace_back(std::make_pair(buffer, stat));
      return;
    } catch (...) {
      delete buffer;
      if (stat) {
        TRI_ReleaseRequestStatistics(stat);
      }
      throw;
    }
  }

  _writeBuffer = buffer;
  _writeBufferStatistics = stat;

  _scheduler->startSocketEvents(_writeWatcher);
}

void SocketTask::completedWriteBuffer() {
  delete _writeBuffer;

  _writeBuffer = nullptr;
  _writeLength = 0;

  if (_writeBufferStatistics != nullptr) {
    _writeBufferStatistics->_writeEnd = TRI_StatisticsTime();
    TRI_ReleaseRequestStatistics(_writeBufferStatistics);
    _writeBufferStatistics = nullptr;
  }

  if (_writeBuffers.empty()) {
    if (_closeRequested) {
      _clientClosed = true;
    }

    return;
  }

  StringBuffer* buffer = _writeBuffers.front().first;
  TRI_request_statistics_t* statistics = _writeBuffers.front().second;
  _writeBuffers.pop_front();

  addWriteBuffer(buffer, statistics);
}

bool SocketTask::setup(Scheduler* scheduler, EventLoop loop) {
#ifdef _WIN32
  // ..........................................................................
  // The problem we have here is that this opening of the fs handle may fail.
  // There is no mechanism to the calling function to report failure.
  // ..........................................................................
  LOG(TRACE) << "attempting to convert socket handle to socket descriptor";

  if (!TRI_isvalidsocket(_commSocket)) {
    LOG(ERR) << "In SocketTask::setup could not convert socket handle to "
                "socket descriptor -- invalid socket handle";
    return false;
  }

  // For the official version of libev we would do this:
  // int res = _open_osfhandle(_commSocket.fileHandle, 0);
  // However, this opens a whole lot of problems and in general one should
  // never use _open_osfhandle for sockets.
  // Therefore, we do the following, although it has the potential to
  // lose the higher bits of the socket handle:
  int res = (int)_commSocket.fileHandle;

  if (res == -1) {
    LOG(ERR) << "In SocketTask::setup could not convert socket handle to "
                "socket descriptor -- _open_osfhandle(...) failed";
    res = TRI_CLOSE_SOCKET(_commSocket);

    if (res != 0) {
      res = WSAGetLastError();
      LOG(ERR)
          << "In SocketTask::setup closesocket(...) failed with error code: "
          << res;
    }

    TRI_invalidatesocket(&_commSocket);
    return false;
  }

  _commSocket.fileDescriptor = res;

#endif

  _scheduler = scheduler;
  _loop = loop;

  // install timer for keep-alive timeout with some high default value
  _keepAliveWatcher = _scheduler->installTimerEvent(loop, this, 60.0);

  // and stop it immediately so it's not active at the start
  _scheduler->clearTimer(_keepAliveWatcher);

  _tid = Thread::currentThreadId();

  TRI_ASSERT(_writeWatcher == nullptr);
  _writeWatcher = _scheduler->installSocketEvent(loop, EVENT_SOCKET_WRITE, this,
                                                 _commSocket);

  TRI_ASSERT(_readWatcher == nullptr);
  _readWatcher = _scheduler->installSocketEvent(loop, EVENT_SOCKET_READ, this,
                                                _commSocket);
  return true;
}

void SocketTask::cleanup() {
  if (_scheduler != nullptr) {
    if (_keepAliveWatcher != nullptr) {
      _scheduler->uninstallEvent(_keepAliveWatcher);
    }

    if (_readWatcher != nullptr) {
      _scheduler->uninstallEvent(_readWatcher);
    }

    if (_writeWatcher != nullptr) {
      _scheduler->uninstallEvent(_writeWatcher);
    }
  }

  _keepAliveWatcher = nullptr;
  _readWatcher = nullptr;
  _writeWatcher = nullptr;

  _closed = true;
  _clientClosed = false;
  _closeRequested = false;
}

bool SocketTask::handleEvent(EventToken token, EventType revents) {
  bool result = true;

  if (token == _keepAliveWatcher && (revents & EVENT_TIMER)) {
    // got a keep-alive timeout
    LOG(TRACE) << "got keep-alive timeout signal, closing connection";

    _scheduler->clearTimer(token);
    handleTimeout();
    _scheduler->destroyTask(this);

    return false;
  }

  if (token == _readWatcher && (revents & EVENT_SOCKET_READ)) {
    if (_keepAliveWatcher != nullptr) {
      // disable timer for keep-alive timeout
      _scheduler->clearTimer(_keepAliveWatcher);
    }

    result = handleRead();
  }

  if (result && !_clientClosed && token == _writeWatcher) {
    if (revents & EVENT_SOCKET_WRITE) {
      result = handleWrite();
    }
  }

  if (result) {
    if (_writeBuffer == nullptr) {
      _scheduler->stopSocketEvents(_writeWatcher);
    } else {
      _scheduler->startSocketEvents(_writeWatcher);
    }
  }

  if (_clientClosed) {
    _scheduler->destroyTask(this);
  }

  return result;
}
