////////////////////////////////////////////////////////////////////////////////
/// @brief base class for input-output tasks from sockets
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "SocketTask.h"

#include <errno.h>

#include "Basics/MutexLocker.h"
#include "Basics/StringBuffer.h"
#include "Basics/logging.h"
#include "Basics/socket-utils.h"
#include "Scheduler/Scheduler.h"

using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new task with a given socket
////////////////////////////////////////////////////////////////////////////////

SocketTask::SocketTask (TRI_socket_t socket, double keepAliveTimeout)
  : Task("SocketTask"),
    keepAliveWatcher(0),
    readWatcher(0),
    writeWatcher(0),
    watcher(0),
    _commSocket(socket),
    _keepAliveTimeout(keepAliveTimeout),
    _writeBuffer(nullptr),
#ifdef TRI_ENABLE_FIGURES
    _writeBufferStatistics(0),
#endif
    ownBuffer(true),
    writeLength(0),
    _readBuffer(nullptr),
    tid(0) {

  _readBuffer = new StringBuffer(TRI_UNKNOWN_MEM_ZONE);

  ConnectionStatisticsAgent::acquire();
  ConnectionStatisticsAgentSetStart(this);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a socket task
///
/// This method will close the underlying socket.
////////////////////////////////////////////////////////////////////////////////

SocketTask::~SocketTask () {
  if (TRI_isvalidsocket(_commSocket)) {
    TRI_CLOSE_SOCKET(_commSocket);
    TRI_invalidatesocket(&_commSocket);
  }

  if (_writeBuffer != nullptr && ownBuffer) {
    delete _writeBuffer;
  }

#ifdef TRI_ENABLE_FIGURES

  if (_writeBufferStatistics != 0) {
    TRI_ReleaseRequestStatistics(_writeBufferStatistics);
  }

#endif

  delete _readBuffer;

  ConnectionStatisticsAgentSetEnd(this);
  ConnectionStatisticsAgent::release();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void SocketTask::setKeepAliveTimeout (double timeout) {
  if (keepAliveWatcher != 0 && timeout > 0.0) {
    _scheduler->rearmTimer(keepAliveWatcher, timeout);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                         protected virtual methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the read buffer
////////////////////////////////////////////////////////////////////////////////

bool SocketTask::fillReadBuffer (bool& closed) {
  closed = false;

  // reserve some memory for reading
  if (_readBuffer->reserve(READ_BLOCK_SIZE) == TRI_ERROR_OUT_OF_MEMORY) {
    // out of memory
    LOG_TRACE("out of memory");

    return false;
  }

  int nr = TRI_READ_SOCKET(_commSocket, _readBuffer->end(), READ_BLOCK_SIZE, 0);

  if (nr > 0) {
    _readBuffer->increaseLength(nr);
    return true;
  }
  else if (nr == 0) {
    closed = true;

    LOG_TRACE("read returned 0");

    return false;
  }
  else {
    if (errno == EINTR) {
      return fillReadBuffer(closed);
    }
    else if (errno != EWOULDBLOCK) {
      LOG_TRACE("read failed with %d: %s", (int) errno, strerror(errno));

      return false;
    }
    else {
      LOG_TRACE("read would block with %d: %s", (int) errno, strerror(errno));

      return true;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a write
////////////////////////////////////////////////////////////////////////////////

bool SocketTask::handleWrite (bool& closed) {
  closed = false;

  bool callCompletedWriteBuffer = false;

  {
    MUTEX_LOCKER(_writeBufferLock);

    // size_t is unsigned, should never get < 0
    size_t len = 0;

    if (nullptr != _writeBuffer) {
      TRI_ASSERT(_writeBuffer->length() >= writeLength);
      len = _writeBuffer->length() - writeLength;
    }

    int nr = 0;

    if (0 < len) {
      nr = TRI_WRITE_SOCKET(_commSocket, _writeBuffer->begin() + writeLength, (int) len, 0);

      if (nr < 0) {
        if (errno == EINTR) {
          return handleWrite(closed);
        }
        else if (errno != EWOULDBLOCK) {
          LOG_DEBUG("write failed with %d: %s", (int) errno, strerror(errno));

          return false;
        }
        else {
          nr = 0;
        }
      }

      len -= nr;
    }

    if (len == 0) {
      if (nullptr != _writeBuffer && ownBuffer) {
        delete _writeBuffer;
      }

      callCompletedWriteBuffer = true;
    }
    else {
      writeLength += nr;
    }
  }

  // we have to release the lock, before calling completedWriteBuffer
  if (callCompletedWriteBuffer) {
    completedWriteBuffer(closed);

    if (closed) {
      return false;
    }

    // rearm timer for keep-alive timeout
    // TODO: do we need some lock before we modify the scheduler?
    setKeepAliveTimeout(_keepAliveTimeout);
  }

  // we might have a new write buffer or none at all
  {
    MUTEX_LOCKER(_writeBufferLock);

    if (_writeBuffer == nullptr) {
      _scheduler->stopSocketEvents(writeWatcher);
    }
    else {
      _scheduler->startSocketEvents(writeWatcher);
    }
  }

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief sets an active write buffer
////////////////////////////////////////////////////////////////////////////////

void SocketTask::setWriteBuffer (StringBuffer* buffer, TRI_request_statistics_t* statistics, bool ownBuffer) {
  bool callCompletedWriteBuffer = false;

  {
    MUTEX_LOCKER(_writeBufferLock);

#ifdef TRI_ENABLE_FIGURES

    _writeBufferStatistics = statistics;

    if (_writeBufferStatistics != 0) {
      _writeBufferStatistics->_writeStart = TRI_StatisticsTime();
      _writeBufferStatistics->_sentBytes += buffer->length();
    }

#endif

    writeLength = 0;

    if (buffer->empty()) {
      if (ownBuffer) {
        delete buffer;
      }

      callCompletedWriteBuffer = true;
    }
    else {
      if (_writeBuffer != nullptr) {
        if (this->ownBuffer) {
          delete _writeBuffer;
        }

      }

      _writeBuffer = buffer;
      this->ownBuffer = ownBuffer;
    }
  }

  // we have to release the lock before calling completedWriteBuffer
  if (callCompletedWriteBuffer) {
    bool closed;
    completedWriteBuffer(closed);
  }

  // we might have a new write buffer or none at all
  if (tid == Thread::currentThreadId()) {
    MUTEX_LOCKER(_writeBufferLock);

    if (_writeBuffer == 0) {
      _scheduler->stopSocketEvents(writeWatcher);
    }
    else {
      _scheduler->startSocketEvents(writeWatcher);
    }
  }
  else {
    _scheduler->sendAsync(watcher);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends or creates an active write buffer
////////////////////////////////////////////////////////////////////////////////

void SocketTask::appendWriteBuffer (StringBuffer* buffer) {
  if (buffer->empty()) {
    return;
  }

  bool newBuffer = false;

  {
    MUTEX_LOCKER(_writeBufferLock);

    if (_writeBuffer == nullptr) {
      writeLength = 0;

      _writeBuffer = new StringBuffer(TRI_UNKNOWN_MEM_ZONE);

      newBuffer = true;
    }

    _writeBuffer->appendText(*buffer);
  }

  // we might have a new write buffer
  if (newBuffer) {
    if (tid == Thread::currentThreadId()) {
      MUTEX_LOCKER(_writeBufferLock);

      _scheduler->startSocketEvents(writeWatcher);
    }
    else {
      _scheduler->sendAsync(watcher);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks for presence of an active write buffer
////////////////////////////////////////////////////////////////////////////////

bool SocketTask::hasWriteBuffer () const {
  MUTEX_LOCKER(_writeBufferLock);

  return _writeBuffer != nullptr;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                      Task methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool SocketTask::setup (Scheduler* scheduler, EventLoop loop) {

#ifdef _WIN32
  // ..........................................................................
  // The problem we have here is that this opening of the fs handle may fail.
  // There is no mechanism to the calling function to report failure.
  // ..........................................................................
  LOG_TRACE("attempting to convert socket handle to socket descriptor");

  if (! TRI_isvalidsocket(_commSocket)) {
    LOG_ERROR("In SocketTask::setup could not convert socket handle to socket descriptor -- invalid socket handle");
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
    LOG_ERROR("In SocketTask::setup could not convert socket handle to socket descriptor -- _open_osfhandle(...) failed");
    res = TRI_CLOSE_SOCKET(_commSocket);

    if (res != 0) {
      res = WSAGetLastError();
      LOG_ERROR("In SocketTask::setup closesocket(...) failed with error code: %d", (int) res);
    }

    TRI_invalidatesocket(&_commSocket);
    return false;
  }

  _commSocket.fileDescriptor = res;

#endif


  this->_scheduler = scheduler;
  this->_loop = loop;

  watcher = _scheduler->installAsyncEvent(loop, this);
  readWatcher = _scheduler->installSocketEvent(loop, EVENT_SOCKET_READ, this, _commSocket);
  writeWatcher = _scheduler->installSocketEvent(loop, EVENT_SOCKET_WRITE, this, _commSocket);

  if (readWatcher == -1 || writeWatcher == -1) {
    return false;
  }

  // install timer for keep-alive timeout with some high default value
  keepAliveWatcher = _scheduler->installTimerEvent(loop, this, 60.0);

  // and stop it immediately so it's not actively at the start
  _scheduler->clearTimer(keepAliveWatcher);

  tid = Thread::currentThreadId();
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void SocketTask::cleanup () {
  if (_scheduler == 0) {
    LOG_WARNING("In SocketTask::cleanup the scheduler has disappeared -- invalid pointer");
    watcher = 0;
    keepAliveWatcher = 0;
    readWatcher = 0;
    writeWatcher = 0;
    return;
  }

  _scheduler->uninstallEvent(watcher);
  watcher = 0;

  _scheduler->uninstallEvent(keepAliveWatcher);
  keepAliveWatcher = 0;

  _scheduler->uninstallEvent(readWatcher);
  readWatcher = 0;

  _scheduler->uninstallEvent(writeWatcher);
  writeWatcher = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool SocketTask::handleEvent (EventToken token, EventType revents) {
  bool result = true;
  bool closed = false;

  if (token == keepAliveWatcher && (revents & EVENT_TIMER)) {
    // got a keep-alive timeout
    LOG_TRACE("got keep-alive timeout signal, closing connection");

    // TODO: do we need some lock before we modify the scheduler?
    _scheduler->clearTimer(token);

    // this will close the connection and destroy the task
    handleTimeout();
    return false;
  }

  if (token == readWatcher && (revents & EVENT_SOCKET_READ)) {
    if (keepAliveWatcher != 0) {
      // disable timer for keep-alive timeout
      _scheduler->clearTimer(keepAliveWatcher);
    }

    result = handleRead(closed);
  }

  if (result && ! closed && token == writeWatcher) {
    if (revents & EVENT_SOCKET_WRITE) {
      result = handleWrite(closed);
    }
  }

  if (result) {
    MUTEX_LOCKER(_writeBufferLock);

    if (_writeBuffer == nullptr) {
      _scheduler->stopSocketEvents(writeWatcher);
    }
    else {
      _scheduler->startSocketEvents(writeWatcher);
    }
  }

  return result;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
