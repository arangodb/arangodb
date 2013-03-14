////////////////////////////////////////////////////////////////////////////////
/// @brief base class for input-output tasks from sockets
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Achim Brandt
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "SocketTask.h"

#include <errno.h>

#include "Basics/MutexLocker.h"
#include "Basics/StringBuffer.h"
#include "Logger/Logger.h"
#include "Scheduler/Scheduler.h"
#include "BasicsC/socket-utils.h"

using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

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
    _writeBuffer(0),
#ifdef TRI_ENABLE_FIGURES
    _writeBufferStatistics(0),
#endif
    ownBuffer(true),
    writeLength(0) {

  _readBuffer = new StringBuffer(TRI_UNKNOWN_MEM_ZONE);
  tmpReadBuffer = new char[READ_BLOCK_SIZE];

  ConnectionStatisticsAgent::acquire();
  ConnectionStatisticsAgentSetStart(this);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a socket task
///
/// This method will close the underlying socket.
////////////////////////////////////////////////////////////////////////////////

SocketTask::~SocketTask () {
  if (_commSocket.fileHandle != -1) {
    TRI_CLOSE_SOCKET(_commSocket);
    _commSocket.fileDescriptor = -1;
    _commSocket.fileHandle = -1;
  }

  if (_writeBuffer != 0) {
    if (ownBuffer) {
      delete _writeBuffer;
    }
  }

#ifdef TRI_ENABLE_FIGURES

  if (_writeBufferStatistics != 0) {
    TRI_ReleaseRequestStatistics(_writeBufferStatistics);
  }

#endif

  delete _readBuffer;

  delete[] tmpReadBuffer;

  ConnectionStatisticsAgentSetEnd(this);
  ConnectionStatisticsAgent::release();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void SocketTask::setKeepAliveTimeout (double timeout) {
  if (keepAliveWatcher != 0 && timeout > 0.0) {
    scheduler->rearmTimer(keepAliveWatcher, timeout);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                         protected virtual methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the read buffer
////////////////////////////////////////////////////////////////////////////////

bool SocketTask::fillReadBuffer (bool& closed) {
  closed = false;


  int nr = TRI_READ_SOCKET(_commSocket, tmpReadBuffer, READ_BLOCK_SIZE, 0);


  if (nr > 0) {
    _readBuffer->appendText(tmpReadBuffer, nr);
    return true;
  }
  else if (nr == 0) {
    closed = true;

    LOGGER_TRACE("read returned 0");

    return false;
  }
  else {

    if (errno == EINTR) {
      return fillReadBuffer(closed);
    }
    else if (errno != EWOULDBLOCK) {
      LOGGER_TRACE("read failed with " << errno << " (" << strerror(errno) << ")");

      return false;
    }
    else {
      LOGGER_TRACE("read would block with " << errno << " (" << strerror(errno) << ")");

      return true;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a write
////////////////////////////////////////////////////////////////////////////////

bool SocketTask::handleWrite (bool& closed, bool noWrite) {

  closed = false;

  if (noWrite) {
    return true;
  }

  bool callCompletedWriteBuffer = false;

  {
    MUTEX_LOCKER(writeBufferLock);

    // size_t is unsigned, should never get < 0
    assert(_writeBuffer->length() >= writeLength);
    size_t len = _writeBuffer->length() - writeLength;

    int nr = 0;

    if (0 < len) {
      nr = TRI_WRITE_SOCKET(_commSocket, _writeBuffer->begin() + writeLength, (int) len, 0);

      if (nr < 0) {
        if (errno == EINTR) {
          return handleWrite(closed, noWrite);
        }
        else if (errno != EWOULDBLOCK) {
          LOGGER_DEBUG("write failed with " << errno << " (" << strerror(errno) << ")");

          return false;
        }
        else {
          nr = 0;
        }
      }

      len -= nr;
    }

    if (len == 0) {
      if (ownBuffer) {
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
    MUTEX_LOCKER(writeBufferLock);

    if (_writeBuffer == 0) {
      scheduler->stopSocketEvents(writeWatcher);
    }
    else {
      scheduler->startSocketEvents(writeWatcher);
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief sets an active write buffer
////////////////////////////////////////////////////////////////////////////////

void SocketTask::setWriteBuffer (StringBuffer* buffer, TRI_request_statistics_t* statistics, bool ownBuffer) {
  bool callCompletedWriteBuffer = false;

  {
    MUTEX_LOCKER(writeBufferLock);

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
      if (_writeBuffer != 0) {
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
    MUTEX_LOCKER(writeBufferLock);

    if (_writeBuffer == 0) {
      scheduler->stopSocketEvents(writeWatcher);
    }
    else {
      scheduler->startSocketEvents(writeWatcher);
    }
  }
  else {
    scheduler->sendAsync(watcher);
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
    MUTEX_LOCKER(writeBufferLock);

    if (_writeBuffer == 0) {
      writeLength = 0;

      _writeBuffer = new StringBuffer(TRI_UNKNOWN_MEM_ZONE);

      newBuffer = true;
    }

    _writeBuffer->appendText(*buffer);
  }

  // we might have a new write buffer
  if (newBuffer) {
    if (tid == Thread::currentThreadId()) {
      MUTEX_LOCKER(writeBufferLock);

      scheduler->startSocketEvents(writeWatcher);
    }
    else {
      scheduler->sendAsync(watcher);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks for presence of an active write buffer
////////////////////////////////////////////////////////////////////////////////

bool SocketTask::hasWriteBuffer () const {
  MUTEX_LOCKER(writeBufferLock);

  return _writeBuffer != 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      Task methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool SocketTask::setup (Scheduler* scheduler, EventLoop loop) {

#ifdef _WIN32
  // ..........................................................................
  // The problem we have here is that this opening of the fs handle may fail.
  // There is no mechanism to the calling function to report failure.
  // ..........................................................................
  LOGGER_TRACE("attempting to convert socket handle to socket descriptor");

  if (_commSocket.fileHandle < 1) {
    LOGGER_ERROR("In SocketTask::setup could not convert socket handle to socket descriptor -- invalid socket handle");
    _commSocket.fileHandle = -1;
    _commSocket.fileDescriptor = -1;
    return false;
  }

  _commSocket.fileDescriptor = _open_osfhandle (_commSocket.fileHandle, 0);
  if (_commSocket.fileDescriptor == -1) {
    LOGGER_ERROR("In SocketTask::setup could not convert socket handle to socket descriptor -- _open_osfhandle(...) failed");
    int res = closesocket(_commSocket.fileHandle);
    if (res != 0) {
      res = WSAGetLastError();
      LOGGER_ERROR("In SocketTask::setup closesocket(...) failed with error code: " << res);
    }
    _commSocket.fileHandle = -1;
    _commSocket.fileDescriptor = -1;
    return false;
  }

#endif


  this->scheduler = scheduler;
  this->loop = loop;

  watcher = scheduler->installAsyncEvent(loop, this);
  readWatcher = scheduler->installSocketEvent(loop, EVENT_SOCKET_READ, this, _commSocket);
  writeWatcher = scheduler->installSocketEvent(loop, EVENT_SOCKET_WRITE, this, _commSocket);
  if (readWatcher == -1 || writeWatcher == -1) {
    return false;
  }
  // install timer for keep-alive timeout with some high default value
  keepAliveWatcher = scheduler->installTimerEvent(loop, this, 60.0);
  // and stop it immediately so it's not actively at the start
  scheduler->clearTimer(keepAliveWatcher);

  tid = Thread::currentThreadId();
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void SocketTask::cleanup () {

  if (scheduler == 0) {
    return;
  }

  scheduler->uninstallEvent(watcher);
  watcher = 0;

  scheduler->uninstallEvent(keepAliveWatcher);
  keepAliveWatcher = 0;

  scheduler->uninstallEvent(readWatcher);
  readWatcher = 0;

  scheduler->uninstallEvent(writeWatcher);
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
    LOGGER_TRACE("got keep-alive timeout signal, closing connection");

    // TODO: do we need some lock before we modify the scheduler?
    scheduler->clearTimer(token);

    // this will close the connection and destroy the task
    handleTimeout();
    return false;
  }

  if (token == readWatcher && (revents & EVENT_SOCKET_READ)) {
    if (keepAliveWatcher != 0) {
      // disable timer for keep-alive timeout
      scheduler->clearTimer(keepAliveWatcher);
    }

    result = handleRead(closed);
  }

  if (result && ! closed && token == writeWatcher) {
    bool noWrite = false;

    {
      MUTEX_LOCKER(writeBufferLock);
      noWrite = (_writeBuffer == 0);
    }

    if (revents & EVENT_SOCKET_WRITE) {
      result = handleWrite(closed, noWrite);
    }
  }

  if (result) {
    MUTEX_LOCKER(writeBufferLock);

    if (_writeBuffer == 0) {
      scheduler->stopSocketEvents(writeWatcher);
    }
    else {
      scheduler->startSocketEvents(writeWatcher);
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
