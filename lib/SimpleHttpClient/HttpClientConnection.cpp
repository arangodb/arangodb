////////////////////////////////////////////////////////////////////////////////
/// @brief simple http client connection
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2012, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "HttpClientConnection.h"

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

using namespace triagens::basics;
using namespace triagens::httpclient;
using namespace triagens::rest;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup httpclient
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new client connection
////////////////////////////////////////////////////////////////////////////////

HttpClientConnection::HttpClientConnection (Endpoint* endpoint,
                                            double requestTimeout,
                                            double connectTimeout,
                                            size_t connectRetries) :
  ClientConnection(endpoint, requestTimeout, connectTimeout, connectRetries) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a client connection
////////////////////////////////////////////////////////////////////////////////

HttpClientConnection::~HttpClientConnection () {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                         protected virtual methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup httpclient
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief connect
////////////////////////////////////////////////////////////////////////////////

bool HttpClientConnection::connectSocket () {
  _socket = _endpoint->connect();

  if (_socket == 0) {
    return false;
  }

  struct timeval tv;
  fd_set fdset;

  tv.tv_sec = (uint64_t) _connectTimeout;
  tv.tv_usec = ((uint64_t) (_connectTimeout * 1000000.0)) % 1000000;

  FD_ZERO(&fdset);
  FD_SET(_socket, &fdset);

  if (select(_socket + 1, NULL, &fdset, NULL, &tv) > 0) {
    if (checkSocket()) {
      return true;
    }

    return false;
  }

  // connect timeout reached
  disconnect();

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepare connection for read/write I/O
////////////////////////////////////////////////////////////////////////////////

bool HttpClientConnection::prepare (const double timeout, const bool isWrite) const {
  struct timeval tv;
  fd_set fdset;

  tv.tv_sec = (uint64_t) timeout;
  tv.tv_usec = ((uint64_t) (timeout * 1000000.0)) % 1000000;

  FD_ZERO(&fdset);
  FD_SET(_socket, &fdset);

  fd_set* readFds = NULL;
  fd_set* writeFds = NULL;

  if (isWrite) {
    writeFds = &fdset;
  }
  else {
    readFds = &fdset;
  }

  if (select(_socket + 1, readFds, writeFds, NULL, &tv) > 0) {
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write data to the connection
////////////////////////////////////////////////////////////////////////////////

bool HttpClientConnection::write (void* buffer, size_t length, size_t* bytesWritten) {
  if (!checkSocket()) {
    return false;
  }

#ifdef __APPLE__
  int status = ::send(_socket, buffer, length, 0);
#else
  int status = ::send(_socket, buffer, length, MSG_NOSIGNAL);
#endif

  *bytesWritten = status;

  return (status >= 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read data from the connection
////////////////////////////////////////////////////////////////////////////////
    
bool HttpClientConnection::read (StringBuffer& stringBuffer) {
  if (!checkSocket()) {
    return false;
  }

  do {
    char buffer[READBUFFER_SIZE];

    int lenRead = ::read(_socket, buffer, READBUFFER_SIZE - 1);

    if (lenRead <= 0) {
      // error: stop reading
      break;
    }

    stringBuffer.appendText(buffer, lenRead);
  }
  while (readable());

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether the connection is readable
////////////////////////////////////////////////////////////////////////////////
    
bool HttpClientConnection::readable () {
  fd_set fdset;
  FD_ZERO(&fdset);
  FD_SET(_socket, &fdset);

  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 0;

  if (select(_socket + 1, &fdset, NULL, NULL, &tv) == 1) {        
    return checkSocket();
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the socket is still alive
////////////////////////////////////////////////////////////////////////////////

bool HttpClientConnection::checkSocket () {
  int so_error = -1;
  socklen_t len = sizeof so_error;

  getsockopt(_socket, SOL_SOCKET, SO_ERROR, &so_error, &len);

  if (so_error == 0) {
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

