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

bool ClientConnection::prepare (const double timeout, const bool isWrite) const {
  struct timeval tv;
  fd_set fdset;

  if (! TRI_isvalidsocket(_socket)) {
    return false;
  }

  tv.tv_sec = (long) timeout;
  tv.tv_usec = (long) ((timeout - (double) tv.tv_sec) * 1000000.0);

  FD_ZERO(&fdset);
  FD_SET(TRI_get_fd_or_handle_of_socket(_socket), &fdset);

  fd_set* readFds = NULL;
  fd_set* writeFds = NULL;

  if (isWrite) {
    writeFds = &fdset;
  }
  else {
    readFds = &fdset;
  }

  int sockn = (int) (TRI_get_fd_or_handle_of_socket(_socket) + 1);
  int res = select(sockn, readFds, writeFds, NULL, &tv);

  if (res > 0) {
    return true;
  }

  if (res == 0) {
    if (isWrite) {
      TRI_set_errno(TRI_SIMPLE_CLIENT_COULD_NOT_WRITE);
    }
    else {
      TRI_set_errno(TRI_SIMPLE_CLIENT_COULD_NOT_READ);
    }
  }

  if (res < 0) {
    TRI_set_errno(errno);
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write data to the connection
////////////////////////////////////////////////////////////////////////////////

bool ClientConnection::writeClientConnection (void* buffer, size_t length, size_t* bytesWritten) {
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

bool ClientConnection::readClientConnection (StringBuffer& stringBuffer, bool& progress) {
  if (! checkSocket()) {
    return false;
  }

  TRI_ASSERT(TRI_isvalidsocket(_socket));

  progress = false;

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
      std::cout << "An error occurred!" << std::endl;
      return false;
    }

    if (lenRead == 0) {
      disconnect();
      return true;
    }

    progress = true;
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
