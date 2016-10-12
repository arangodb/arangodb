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

#include "SslClientConnection.h"

#include <openssl/ssl.h>

#ifdef TRI_HAVE_WINSOCK2_H
#include <WinSock2.h>
#include <WS2tcpip.h>
#endif

#include <sys/types.h>

#include <openssl/ssl.h>

#include "Basics/socket-utils.h"
#include "Ssl/ssl-helper.h"

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
#include <sys/poll.h>
#endif

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new client connection
////////////////////////////////////////////////////////////////////////////////

SslClientConnection::SslClientConnection(Endpoint* endpoint,
                                         double requestTimeout,
                                         double connectTimeout,
                                         size_t connectRetries,
                                         uint64_t sslProtocol)
    : GeneralClientConnection(endpoint, requestTimeout, connectTimeout,
                              connectRetries),
      _ssl(nullptr),
      _ctx(nullptr) {

  init(sslProtocol);
}

SslClientConnection::SslClientConnection(std::unique_ptr<Endpoint>& endpoint,
                                         double requestTimeout,
                                         double connectTimeout,
                                         size_t connectRetries,
                                         uint64_t sslProtocol)
    : GeneralClientConnection(endpoint, requestTimeout, connectTimeout,
                              connectRetries),
      _ssl(nullptr),
      _ctx(nullptr) {

  init(sslProtocol);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a client connection
////////////////////////////////////////////////////////////////////////////////

SslClientConnection::~SslClientConnection() {
  disconnect();

  if (_ssl != nullptr) {
    SSL_free(_ssl);
  }

  if (_ctx != nullptr) {
    SSL_CTX_free(_ctx);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief internal initialization method, called from ctor
////////////////////////////////////////////////////////////////////////////////

void SslClientConnection::init(uint64_t sslProtocol) {
  TRI_invalidatesocket(&_socket);

  SSL_METHOD SSL_CONST* meth = nullptr;

  switch (protocol_e(sslProtocol)) {
#ifndef OPENSSL_NO_SSL2
    case SSL_V2:
      meth = SSLv2_method();
      break;
#endif

#ifndef OPENSSL_NO_SSL3_METHOD
    case SSL_V3:
      meth = SSLv3_method();
      break;
#endif

    case SSL_V23:
      meth = SSLv23_method();
      break;

    case TLS_V1:
      meth = TLSv1_method();
      break;

    default:
      // fallback is to use tlsv1
      meth = TLSv1_method();
  }

  _ctx = SSL_CTX_new(meth);

  if (_ctx != nullptr) {
    SSL_CTX_set_cipher_list(_ctx, "ALL");

    bool sslCache = true;
    SSL_CTX_set_session_cache_mode(
        _ctx, sslCache ? SSL_SESS_CACHE_SERVER : SSL_SESS_CACHE_OFF);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief connect
////////////////////////////////////////////////////////////////////////////////

bool SslClientConnection::connectSocket() {
#ifdef _WIN32
  char windowsErrorBuf[256];
#endif

  TRI_ASSERT(_endpoint != nullptr);

  if (_endpoint->isConnected()) {
    disconnectSocket();
    _isConnected = false;
  }

  _socket = _endpoint->connect(_connectTimeout, _requestTimeout);

  if (!TRI_isvalidsocket(_socket) || _ctx == nullptr) {
    _errorDetails = _endpoint->_errorMessage;
    _isConnected = false;
    return false;
  }

  _isConnected = true;

  _ssl = SSL_new(_ctx);

  if (_ssl == nullptr) {
    _errorDetails = std::string("failed to create ssl context");
    disconnectSocket();
    _isConnected = false;
    return false;
  }

  if (SSL_set_fd(_ssl, (int)TRI_get_fd_or_handle_of_socket(_socket)) != 1) {
    _errorDetails = std::string("SSL: failed to create context ") +
                    ERR_error_string(ERR_get_error(), nullptr);
    disconnectSocket();
    _isConnected = false;
    return false;
  }

  SSL_set_verify(_ssl, SSL_VERIFY_NONE, nullptr);

  ERR_clear_error();

  int ret = SSL_connect(_ssl);

  if (ret != 1) {
    int errorDetail;
    int certError;

    errorDetail = SSL_get_error(_ssl, ret);
    if ((errorDetail == SSL_ERROR_WANT_READ) ||
        (errorDetail == SSL_ERROR_WANT_WRITE)) {
      return true;
    } else if (errorDetail == SSL_ERROR_SYSCALL) {
      char const* pErr = STR_ERROR();
      _errorDetails = std::string("SSL: during SSL_connect: ") +
                      std::to_string(errno) + std::string(" - ") + pErr;
    } else {
      errorDetail = ERR_get_error(); /* Gets the earliest error code from the
                                        thread's error queue and removes the
                                        entry. */
      switch (errorDetail) {
        case 0x1407E086:
        /* 1407E086:
          SSL routines:
          SSL2_SET_CERTIFICATE:
          certificate verify failed */
        /* fall-through */
        case 0x14090086:
          /* 14090086:
            SSL routines:
            SSL3_GET_SERVER_CERTIFICATE:
            certificate verify failed */

          certError = SSL_get_verify_result(_ssl);

          if (certError != X509_V_OK) {
            _errorDetails = std::string("SSL: certificate problem: ") +
                            X509_verify_cert_error_string(certError);
          } else {
            _errorDetails = std::string(
                "SSL: certificate problem, verify that the CA cert is OK.");
          }
          break;

        default:
          char errorBuffer[256];
          ERR_error_string_n(errorDetail, errorBuffer, sizeof(errorBuffer));
          _errorDetails = std::string("SSL: ") + errorBuffer;
          break;
      }
    }
    disconnectSocket();
    _isConnected = false;
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief disconnect
////////////////////////////////////////////////////////////////////////////////

void SslClientConnection::disconnectSocket() {
  _endpoint->disconnect();
  TRI_invalidatesocket(&_socket);

  if (_ssl != nullptr) {
    SSL_free(_ssl);
    _ssl = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepare connection for read/write I/O
////////////////////////////////////////////////////////////////////////////////

bool SslClientConnection::prepare(double timeout, bool isWrite) const {
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
  poller.events = isWrite ? POLLOUT : POLLIN;

  while (true) {  // will be left by break
    res = poll(&poller, 1, towait);
    if (res == -1 && errno == EINTR) {
      if (!nowait) {
        double end = TRI_microtime();
        towait -= static_cast<int>((end - start) * 1000.0);
        start = end;
        if (towait < 0) {  // Should not happen, but there might be rounding
                           // errors, so just to prevent a poll call with
                           // negative timeout...
          res = 0;
          break;
        }
      }
      continue;
    }
    break;
  }
// Now res can be:
//   1 : if the file descriptor was ready
//   0 : if the timeout happened
//   -1: if an error happened, EINTR within the timeout is already caught
#else
  // All versions other use select:

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

  fd_set fdset;

  // handle interrupt
  do {
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

    struct timeval t;
    t.tv_sec = (long)timeout;
    t.tv_usec = (long)((timeout - (double)t.tv_sec) * 1000000.0);

    res = select(sockn, readFds, writeFds, nullptr, &t);

    if ((res == -1 && errno == EINTR)) {
      int myerrno = errno;
      double end = TRI_microtime();
      errno = myerrno;
      timeout = timeout - (end - start);
      start = end;
    }
  } while ((res == -1) && (errno == EINTR) && (timeout > 0.0));
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
  } else {  //    res < 0
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

bool SslClientConnection::writeClientConnection(void const* buffer,
                                                size_t length,
                                                size_t* bytesWritten) {
  TRI_ASSERT(bytesWritten != nullptr);

#ifdef _WIN32
  char windowsErrorBuf[256];
#endif
  *bytesWritten = 0;

  if (_ssl == nullptr) {
    return false;
  }

  int errorDetail;
  int written = SSL_write(_ssl, buffer, (int)length);
  int err = SSL_get_error(_ssl, written);
  switch (err) {
    case SSL_ERROR_NONE:
      *bytesWritten = written;

      return true;

    case SSL_ERROR_ZERO_RETURN:
      SSL_shutdown(_ssl);
      break;

    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
    case SSL_ERROR_WANT_CONNECT:
      break;

    case SSL_ERROR_SYSCALL: {
      char const* pErr = STR_ERROR();
      _errorDetails =
          std::string("SSL: while writing: SYSCALL returned errno = ") +
          std::to_string(errno) + std::string(" - ") + pErr;
      break;
    }

    case SSL_ERROR_SSL:
      /*  A failure in the SSL library occurred, usually a protocol error.
          The OpenSSL error queue contains more information on the error. */
      errorDetail = ERR_get_error();
      char errorBuffer[256];
      ERR_error_string_n(errorDetail, errorBuffer, sizeof(errorBuffer));
      _errorDetails = std::string("SSL: while writing: ") + errorBuffer;

      break;

    default:
      /* a true error */
      _errorDetails =
          std::string("SSL: while writing: error ") + std::to_string(err);
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read data from the connection
////////////////////////////////////////////////////////////////////////////////

bool SslClientConnection::readClientConnection(StringBuffer& stringBuffer,
                                               bool& connectionClosed) {
#ifdef _WIN32
  char windowsErrorBuf[256];
#endif

  connectionClosed = true;
  if (_ssl == nullptr) {
    return false;
  }
  if (!_isConnected) {
    return true;
  }

  connectionClosed = false;

  do {
  again:
    // reserve some memory for reading
    if (stringBuffer.reserve(READBUFFER_SIZE) == TRI_ERROR_OUT_OF_MEMORY) {
      // out of memory
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return false;
    }

    ERR_clear_error();

    int lenRead = SSL_read(_ssl, stringBuffer.end(), READBUFFER_SIZE - 1);

    switch (SSL_get_error(_ssl, lenRead)) {
      case SSL_ERROR_NONE:
        stringBuffer.increaseLength(lenRead);
        break;

      case SSL_ERROR_ZERO_RETURN:
        connectionClosed = true;
        SSL_shutdown(_ssl);
        _isConnected = false;
        return true;

      case SSL_ERROR_WANT_READ:
        goto again;

      case SSL_ERROR_WANT_WRITE:
      case SSL_ERROR_WANT_CONNECT:
      case SSL_ERROR_SYSCALL:
      default: {
        char const* pErr = STR_ERROR();
        int errorDetail = ERR_get_error();
        char errorBuffer[256];
        ERR_error_string_n(errorDetail, errorBuffer, sizeof(errorBuffer));
        _errorDetails = std::string("SSL: while reading: error '") +
                        std::to_string(errno) + std::string("' - ") +
                        errorBuffer + std::string("' - ") + pErr;

        /* unexpected */
        connectionClosed = true;
        return false;
      }
    }
  } while (readable());

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether the connection is readable
////////////////////////////////////////////////////////////////////////////////

bool SslClientConnection::readable() {
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

bool SslClientConnection::checkSocket() {
  int so_error = -1;
  socklen_t len = sizeof so_error;

  TRI_ASSERT(TRI_isvalidsocket(_socket));

  int res =
      TRI_getsockopt(_socket, SOL_SOCKET, SO_ERROR, (char*)(&so_error), &len);

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
