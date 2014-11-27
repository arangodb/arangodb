////////////////////////////////////////////////////////////////////////////////
/// @brief general client connection
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

#include "GeneralClientConnection.h"
#include "SimpleHttpClient/ClientConnection.h"
#include "SimpleHttpClient/SslClientConnection.h"

using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::httpclient;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new client connection
////////////////////////////////////////////////////////////////////////////////

GeneralClientConnection::GeneralClientConnection (Endpoint* endpoint,
                                                  double requestTimeout,
                                                  double connectTimeout,
                                                  size_t connectRetries) :
  _endpoint(endpoint),
  _requestTimeout(requestTimeout),
  _connectTimeout(connectTimeout),
  _connectRetries(connectRetries),
  _numConnectRetries(0),
  _isConnected(false) {

}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a client connection
////////////////////////////////////////////////////////////////////////////////

GeneralClientConnection::~GeneralClientConnection () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new connection from an endpoint
////////////////////////////////////////////////////////////////////////////////

GeneralClientConnection* GeneralClientConnection::factory (Endpoint* endpoint,
                                                           double requestTimeout,
                                                           double connectTimeout,
                                                           size_t numRetries,
                                                           uint32_t sslProtocol) {
  if (endpoint->getEncryption() == Endpoint::ENCRYPTION_NONE) {
    return new ClientConnection(endpoint, requestTimeout, connectTimeout, numRetries);
  }
  else if (endpoint->getEncryption() == Endpoint::ENCRYPTION_SSL) {
    return new SslClientConnection(endpoint, requestTimeout, connectTimeout, numRetries, sslProtocol);
  }
  else {
    return 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief connect
////////////////////////////////////////////////////////////////////////////////

bool GeneralClientConnection::connect () {
  disconnect();

  if (_numConnectRetries < _connectRetries + 1) {
    _numConnectRetries++;
  }
  else {
    return false;
  }

  _isConnected = connectSocket();

  if (! _isConnected) {
    return false;
  }

  _numConnectRetries = 0;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief disconnect
////////////////////////////////////////////////////////////////////////////////

void GeneralClientConnection::disconnect () {
  if (isConnected()) {
    disconnectSocket();
  }

  _isConnected = false;
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
/// prepare() does a select and the call to readClientCollection does
/// what is described here.
////////////////////////////////////////////////////////////////////////////////

bool GeneralClientConnection::handleWrite (const double timeout, void* buffer, size_t length, size_t* bytesWritten) {
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
/// indicated by two flags, the return value and the progress flag,
/// which is always set by this method. The progress flag indicates
/// whether or not at least one byte has been appended to the buffer
/// (regardless of whether there was an error or not). The return value
/// indicates, whether an error has happened. Note that the other side
/// closing the connection is not considered to be an error! The call to
/// prepare() does a select and the call to readClientCollection does
/// what is described here.
////////////////////////////////////////////////////////////////////////////////

bool GeneralClientConnection::handleRead (double timeout, StringBuffer& buffer, bool& progress) {
  progress = false;

  if (prepare(timeout, false)) {
    return this->readClientConnection(buffer, progress);
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
