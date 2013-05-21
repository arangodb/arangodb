////////////////////////////////////////////////////////////////////////////////
/// @brief ssl client connection
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
/// @author Jan Steemann
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "SslClientConnection.h"

#include "Basics/ssl-helper.h"
#include "BasicsC/socket-utils.h"

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
/// @addtogroup httpclient
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new client connection
////////////////////////////////////////////////////////////////////////////////

SslClientConnection::SslClientConnection (Endpoint* endpoint,
                                          double requestTimeout,
                                          double connectTimeout,
                                          size_t connectRetries) :
  GeneralClientConnection(endpoint, requestTimeout, connectTimeout, connectRetries),
  _ssl(0),
  _ctx(0) {
  _socket.fileHandle = 0;
  _socket.fileDescriptor = 0;
  _ctx = SSL_CTX_new(TLSv1_method());
  if (_ctx) {
    SSL_CTX_set_cipher_list(_ctx, "ALL");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a client connection
////////////////////////////////////////////////////////////////////////////////

SslClientConnection::~SslClientConnection () {
  disconnect();

  if (_ssl) {
    SSL_free(_ssl);
  }

  if (_ctx) {
    SSL_CTX_free(_ctx);
  }
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

bool SslClientConnection::connectSocket () {
  _socket = _endpoint->connect(_connectTimeout, _requestTimeout);

  if (_socket.fileHandle <= 0 || _ctx == 0) {
    return false;
  }

  _ssl = SSL_new(_ctx);
  if (_ssl == 0) {
    _endpoint->disconnect();
    _socket.fileHandle = 0;
    _socket.fileDescriptor = 0;
    return false;
  }

  if (SSL_set_fd(_ssl, (int) _socket.fileHandle) != 1) {
    _endpoint->disconnect();
    SSL_free(_ssl);
    _ssl = 0;
    _socket.fileHandle = 0;
    _socket.fileDescriptor = 0;
    return false;
  }

  SSL_set_verify(_ssl, SSL_VERIFY_NONE, NULL);

  int ret = SSL_connect(_ssl);
  if (ret != 1) {
    _endpoint->disconnect();
    SSL_free(_ssl);
    _ssl = 0;
    _socket.fileHandle = 0;
    _socket.fileDescriptor = 0;
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief disconnect
////////////////////////////////////////////////////////////////////////////////

void SslClientConnection::disconnectSocket () {
  _endpoint->disconnect();
  _socket.fileHandle = 0;
  _socket.fileDescriptor = 0;

  if (_ssl) {
    SSL_free(_ssl);
    _ssl = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepare connection for read/write I/O
////////////////////////////////////////////////////////////////////////////////

bool SslClientConnection::prepare (const double timeout, const bool isWrite) const {
  struct timeval tv;
  fd_set fdset;

  tv.tv_sec = (uint64_t) timeout;
  tv.tv_usec = ((uint64_t) (timeout * 1000000.0)) % 1000000;

  FD_ZERO(&fdset);
  FD_SET(_socket.fileHandle, &fdset);

  fd_set* readFds = NULL;
  fd_set* writeFds = NULL;

  if (isWrite) {
    writeFds = &fdset;
  }
  else {
    readFds = &fdset;
  }

  if (select(_socket.fileHandle + 1, readFds, writeFds, NULL, &tv) > 0) {
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write data to the connection
////////////////////////////////////////////////////////////////////////////////

bool SslClientConnection::writeClientConnection (void* buffer, size_t length, size_t* bytesWritten) {
  *bytesWritten = 0;

  if (_ssl == 0) {
    return false;
  }

  int written = SSL_write(_ssl, buffer, length);
  switch (SSL_get_error(_ssl, written)) {
    case SSL_ERROR_NONE:
      *bytesWritten = written;

      return true;

    case SSL_ERROR_ZERO_RETURN:
      SSL_shutdown(_ssl);
      break;

    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
    case SSL_ERROR_WANT_CONNECT:
    case SSL_ERROR_SYSCALL:
    default: {
      /* fall through */
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read data from the connection
////////////////////////////////////////////////////////////////////////////////

bool SslClientConnection::readClientConnection (StringBuffer& stringBuffer) {
  if (_ssl == 0) {
    return false;
  }

  do {
    // reserve some memory for reading
    if (stringBuffer.reserve(READBUFFER_SIZE) == TRI_ERROR_OUT_OF_MEMORY) {
      // out of memory
      return false;
    }

    int lenRead = SSL_read(_ssl, stringBuffer.end(), READBUFFER_SIZE - 1);

    switch (SSL_get_error(_ssl, lenRead)) {
      case SSL_ERROR_NONE:
        stringBuffer.increaseLength(lenRead);
        break;

      case SSL_ERROR_ZERO_RETURN:
        SSL_shutdown(_ssl);
        return true;

      case SSL_ERROR_WANT_READ:
      case SSL_ERROR_WANT_WRITE:
      case SSL_ERROR_WANT_CONNECT:
      case SSL_ERROR_SYSCALL:
      default:
        /* unexpected */
        return false;
    }
  }
  while (readable());

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether the connection is readable
////////////////////////////////////////////////////////////////////////////////

bool SslClientConnection::readable () {
  // must use SSL_pending() and not select() as SSL_read() might read more
  // bytes from the socket into the _ssl structure than we actually requested
  // via SSL_read().
  // if we used select() to check whether there is more data available, select()
  // might return 0 already but we might not have consumed all bytes yet

  // ...........................................................................
  // SSL_pending(...) is an OpenSSL function which returns the number of bytes
  // which are available inside ssl for reading.
  // ...........................................................................

  return (SSL_pending(_ssl) > 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

