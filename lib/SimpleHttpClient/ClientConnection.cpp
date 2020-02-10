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

#include <errno.h>
#include <string>

#include "Basics/Common.h"
#include "Basics/operating-system.h"

#ifdef TRI_HAVE_WINSOCK2_H
#include <WS2tcpip.h>
#include <WinSock2.h>
#endif

#include "ClientConnection.h"

#include "Basics/StringBuffer.h"
#include "Basics/debugging.h"
#include "Basics/error.h"
#include "Basics/socket-utils.h"
#include "Basics/voc-errors.h"
#include "Endpoint/Endpoint.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new client connection
////////////////////////////////////////////////////////////////////////////////

ClientConnection::ClientConnection(application_features::ApplicationServer& server,
                                   Endpoint* endpoint, double requestTimeout,
                                   double connectTimeout, size_t connectRetries)
    : GeneralClientConnection(server, endpoint, requestTimeout, connectTimeout,
                              connectRetries) {}

ClientConnection::ClientConnection(application_features::ApplicationServer& server,
                                   std::unique_ptr<Endpoint>& endpoint, double requestTimeout,
                                   double connectTimeout, size_t connectRetries)
    : GeneralClientConnection(server, endpoint, requestTimeout, connectTimeout,
                              connectRetries) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a client connection
////////////////////////////////////////////////////////////////////////////////

ClientConnection::~ClientConnection() { disconnect(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief connect
////////////////////////////////////////////////////////////////////////////////

bool ClientConnection::connectSocket() {
  TRI_ASSERT(_endpoint != nullptr);

  if (_endpoint->isConnected()) {
    _endpoint->disconnect();
    _isConnected = false;
  }

  _errorDetails.clear();
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
  long status = TRI_send(_socket, buffer, length, 0);
#elif defined(_WIN32)
  // MSG_NOSIGNAL not supported on windows platform
  long status = TRI_send(_socket, buffer, length, 0);
#else
  long status = TRI_send(_socket, buffer, length, MSG_NOSIGNAL);
#endif

  if (status < 0) {
    TRI_set_errno(errno);
    disconnect();
    return false;
  } else if (status == 0) {
    disconnect();
    return false;
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  _written += (uint64_t)status;
#endif
  *bytesWritten = (size_t)status;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read data from the connection
////////////////////////////////////////////////////////////////////////////////

bool ClientConnection::readClientConnection(StringBuffer& stringBuffer, bool& connectionClosed) {
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

    TRI_read_return_t  lenRead = TRI_READ_SOCKET(_socket, stringBuffer.end(), READBUFFER_SIZE - 1, 0);

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
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    _read += static_cast<uint64_t>(lenRead);
#endif
    stringBuffer.increaseLength(lenRead);
  } while (readable());

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether the connection is readable
////////////////////////////////////////////////////////////////////////////////

bool ClientConnection::readable() {
  if (prepare(_socket, 0.0, false)) {
    return checkSocket();
  }

  return false;
}
