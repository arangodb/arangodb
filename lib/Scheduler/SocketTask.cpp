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

#include "Basics/logging.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringBuffer.h"
#include "Basics/socket-utils.h"
#include "Scheduler/Scheduler.h"

#include <errno.h>

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
    _keepAliveWatcher(nullptr),
    _readWatcher(nullptr),
    _writeWatcher(nullptr),
    _asyncWatcher(nullptr),
    _commSocket(socket),
    _keepAliveTimeout(keepAliveTimeout),
    _writeBuffer(nullptr),
#ifdef TRI_ENABLE_FIGURES
    _writeBufferStatistics(nullptr),
#endif
    _ownBuffer(true),
    _writeLength(0),
    _readBuffer(nullptr),
    _clientClosed(false),
    _tid(0) {

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

  if (_ownBuffer) {
    delete _writeBuffer;
  }

#ifdef TRI_ENABLE_FIGURES

  if (_writeBufferStatistics != nullptr) {
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
  if (_keepAliveWatcher != nullptr && timeout > 0.0) {
    _scheduler->rearmTimer(_keepAliveWatcher, timeout);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                         protected virtual methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the read buffer
////////////////////////////////////////////////////////////////////////////////

bool SocketTask::fillReadBuffer () {
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
    _clientClosed = true;

    LOG_TRACE("read returned 0");

    return false;
  }

  if (errno == EINTR) {
    return fillReadBuffer();
  }

  if (errno != EWOULDBLOCK) {
    LOG_TRACE("read failed with %d: %s", (int) errno, strerror(errno));

    return false;
  }
    
  LOG_TRACE("read would block with %d: %s", (int) errno, strerror(errno));

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a write
////////////////////////////////////////////////////////////////////////////////

bool SocketTask::handleWrite () {
  bool callCompletedWriteBuffer = false;

  // size_t is unsigned, should never get < 0
  size_t len = 0;

  if (nullptr != _writeBuffer) {
    TRI_ASSERT(_writeBuffer->length() >= _writeLength);
    len = _writeBuffer->length() - _writeLength;
  }

  int nr = 0;

  if (0 < len) {
    nr = TRI_WRITE_SOCKET(_commSocket, _writeBuffer->begin() + _writeLength, (int) len, 0);

    if (nr < 0) {
      if (errno == EINTR) {
        return handleWrite();
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
    if (nullptr != _writeBuffer && _ownBuffer) {
      delete _writeBuffer;
    }

    callCompletedWriteBuffer = true;
  }
  else {
    _writeLength += nr;
  }

  if (callCompletedWriteBuffer) {
    completedWriteBuffer();

    // rearm timer for keep-alive timeout
    // TODO: do we need some lock before we modify the scheduler?
    setKeepAliveTimeout(_keepAliveTimeout);
  }

  if (_clientClosed) {
    return false;
  }

  // we might have a new write buffer or none at all
  if (_writeBuffer == nullptr) {
    _scheduler->stopSocketEvents(_writeWatcher);
  }
  else {
    _scheduler->startSocketEvents(_writeWatcher);
  }

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief sets an active write buffer
////////////////////////////////////////////////////////////////////////////////

void SocketTask::setWriteBuffer (StringBuffer* buffer,
                                 TRI_request_statistics_t* statistics,
                                 bool ownBuffer) {
  bool callCompletedWriteBuffer = false;

#ifdef TRI_ENABLE_FIGURES

  _writeBufferStatistics = statistics;

  if (_writeBufferStatistics != nullptr) {
    _writeBufferStatistics->_writeStart = TRI_StatisticsTime();
    _writeBufferStatistics->_sentBytes += buffer->length();
  }

#endif

  _writeLength = 0;

  if (buffer->empty()) {
    if (ownBuffer) {
      delete buffer;
    }

    callCompletedWriteBuffer = true;
  }
  else {
    if (_writeBuffer != nullptr) {
      if (_ownBuffer) {
        delete _writeBuffer;
      }
    }

    _writeBuffer = buffer;
    _ownBuffer = ownBuffer;
  }

  if (callCompletedWriteBuffer) {
    completedWriteBuffer();

  }

  if (_clientClosed) {
    return;
  }

  // we might have a new write buffer or none at all
  TRI_ASSERT(_tid == Thread::currentThreadId());

  if (_writeBuffer == nullptr) {
    _scheduler->stopSocketEvents(_writeWatcher);
  }
  else {
    _scheduler->startSocketEvents(_writeWatcher);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks for presence of an active write buffer
////////////////////////////////////////////////////////////////////////////////

bool SocketTask::hasWriteBuffer () const {
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
  int res = (int) _commSocket.fileHandle;

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


  _scheduler = scheduler;
  _loop = loop;

  _asyncWatcher = _scheduler->installAsyncEvent(loop, this);
  _readWatcher = _scheduler->installSocketEvent(loop, EVENT_SOCKET_READ, this, _commSocket);
  _writeWatcher = _scheduler->installSocketEvent(loop, EVENT_SOCKET_WRITE, this, _commSocket);

  if (_readWatcher == nullptr || _writeWatcher == nullptr) {
    return false;
  }

  // install timer for keep-alive timeout with some high default value
  _keepAliveWatcher = _scheduler->installTimerEvent(loop, this, 60.0);

  // and stop it immediately so it's not actively at the start
  _scheduler->clearTimer(_keepAliveWatcher);

  _tid = Thread::currentThreadId();
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void SocketTask::cleanup () {
  if (_scheduler == nullptr) {
    LOG_WARNING("In SocketTask::cleanup the scheduler has disappeared -- invalid pointer");
    _asyncWatcher = nullptr;
    _keepAliveWatcher = nullptr;
    _readWatcher = nullptr;
    _writeWatcher = nullptr;
    return;
  }

  _scheduler->uninstallEvent(_asyncWatcher);
  _asyncWatcher = nullptr;

  _scheduler->uninstallEvent(_keepAliveWatcher);
  _keepAliveWatcher = nullptr;

  _scheduler->uninstallEvent(_readWatcher);
  _readWatcher = nullptr;

  _scheduler->uninstallEvent(_writeWatcher);
  _writeWatcher = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool SocketTask::handleEvent (EventToken token, EventType revents) {
  bool result = true;

  if (token == _keepAliveWatcher && (revents & EVENT_TIMER)) {
    // got a keep-alive timeout
    LOG_TRACE("got keep-alive timeout signal, closing connection");

    // TODO: do we need some lock before we modify the scheduler?
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

  if (result && ! _clientClosed && token == _writeWatcher) {
    if (revents & EVENT_SOCKET_WRITE) {
      result = handleWrite();
    }
  }

  if (result) {
    if (_writeBuffer == nullptr) {
      _scheduler->stopSocketEvents(_writeWatcher);
    }
    else {
      _scheduler->startSocketEvents(_writeWatcher);
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
