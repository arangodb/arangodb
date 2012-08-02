////////////////////////////////////////////////////////////////////////////////
/// @brief base class for input-output tasks from sockets
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2009-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "SocketTask.h"

#include <errno.h>

#include "Basics/MutexLocker.h"
#include "Basics/StringBuffer.h"
#include "Logger/Logger.h"
#include "Scheduler/Scheduler.h"

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

SocketTask::SocketTask (socket_t fd)
  : Task("SocketTask"),
    readWatcher(0),
    writeWatcher(0),
    watcher(0),
    commSocket(fd),
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
  if (commSocket != -1) {
    close(commSocket);
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

  int nr = read(commSocket, tmpReadBuffer, READ_BLOCK_SIZE);

  if (nr > 0) {
    _readBuffer->appendText(tmpReadBuffer, nr);

    return true;
  }
  else if (nr == 0) {
    closed = true;

    LOGGER_TRACE << "read return 0 with " << errno << " (" << strerror(errno) << ")";

    return false;
  }
  else {
    if (errno == EINTR) {
      return fillReadBuffer(closed);
    }
    else if (errno != EWOULDBLOCK) {
      LOGGER_TRACE << "read failed with " << errno << " (" << strerror(errno) << ")";

      return false;
    }
    else {
      LOGGER_TRACE << "read would block with " << errno << " (" << strerror(errno) << ")";

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

    size_t len = _writeBuffer->length() - writeLength;

    int nr = 0;

    if (0 < len) {
      nr = write(commSocket, _writeBuffer->begin() + writeLength, (int) len);

      if (nr < 0) {
        if (errno == EINTR) {
          return handleWrite(closed, noWrite);
        }
        else if (errno != EWOULDBLOCK) {
          LOGGER_DEBUG << "write failed with " << errno << " (" << strerror(errno) << ")";

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

void SocketTask::setup (Scheduler* scheduler, EventLoop loop) {
  this->scheduler = scheduler;
  this->loop = loop;

  watcher = scheduler->installAsyncEvent(loop, this);
  readWatcher = scheduler->installSocketEvent(loop, EVENT_SOCKET_READ, this, commSocket);
  writeWatcher = scheduler->installSocketEvent(loop, EVENT_SOCKET_WRITE, this, commSocket);

  tid = Thread::currentThreadId();
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void SocketTask::cleanup () {
  scheduler->uninstallEvent(watcher);
  watcher = 0;

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

  if (token == readWatcher && (revents & EVENT_SOCKET_READ)) {
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
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
