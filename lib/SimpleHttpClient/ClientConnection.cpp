////////////////////////////////////////////////////////////////////////////////
/// @brief client connection
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ClientConnection.h"

#ifdef TRI_HAVE_LINUX_SOCKETS
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/socket.h>
#endif


#ifdef TRI_HAVE_WINSOCK2_H
#include <WinSock2.h>
#include <WS2tcpip.h>
#endif

#include <sys/types.h>

#ifdef _WIN32
#define STR_ERROR()                             \
  windowsErrorBuf;                              \
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,     \
                NULL,                           \
                GetLastError(),                 \
                0,                              \
                windowsErrorBuf,                \
                sizeof(windowsErrorBuf), NULL); \
  errno = GetLastError();
#else
#define STR_ERROR() strerror(errno)
#endif

#ifdef __linux__
#include <sys/epoll.h>
#endif

using namespace triagens::basics;
using namespace triagens::httpclient;
using namespace triagens::rest;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new client connection
////////////////////////////////////////////////////////////////////////////////

ClientConnection::ClientConnection (Endpoint* endpoint,
                                    double requestTimeout,
                                    double connectTimeout,
                                    size_t connectRetries) :
  GeneralClientConnection(endpoint, requestTimeout, connectTimeout, connectRetries) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a client connection
////////////////////////////////////////////////////////////////////////////////

ClientConnection::~ClientConnection () {
  disconnect();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the socket is still alive
////////////////////////////////////////////////////////////////////////////////

bool ClientConnection::checkSocket () {
  int so_error = -1;
  socklen_t len = sizeof so_error;

  TRI_ASSERT(TRI_isvalidsocket(_socket));

  int res = TRI_getsockopt(_socket, SOL_SOCKET, SO_ERROR, (void*) &so_error, &len);

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

// -----------------------------------------------------------------------------
// --SECTION--                                         protected virtual methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief connect
////////////////////////////////////////////////////////////////////////////////

bool ClientConnection::connectSocket () {
  TRI_ASSERT(_endpoint != nullptr);

  if (_endpoint->isConnected()) {
    _endpoint->disconnect();
  }

  _socket = _endpoint->connect(_connectTimeout, _requestTimeout);

  if (! TRI_isvalidsocket(_socket)) {
    _errorDetails = _endpoint->_errorMessage; 
    return false;
  }

  if (checkSocket()) {
    return _endpoint->isConnected();
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief disconnect
////////////////////////////////////////////////////////////////////////////////

void ClientConnection::disconnectSocket () {
  if (_endpoint) {
    _endpoint->disconnect();
  }

  TRI_invalidatesocket(&_socket);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepare connection for read/write I/O
////////////////////////////////////////////////////////////////////////////////

bool ClientConnection::prepare (double timeout, bool isWrite) const {
  if (! TRI_isvalidsocket(_socket)) {
    _errorDetails = std::string("not a valid socket");
    return false;
  }

  auto const fd = TRI_get_fd_or_handle_of_socket(_socket);
  double start = TRI_microtime();
  int res;
    
#ifdef __linux__
  // Here we have epoll, on all other platforms we use select
  int towait;
  if (timeout * 1000.0 > static_cast<double>(INT_MAX)) {
    towait = INT_MAX;
  }
  else {
    towait = static_cast<int>(timeout * 1000.0);
  }

  int epollfd = epoll_create(1);
  if (epollfd < 0) {
    res = -1;
  }
  else {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(struct epoll_event)); // for our old friend Valgrind

    struct epoll_event happened[1];
    int myerrno = 0;  // Eiertanz to preserve the error number

    ev.events = isWrite ? EPOLLOUT : EPOLLIN;
    ev.data.fd = fd;

    res = epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);

    if (res == 0) {
      do {
        res = epoll_wait(epollfd, happened, 1, towait);

        if (res < 0) {
          myerrno = errno;
        }
        double end = TRI_microtime();
        towait -= static_cast<int>((end - start) * 1000.0);
        start = end;
      } while (res == -1 && myerrno == EINTR && towait > 0);
    }
    else {
      myerrno = errno;
    }
    close(epollfd);
    errno = myerrno;
  }
#else
  // All versions other than Linux use select:
  struct timeval tv;
  tv.tv_sec = (long) timeout;
  tv.tv_usec = (long) ((timeout - (double) tv.tv_sec) * 1000000.0);
  
  fd_set fdset;
  do {
    // An fd_set is a fixed size buffer. 
    // Executing FD_CLR() or FD_SET() with a value of fd that is negative or is equal to or larger than FD_SETSIZE 
    // will result in undefined behavior. Moreover, POSIX requires fd to be a valid file descriptor.
    if (fd < 0 || fd >= FD_SETSIZE) {
      // invalid or too high file descriptor value...
      // if we call FD_ZERO() or FD_SET() with it, the program behavior will be undefined
      _errorDetails = std::string("file descriptor value too high");
      return false;
    }

    FD_ZERO(&fdset);
    FD_SET(fd, &fdset);

    fd_set* readFds = nullptr;
    fd_set* writeFds = nullptr;

    if (isWrite) {
      writeFds = &fdset;
    }
    else {
      readFds = &fdset;
    }

    int sockn = (int) (fd + 1);
    res = select(sockn, readFds, writeFds, nullptr, &tv);
    if ((res == -1 && errno == EINTR)) {
      int myerrno = errno;
      double end = TRI_microtime();
      errno = myerrno;
      timeout = timeout - (end - start);
      start = end;
      tv.tv_sec = (long) timeout;
      tv.tv_usec = (long) ((timeout - (double) tv.tv_sec) * 1000000.0);
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
    }
    else {
      _errorDetails = std::string("timeout during read");
      TRI_set_errno(TRI_SIMPLE_CLIENT_COULD_NOT_READ);
    }
  }
  else {    // res < 0
#ifdef _WIN32
    char windowsErrorBuf[256];
#endif

    char const* pErr = STR_ERROR();
    _errorDetails = std::string("during prepare: ") + std::to_string(errno) + std::string(" - ") + pErr;
    
    TRI_set_errno(errno);
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write data to the connection
////////////////////////////////////////////////////////////////////////////////

bool ClientConnection::writeClientConnection (void const* buffer, size_t length, size_t* bytesWritten) {
  if (! checkSocket()) {
    return false;
  }

#if defined(__APPLE__)
  // MSG_NOSIGNAL not supported on apple platform
  int status = TRI_send(_socket, buffer, length, 0);
#elif defined(_WIN32)
  // MSG_NOSIGNAL not supported on windows platform
  int status = TRI_send(_socket, buffer, length, 0);
#else
  int status = TRI_send(_socket, buffer, length, MSG_NOSIGNAL);
#endif

  if (status < 0) {
    TRI_set_errno(errno);
    disconnect();
    return false;
  }
  else if (status == 0) {
    disconnect();
    return false;
  }

  *bytesWritten = (size_t) status;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read data from the connection
////////////////////////////////////////////////////////////////////////////////

bool ClientConnection::readClientConnection (StringBuffer& stringBuffer, 
                                             bool& connectionClosed) {
  if (! checkSocket()) {
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

    int lenRead = TRI_READ_SOCKET(_socket, stringBuffer.end(), READBUFFER_SIZE - 1, 0);

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
  }
  while (readable());

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether the connection is readable
////////////////////////////////////////////////////////////////////////////////

bool ClientConnection::readable () {
  if (prepare(0.0, false)) {
    return checkSocket();
  }

  return false;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
