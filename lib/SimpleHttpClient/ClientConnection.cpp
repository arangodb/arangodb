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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ClientConnection.h"

#ifdef TRI_HAVE_WINSOCK2_H
#include <WinSock2.h>
#include <WS2tcpip.h>
#endif

#include <sys/types.h>

#ifdef _WIN32
#define STR_ERROR()                                                  \
  windowsErrorBuf;                                                   \
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0, \
                windowsErrorBuf, sizeof(windowsErrorBuf), NULL);     \
  errno = GetLastError();
#else
#define STR_ERROR() strerror(errno)
#endif

#ifdef TRI_HAVE_POLL_H
#include <poll.h>
#endif

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new client connection
////////////////////////////////////////////////////////////////////////////////

ClientConnection::ClientConnection(Endpoint* endpoint, double requestTimeout,
                                   double connectTimeout, size_t connectRetries)
    : GeneralClientConnection(endpoint, requestTimeout, connectTimeout,
                              connectRetries) {}

ClientConnection::ClientConnection(std::unique_ptr<Endpoint>& endpoint,
                                   double requestTimeout, double connectTimeout,
                                   size_t connectRetries)
    : GeneralClientConnection(endpoint, requestTimeout, connectTimeout,
                              connectRetries) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a client connection
////////////////////////////////////////////////////////////////////////////////

