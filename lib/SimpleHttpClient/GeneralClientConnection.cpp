////////////////////////////////////////////////////////////////////////////////
/// @brief general client connection
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
/// @addtogroup httpclient
/// @{
////////////////////////////////////////////////////////////////////////////////

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
  disconnect();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup httpclient
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new connection from an endpoint
////////////////////////////////////////////////////////////////////////////////

GeneralClientConnection* GeneralClientConnection::factory (Endpoint* endpoint,
                                                           double requestTimeout,
                                                           double connectTimeout,
                                                           size_t numRetries) {
  if (endpoint->getEncryption() == Endpoint::ENCRYPTION_NONE) {
    return new ClientConnection(endpoint, requestTimeout, connectTimeout, numRetries);
  }
  else if (endpoint->getEncryption() == Endpoint::ENCRYPTION_SSL) {
    return new SslClientConnection(endpoint, requestTimeout, connectTimeout, numRetries);
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

  if (!_isConnected) {
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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send data to the endpoint
////////////////////////////////////////////////////////////////////////////////

bool GeneralClientConnection::handleWrite (const double timeout, void* buffer, size_t length, size_t* bytesWritten) {
  *bytesWritten = 0;

  if (prepare(timeout, true)) {
    return this->writeClientConnection(buffer, length, bytesWritten);
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read data from endpoint
////////////////////////////////////////////////////////////////////////////////

bool GeneralClientConnection::handleRead (double timeout, StringBuffer& buffer) {
  if (prepare(timeout, false)) {
    return this->readClientConnection(buffer);
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

