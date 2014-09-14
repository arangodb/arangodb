////////////////////////////////////////////////////////////////////////////////
/// @brief ssl client connection
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

#include "SslClientConnection.h"

#include "Basics/ssl-helper.h"
#include "Basics/socket-utils.h"
#include "GeneralServer/GeneralSslServer.h"
#include "HttpServer/HttpsServer.h"

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

#include <openssl/ssl.h>



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

SslClientConnection::SslClientConnection (Endpoint* endpoint,
                                          double requestTimeout,
                                          double connectTimeout,
                                          size_t connectRetries,
                                          uint32_t sslProtocol) :
  GeneralClientConnection(endpoint, requestTimeout, connectTimeout, connectRetries),
  _ssl(0),
  _ctx(0) {

  TRI_invalidatesocket(&_socket);

  SSL_METHOD SSL_CONST* meth = 0;

  switch (HttpsServer::protocol_e(sslProtocol)) {
#ifndef OPENSSL_NO_SSL2
    case HttpsServer::SSL_V2:
      meth = SSLv2_method();
      break;
#endif
    case HttpsServer::SSL_V3:
      meth = SSLv3_method();
      break;

    case HttpsServer::SSL_V23:
      meth = SSLv23_method();
      break;

    case HttpsServer::TLS_V1:
      meth = TLSv1_method();
      break;

    default:
      // fallback is to use tlsv1
      meth = TLSv1_method();
  }

  _ctx = SSL_CTX_new(meth);

  if (_ctx) {
    SSL_CTX_set_cipher_list(_ctx, "ALL");

    const bool sslCache = true;
    SSL_CTX_set_session_cache_mode(_ctx, sslCache ? SSL_SESS_CACHE_SERVER : SSL_SESS_CACHE_OFF);
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

// -----------------------------------------------------------------------------
// --SECTION--                                         protected virtual methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief connect
////////////////////////////////////////////////////////////////////////////////

bool SslClientConnection::connectSocket () {
  TRI_ASSERT(_endpoint != nullptr);

  if (_endpoint->isConnected()) {
    _endpoint->disconnect();
  }

  _socket = _endpoint->connect(_connectTimeout, _requestTimeout);

  if (! TRI_isvalidsocket(_socket) || _ctx == nullptr) {
    return false;
  }

  _ssl = SSL_new(_ctx);
  if (_ssl == nullptr) {
    _endpoint->disconnect();
    TRI_invalidatesocket(&_socket);
    return false;
  }

  if (SSL_set_fd(_ssl, (int) TRI_get_fd_or_handle_of_socket(_socket)) != 1) {
    _endpoint->disconnect();
    SSL_free(_ssl);
    _ssl = nullptr;
    TRI_invalidatesocket(&_socket);
    return false;
  }

  SSL_set_verify(_ssl, SSL_VERIFY_NONE, NULL);

  int ret = SSL_connect(_ssl);
  if (ret != 1) {
    _endpoint->disconnect();
    SSL_free(_ssl);
    _ssl = 0;
    TRI_invalidatesocket(&_socket);
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief disconnect
////////////////////////////////////////////////////////////////////////////////

void SslClientConnection::disconnectSocket () {
  _endpoint->disconnect();
  TRI_invalidatesocket(&_socket);

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
  if (select(sockn, readFds, writeFds, NULL, &tv) > 0) {
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

  int written = SSL_write(_ssl, buffer, (int) length);
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
  if (_ssl == nullptr || ! _isConnected) {
    return false;
  }

  do {

again:
    // reserve some memory for reading
    if (stringBuffer.reserve(READBUFFER_SIZE) == TRI_ERROR_OUT_OF_MEMORY) {
      // out of memory
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return false;
    }

    int lenRead = SSL_read(_ssl, stringBuffer.end(), READBUFFER_SIZE - 1);

    switch (SSL_get_error(_ssl, lenRead)) {
      case SSL_ERROR_NONE:
        stringBuffer.increaseLength(lenRead);
        break;

      case SSL_ERROR_ZERO_RETURN:
        SSL_shutdown(_ssl);
        _isConnected = false;
        return true;

      case SSL_ERROR_WANT_READ:
        goto again;

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

  if (SSL_pending(_ssl) > 0) {
    return true;
  }

  if (prepare(0.0, false)) {
    return checkSocket();
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether the socket is workable
////////////////////////////////////////////////////////////////////////////////

bool SslClientConnection::checkSocket () {
  int so_error = -1;
  socklen_t len = sizeof so_error;

  TRI_ASSERT(TRI_isvalidsocket(_socket));

  int res = TRI_getsockopt(_socket, SOL_SOCKET, SO_ERROR, (char*)(&so_error), &len);

  if (res != TRI_ERROR_NO_ERROR) {
    _isConnected = false;
    TRI_set_errno(errno);
    return false;
  }

  if (so_error == 0) {
    return true;
  }

  TRI_set_errno(so_error);
  _isConnected = false;

  return false;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
