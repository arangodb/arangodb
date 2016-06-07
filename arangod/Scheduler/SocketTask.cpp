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

#include "Logger/Logger.h"
#include "Basics/StringBuffer.h"
#include "Basics/socket-utils.h"
#include "Scheduler/Scheduler.h"

#include <errno.h>

using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new task with a given socket
////////////////////////////////////////////////////////////////////////////////

SocketTask::SocketTask(TRI_socket_t socket, double keepAliveTimeout)
    : Task("SocketTask"),
      _keepAliveWatcher(nullptr),
      _readWatcher(nullptr),
      _writeWatcher(nullptr),
      _commSocket(socket),
      _keepAliveTimeout(keepAliveTimeout),
      _writeBuffer(nullptr),
      _writeBufferStatistics(nullptr),
      _writeLength(0),
      _readBuffer(nullptr),
      _clientClosed(false),
      _tid(0) {
  _readBuffer = new StringBuffer(TRI_UNKNOWN_MEM_ZONE, false);

  ConnectionStatisticsAgent::acquire();
  connectionStatisticsAgentSetStart();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a socket task
///
/// This method will close the underlying socket.
////////////////////////////////////////////////////////////////////////////////

SocketTask::~SocketTask() {
  if (TRI_isvalidsocket(_commSocket)) {
    TRI_CLOSE_SOCKET(_commSocket);
    TRI_invalidatesocket(&_commSocket);
  }

  delete _writeBuffer;

  if (_writeBufferStatistics != nullptr) {
    TRI_ReleaseRequestStatistics(_writeBufferStatistics);
  }

  delete _readBuffer;

  connectionStatisticsAgentSetEnd();
  ConnectionStatisticsAgent::release();
}

void SocketTask::setKeepAliveTimeout(double timeout) {
  if (_keepAliveWatcher != nullptr && timeout > 0.0) {
    _scheduler->rearmTimer(_keepAliveWatcher, timeout);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the read buffer
////////////////////////////////////////////////////////////////////////////////

bool SocketTask::fillReadBuffer() {
  // reserve some memory for reading
  if (_readBuffer->reserve(READ_BLOCK_SIZE + 1) == TRI_ERROR_OUT_OF_MEMORY) {
    // out of memory
    LOG(TRACE) << "out of memory";

    return false;
  }

  int nr = TRI_READ_SOCKET(_commSocket, _readBuffer->end(), READ_BLOCK_SIZE, 0);

  if (nr > 0) {
    _readBuffer->increaseLength(nr);
    _readBuffer->ensureNullTerminated();
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

  if (myerrno != EWOULDBLOCK && myerrno != EAGAIN) {
    LOG(DEBUG) << "read from socket failed with " << myerrno << ": " << strerror(myerrno);

    return false;
  }

  TRI_ASSERT(myerrno == EWOULDBLOCK || myerrno == EAGAIN);

  // from man(2) read:
  // The  file  descriptor  fd  refers  to  a socket and has been marked
  // nonblocking (O_NONBLOCK),
  // and the read would block.  POSIX.1-2001 allows
  // either error to be returned for this case, and does not require these
  // constants to have the same value,
  // so a  portable  application  should check for both possibilities.
  LOG(TRACE) << "read would block with " << myerrno << ": " << strerror(myerrno);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a write
////////////////////////////////////////////////////////////////////////////////

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

      if (myerrno != EWOULDBLOCK || myerrno != EAGAIN) {
        LOG(DEBUG) << "writing to socket failed with " << myerrno << ": " << strerror(myerrno);

        return false;
      }

      TRI_ASSERT(myerrno == EWOULDBLOCK || myerrno == EAGAIN);
      nr = 0;
    }

    TRI_ASSERT(nr >= 0);

    len -= nr;
  }

  if (len == 0) {
    delete _writeBuffer;
    _writeBuffer = nullptr;

    completedWriteBuffer();

    // rearm timer for keep-alive timeout
    setKeepAliveTimeout(_keepAliveTimeout);
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

////////////////////////////////////////////////////////////////////////////////
/// @brief sets an active write buffer
////////////////////////////////////////////////////////////////////////////////

void SocketTask::setWriteBuffer(StringBuffer* buffer,
                                TRI_request_statistics_t* statistics) {
  TRI_ASSERT(buffer != nullptr);

  _writeBufferStatistics = statistics;

  if (_writeBufferStatistics != nullptr) {
    _writeBufferStatistics->_writeStart = TRI_StatisticsTime();
    _writeBufferStatistics->_sentBytes += buffer->length();
  }

  _writeLength = 0;

  if (buffer->empty()) {
    delete buffer;

    completedWriteBuffer();
  } else {
    delete _writeBuffer;

    _writeBuffer = buffer;
  }

  if (_clientClosed) {
    return;
  }

  // we might have a new write buffer or none at all
  TRI_ASSERT(_tid == Thread::currentThreadId());

  if (_writeBuffer == nullptr) {
    _scheduler->stopSocketEvents(_writeWatcher);
  } else {
    _scheduler->startSocketEvents(_writeWatcher);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks for presence of an active write buffer
////////////////////////////////////////////////////////////////////////////////

bool SocketTask::hasWriteBuffer() const { return _writeBuffer != nullptr; }

bool SocketTask::setup(Scheduler* scheduler, EventLoop loop) {
#ifdef _WIN32
  // ..........................................................................
  // The problem we have here is that this opening of the fs handle may fail.
  // There is no mechanism to the calling function to report failure.
  // ..........................................................................
  LOG(TRACE) << "attempting to convert socket handle to socket descriptor";

  if (!TRI_isvalidsocket(_commSocket)) {
    LOG(ERR) << "In SocketTask::setup could not convert socket handle to socket descriptor -- invalid socket handle";
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
    LOG(ERR) << "In SocketTask::setup could not convert socket handle to socket descriptor -- _open_osfhandle(...) failed";
    res = TRI_CLOSE_SOCKET(_commSocket);

    if (res != 0) {
      res = WSAGetLastError();
      LOG(ERR) << "In SocketTask::setup closesocket(...) failed with error code: " << res;
    }

    TRI_invalidatesocket(&_commSocket);
    return false;
  }

  _commSocket.fileDescriptor = res;

#endif

  _scheduler = scheduler;
  _loop = loop;

  _readWatcher = _scheduler->installSocketEvent(loop, EVENT_SOCKET_READ, this,
                                                _commSocket);
  _writeWatcher = _scheduler->installSocketEvent(loop, EVENT_SOCKET_WRITE, this,
                                                 _commSocket);

  // install timer for keep-alive timeout with some high default value
  _keepAliveWatcher = _scheduler->installTimerEvent(loop, this, 60.0);

  // and stop it immediately so it's not active at the start
  _scheduler->clearTimer(_keepAliveWatcher);

  _tid = Thread::currentThreadId();

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cleans up the task by unregistering all watchers
////////////////////////////////////////////////////////////////////////////////

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
}

bool SocketTask::handleEvent(EventToken token, EventType revents) {
  bool result = true;

  if (token == _keepAliveWatcher && (revents & EVENT_TIMER)) {
    // got a keep-alive timeout
    LOG(TRACE) << "got keep-alive timeout signal, closing connection";

    _scheduler->clearTimer(token);

    // this will close the connection and destroy the task
    handleTimeout();
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

  return result;
}
