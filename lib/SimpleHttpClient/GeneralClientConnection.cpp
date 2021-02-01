////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include <errno.h>
#include <limits.h>
#include <string.h>

#include "Basics/Common.h"
#include "Basics/operating-system.h"

#ifdef TRI_HAVE_POLL_H
#include <poll.h>
#endif

#ifdef TRI_HAVE_WINSOCK2_H
#include <WS2tcpip.h>
#include <WinSock2.h>
#endif

#include "GeneralClientConnection.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "Basics/StringBuffer.h"
#include "Basics/debugging.h"
#include "Basics/error.h"
#include "Basics/socket-utils.h"
#include "Basics/system-functions.h"
#include "Basics/voc-errors.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "SimpleHttpClient/ClientConnection.h"
#include "SimpleHttpClient/SslClientConnection.h"

#ifdef _WIN32
#define STR_ERROR()                                                  \
  windowsErrorBuf;                                                   \
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0, \
                windowsErrorBuf, sizeof(windowsErrorBuf), NULL);     \
  errno = GetLastError();
#else
#define STR_ERROR() strerror(errno)
#endif

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new client connection
////////////////////////////////////////////////////////////////////////////////

GeneralClientConnection::GeneralClientConnection(application_features::ApplicationServer& server,
                                                 Endpoint* endpoint, double requestTimeout,
                                                 double connectTimeout, size_t connectRetries)
    : _server(server),
      _endpoint(endpoint),
      _freeEndpointOnDestruction(false),
      _requestTimeout(requestTimeout),
      _connectTimeout(connectTimeout),
      _connectRetries(connectRetries),
      _numConnectRetries(0),
      _isConnected(false),
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      _read(0),
      _written(0),
#endif
      _isInterrupted(false) {
  TRI_invalidatesocket(&_socket);
}

GeneralClientConnection::GeneralClientConnection(application_features::ApplicationServer& server,
                                                 std::unique_ptr<Endpoint>& endpoint,
                                                 double requestTimeout,
                                                 double connectTimeout, size_t connectRetries)
    : _server(server),
      _endpoint(endpoint.release()),
      _freeEndpointOnDestruction(true),
      _requestTimeout(requestTimeout),
      _connectTimeout(connectTimeout),
      _connectRetries(connectRetries),
      _numConnectRetries(0),
      _isConnected(false),
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      _read(0),
      _written(0),