ClientConnection::~ClientConnection() { disconnect(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the socket is still alive
////////////////////////////////////////////////////////////////////////////////

bool ClientConnection::checkSocket() {
  int so_error = -1;
  socklen_t len = sizeof so_error;

  TRI_ASSERT(TRI_isvalidsocket(_socket));

  int res =
      TRI_getsockopt(_socket, SOL_SOCKET, SO_ERROR, (void*)&so_error, &len);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(errno);
    disconnect();
    return false;
  }

  if (so_error == 0) {
    return true;
  }

  TRI_set_errno(so_error);
  disconnect();

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief connect
////////////////////////////////////////////////////////////////////////////////

bool ClientConnection::connectSocket() {
  TRI_ASSERT(_endpoint != nullptr);

  if (_endpoint->isConnected()) {
    _endpoint->disconnect();
    _isConnected = false;
  }

  _socket = _endpoint->connect(_connectTimeout, _requestTimeout);

  if (!TRI_isvalidsocket(_socket)) {
    _errorDetails = _endpoint->_errorMessage;
    _isConnected = false;
    return false;
  }

  _isConnected = true;

  // note: checkSocket will disconnect the socket if the check fails
  if (checkSocket()) {
    return _endpoint->isConnected();
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief disconnect
////////////////////////////////////////////////////////////////////////////////

void ClientConnection::disconnectSocket() {
  if (_endpoint) {
    _endpoint->disconnect();
  }

  TRI_invalidatesocket(&_socket);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepare connection for read/write I/O
////////////////////////////////////////////////////////////////////////////////

bool ClientConnection::prepare(double timeout, bool isWrite) const {
  if (!TRI_isvalidsocket(_socket)) {
    _errorDetails = std::string("not a valid socket");
    return false;
  }

  // wait for at most 0.5 seconds for poll/select to complete
  // if it takes longer, break each poll/select into smaller chunks so we can
  // interrupt the whole process if it takes too long in total
  static double const POLL_DURATION = 0.5;
  auto const fd = TRI_get_fd_or_handle_of_socket(_socket);
  double start = TRI_microtime();
  int res;

#ifdef TRI_HAVE_POLL_H
  // Here we have poll, on all other platforms we use select
  bool nowait = (timeout == 0.0);
  int towait;
  if (timeout * 1000.0 > static_cast<double>(INT_MAX)) {
    towait = INT_MAX;
  } else {
    towait = static_cast<int>(timeout * 1000.0);
  }

  struct pollfd poller;
  memset(&poller, 0, sizeof(struct pollfd));  // for our old friend Valgrind
  poller.fd = fd;
  poller.events = (isWrite ? POLLOUT : POLLIN);

  while (true) {  // will be left by break
    res = poll(&poller, 1, towait > static_cast<int>(POLL_DURATION * 1000.0)
                               ? static_cast<int>(POLL_DURATION * 1000.0)
                               : towait);
    if (res == -1 && errno == EINTR) {
      if (!nowait) {
        double end = TRI_microtime();
        towait -= static_cast<int>((end - start) * 1000.0);
        start = end;
        if (towait <= 0) {  // Should not happen, but there might be rounding
                            // errors, so just to prevent a poll call with
                            // negative timeout...
          res = 0;
          break;
        }
      }
      continue;
    }

    if (res == 0) {
      if (isInterrupted()) {
        _errorDetails = std::string("command locally aborted");
        TRI_set_errno(TRI_ERROR_REQUEST_CANCELED);
        return false;
      }
      double end = TRI_microtime();
      towait -= static_cast<int>((end - start) * 1000.0);
      if (towait <= 0) {
        break;
      }
      start = end;
      continue;
    }

    break;
  }
// Now res can be:
//   1 : if the file descriptor was ready
//   0 : if the timeout happened
//   -1: if an error happened, EINTR within the timeout is already caught
#else
  // All versions use select:

  // An fd_set is a fixed size buffer.
  // Executing FD_CLR() or FD_SET() with a value of fd that is negative or is
  // equal to or larger than FD_SETSIZE
  // will result in undefined behavior. Moreover, POSIX requires fd to be a
  // valid file descriptor.
  if (fd < 0 || fd >= FD_SETSIZE) {
    // invalid or too high file descriptor value...
    // if we call FD_ZERO() or FD_SET() with it, the program behavior will be
    // undefined
    _errorDetails = std::string("file descriptor value too high");
    return false;
  }

  // handle interrupt
  do {
  retry:
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(fd, &fdset);

    fd_set* readFds = nullptr;
    fd_set* writeFds = nullptr;

    if (isWrite) {
      writeFds = &fdset;
    } else {
      readFds = &fdset;
    }

    int sockn = (int)(fd + 1);

    double waitTimeout = timeout;
    if (waitTimeout > POLL_DURATION) {
      waitTimeout = POLL_DURATION;
    }

    struct timeval t;
    t.tv_sec = (long)waitTimeout;
    t.tv_usec = (long)((waitTimeout - (double)t.tv_sec) * 1000000.0);

    res = select(sockn, readFds, writeFds, nullptr, &t);

    if ((res == -1 && errno == EINTR)) {
      int myerrno = errno;
      double end = TRI_microtime();
      errno = myerrno;
      timeout = timeout - (end - start);
      start = end;
    } else if (res == 0) {
      if (isInterrupted()) {
        _errorDetails = std::string("command locally aborted");
        TRI_set_errno(TRI_ERROR_REQUEST_CANCELED);
        return false;
      }
      double end = TRI_microtime();
      timeout = timeout - (end - start);
      if (timeout <= 0.0) {
        break;
      }
      start = end;
      goto retry;
    }
  } while (res == -1 && errno == EINTR && timeout > 0.0);
#endif

  if (res > 0) {
    return true;
  }

  if (res == 0) {
    if (isWrite) {
      _errorDetails = std::string("timeout during write");
      TRI_set_errno(TRI_SIMPLE_CLIENT_COULD_NOT_WRITE);
    } else {
      _errorDetails = std::string("timeout during read");
      TRI_set_errno(TRI_SIMPLE_CLIENT_COULD_NOT_READ);
    }
  } else {  // res < 0
#ifdef _WIN32
    char windowsErrorBuf[256];
#endif

    char const* pErr = STR_ERROR();
    _errorDetails = std::string("during prepare: ") + std::to_string(errno) +
                    std::string(" - ") + pErr;

    TRI_set_errno(errno);
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write data to the connection
////////////////////////////////////////////////////////////////////////////////

bool ClientConnection::writeClientConnection(void const* buffer, size_t length,
                                             size_t* bytesWritten) {
  TRI_ASSERT(bytesWritten != nullptr);

  if (!checkSocket()) {
    return false;
  }

#if defined(__APPLE__)
  // MSG_NOSIGNAL not supported on apple platform
  int status = TRI_send(_socket, buffer, length, 0);
#elif defined(_WIN32)
  // MSG_NOSIGNAL not supported on windows platform
  int status = TRI_send(_socket, buffer, length, 0);
#elif defined(__sun)
  // MSG_NOSIGNAL not supported on solaris platform
  int status = TRI_send(_socket, buffer, length, 0);
#else
  int status = TRI_send(_socket, buffer, length, MSG_NOSIGNAL);
#endif

  if (status < 0) {
    TRI_set_errno(errno);
    disconnect();
    return false;
  } else if (status == 0) {
    disconnect();
    return false;
  }

  *bytesWritten = (size_t)status;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read data from the connection
////////////////////////////////////////////////////////////////////////////////

bool ClientConnection::readClientConnection(StringBuffer& stringBuffer,
                                            bool& connectionClosed) {
  if (!checkSocket()) {
    connectionClosed = true;
    return false;
  }

  TRI_ASSERT(TRI_isvalidsocket(_socket));

  connectionClosed = false;

  do {
    // reserve some memory for reading
    if (stringBuffer.reserve(READBUFFER_SIZE) == TRI_ERROR_OUT_OF_MEMORY) {
      // out of memory
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return false;
    }

    int lenRead =
        TRI_READ_SOCKET(_socket, stringBuffer.end(), READBUFFER_SIZE - 1, 0);

    if (lenRead == -1) {
      // error occurred
      connectionClosed = true;
      return false;
    }

    if (lenRead == 0) {
      connectionClosed = true;
      disconnect();
      return true;
    }

    stringBuffer.increaseLength(lenRead);
  } while (readable());

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether the connection is readable
////////////////////////////////////////////////////////////////////////////////

bool ClientConnection::readable() {
  if (prepare(0.0, false)) {
    return checkSocket();
  }

  return false;
}
