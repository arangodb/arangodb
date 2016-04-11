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

#include "GeneralClientConnection.h"
#include "SimpleHttpClient/ClientConnection.h"
#include "SimpleHttpClient/SslClientConnection.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new client connection
////////////////////////////////////////////////////////////////////////////////

GeneralClientConnection::GeneralClientConnection(Endpoint* endpoint,
                                                 double requestTimeout,
                                                 double connectTimeout,
                                                 size_t connectRetries)
    : _endpoint(endpoint),
      _freeEndpointOnDestruction(false),
      _requestTimeout(requestTimeout),
      _connectTimeout(connectTimeout),
      _connectRetries(connectRetries),
      _numConnectRetries(0),
      _isConnected(false),
      _isInterrupted(false) {}

GeneralClientConnection::GeneralClientConnection(
    std::unique_ptr<Endpoint>& endpoint, double requestTimeout,
    double connectTimeout, size_t connectRetries)
    : _endpoint(endpoint.release()),
      _freeEndpointOnDestruction(true),
      _requestTimeout(requestTimeout),
      _connectTimeout(connectTimeout),
      _connectRetries(connectRetries),
      _numConnectRetries(0),
      _isConnected(false),
      _isInterrupted(false) {}

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
    Endpoint* endpoint, double requestTimeout, double connectTimeout,
    size_t numRetries, uint64_t sslProtocol) {
  if (endpoint->encryption() == Endpoint::EncryptionType::NONE) {
    return new ClientConnection(endpoint, requestTimeout, connectTimeout,
                                numRetries);
  } else if (endpoint->encryption() == Endpoint::EncryptionType::SSL) {
    return new SslClientConnection(endpoint, requestTimeout, connectTimeout,
                                   numRetries, sslProtocol);
  }

  return nullptr;
}

GeneralClientConnection* GeneralClientConnection::factory(
    std::unique_ptr<Endpoint>& endpoint, double requestTimeout, double connectTimeout,
    size_t numRetries, uint64_t sslProtocol) {
  if (endpoint->encryption() == Endpoint::EncryptionType::NONE) {
    return new ClientConnection(endpoint, requestTimeout, connectTimeout,
                                numRetries);
  } else if (endpoint->encryption() == Endpoint::EncryptionType::SSL) {
    return new SslClientConnection(endpoint, requestTimeout, connectTimeout,
                                   numRetries, sslProtocol);
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief connect
////////////////////////////////////////////////////////////////////////////////

bool GeneralClientConnection::connect() {
  _isInterrupted = false;
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
    disconnectSocket();
  }

  _isConnected = false;
  _isInterrupted = false;
  _numConnectRetries = 0;
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

  if (prepare(timeout, true)) {
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

  if (prepare(timeout, false)) {
    return this->readClientConnection(buffer, connectionClosed);
  }

  connectionClosed = true;
  return false;
}