#endif
      _isInterrupted(false) {
  TRI_invalidatesocket(&_socket);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a client connection
////////////////////////////////////////////////////////////////////////////////

GeneralClientConnection::~GeneralClientConnection() {
  if (_freeEndpointOnDestruction) {
    delete _endpoint;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new connection from an endpoint
////////////////////////////////////////////////////////////////////////////////

GeneralClientConnection* GeneralClientConnection::factory(
    application_features::ApplicationServer& server, Endpoint* endpoint, double requestTimeout,
    double connectTimeout, size_t numRetries, uint64_t sslProtocol) {
  if (endpoint->encryption() == Endpoint::EncryptionType::NONE) {
    return new ClientConnection(server, endpoint, requestTimeout, connectTimeout, numRetries);
  } else if (endpoint->encryption() == Endpoint::EncryptionType::SSL) {
    return new SslClientConnection(server, endpoint, requestTimeout,
                                   connectTimeout, numRetries, sslProtocol);
  }

  return nullptr;
}

GeneralClientConnection* GeneralClientConnection::factory(
    application_features::ApplicationServer& server,
    std::unique_ptr<Endpoint>& endpoint, double requestTimeout,
    double connectTimeout, size_t numRetries, uint64_t sslProtocol) {
  if (endpoint->encryption() == Endpoint::EncryptionType::NONE) {
    return new ClientConnection(server, endpoint, requestTimeout, connectTimeout, numRetries);
  } else if (endpoint->encryption() == Endpoint::EncryptionType::SSL) {
    return new SslClientConnection(server, endpoint, requestTimeout,
                                   connectTimeout, numRetries, sslProtocol);
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief connect
////////////////////////////////////////////////////////////////////////////////

bool GeneralClientConnection::connect() {
  disconnect();

  if (_numConnectRetries < _connectRetries + 1) {
    _numConnectRetries++;
  } else {
    return false;
  }

  connectSocket();

  if (!_isConnected) {
    return false;
  }

  _numConnectRetries = 0;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief disconnect
////////////////////////////////////////////////////////////////////////////////

void GeneralClientConnection::disconnect() {
  if (isConnected()) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    if ((_written + _read) == 0) {
      std::string bt;
      TRI_GetBacktrace(bt);
      LOG_TOPIC("b10b9", WARN, Logger::COMMUNICATION)
          << "Closing HTTP-connection right after opening it without sending "
             "data!"
          << bt;
    }
    _written = 0;
    _read = 0;
#endif
    disconnectSocket();
    _numConnectRetries = 0;
    _isConnected = false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepare connection for read/write I/O
////////////////////////////////////////////////////////////////////////////////

bool GeneralClientConnection::prepare(TRI_socket_t socket, double timeout, bool isWrite) {
  // wait for at most 0.5 seconds for poll/select to complete
  // if it takes longer, break each poll/select into smaller chunks so we can
  // interrupt the whole process if it takes too long in total
  static constexpr double POLL_DURATION = 0.5;
  auto const fd = TRI_get_fd_or_handle_of_socket(socket);
  double start = TRI_microtime();
  int res;

  auto& comm = server().getFeature<application_features::CommunicationFeaturePhase>();

#ifdef TRI_HAVE_POLL_H
  // Here we have poll, on all other platforms we use select
  double sinceLastSocketCheck = start;
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
    res = poll(&poller, 1,
               towait > static_cast<int>(POLL_DURATION * 1000.0)
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
      if (isInterrupted() || !comm.getCommAllowed()) {
        _errorDetails = std::string("command locally aborted");
        TRI_set_errno(TRI_ERROR_REQUEST_CANCELED);
        return false;
      }
      double end = TRI_microtime();
      towait -= static_cast<int>((end - start) * 1000.0);
      if (towait <= 0) {
        break;
      }

      // periodically recheck our socket
      if (end - sinceLastSocketCheck >= 20.0) {
        sinceLastSocketCheck = end;
        if (!checkSocket()) {
          // socket seems broken. now escape this loop
          break;
        }
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
  // All other versions use select:

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
      if (isInterrupted() || !comm.getCommAllowed()) {
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
    if (isInterrupted() || !comm.getCommAllowed()) {
      _errorDetails = std::string("command locally aborted");
      TRI_set_errno(TRI_ERROR_REQUEST_CANCELED);
      return false;
    }
    return true;
  }

  if (res == 0) {
    if (isWrite) {
      _errorDetails = std::string("timeout during write");
      TRI_set_errno(TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_WRITE);
    } else {
      _errorDetails = std::string("timeout during read");
      TRI_set_errno(TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_READ);
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
/// @brief check whether the socket is still alive
////////////////////////////////////////////////////////////////////////////////

bool GeneralClientConnection::checkSocket() {
  int so_error = -1;
  socklen_t len = sizeof so_error;

  TRI_ASSERT(TRI_isvalidsocket(_socket));

  int res = TRI_getsockopt(_socket, SOL_SOCKET, SO_ERROR, (void*)&so_error, &len);

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
/// @brief handleWrite
/// Write data to endpoint, this uses select to block until some
/// data can be written. Then it writes as much as it can without further
/// blocking, not calling select again. What has happened is
/// indicated by the return value and the bytesWritten variable,
/// which is always set by this method. The bytesWritten indicates
/// how many bytes have been written from the buffer
/// (regardless of whether there was an error or not). The return value
/// indicates, whether an error has happened. Note that the other side
/// closing the connection is not considered to be an error! The call to
/// prepare() does a select and the call to readClientConnection does
/// what is described here.
////////////////////////////////////////////////////////////////////////////////

bool GeneralClientConnection::handleWrite(double timeout, void const* buffer,
                                          size_t length, size_t* bytesWritten) {
  *bytesWritten = 0;

  if (prepare(_socket, timeout, true)) {
    return this->writeClientConnection(buffer, length, bytesWritten);
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handleRead
/// Read data from endpoint, this uses select to block until some
/// data has arrived. Then it reads as much as it can without further
/// blocking, using select multiple times. What has happened is
/// indicated by two flags, the return value and the connectionClosed flag,
/// which is always set by this method. The connectionClosed flag indicates
/// whether or not the connection has been closed by the other side
/// (regardless of whether there was an error or not). The return value
/// indicates, whether an error has happened. Note that the other side
/// closing the connection is not considered to be an error! The call to
/// prepare() does a select and the call to readClientCollection does
/// what is described here.
////////////////////////////////////////////////////////////////////////////////

bool GeneralClientConnection::handleRead(double timeout, StringBuffer& buffer,
                                         bool& connectionClosed) {
  connectionClosed = false;

  if (prepare(_socket, timeout, false)) {
    return this->readClientConnection(buffer, connectionClosed);
  }

  connectionClosed = true;
  return false;
}

application_features::ApplicationServer& GeneralClientConnection::server() const {
  return _server;
}