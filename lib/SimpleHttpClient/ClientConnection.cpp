////////////////////////////////////////////////////////////////////////////////
/// @brief simple client connection
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

#include "ClientConnection.h"

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

ClientConnection::ClientConnection (Endpoint* endpoint,
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

ClientConnection::~ClientConnection () {
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
/// @brief connect
////////////////////////////////////////////////////////////////////////////////      

bool ClientConnection::connect () {
  disconnect();

  if (_numConnectRetries < _connectRetries + 1) {
    _numConnectRetries++;
  }
  else {
    return false;
  }

  connectSocket();
  _isConnected = _endpoint->isConnected();

  if (!_isConnected) {
    return false;
  }

  return true;
}
    
////////////////////////////////////////////////////////////////////////////////
/// @brief disconnect
////////////////////////////////////////////////////////////////////////////////      

void ClientConnection::disconnect () {
  if (isConnected()) {
    _endpoint->disconnect();
    _isConnected = false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send data to the endpoint
////////////////////////////////////////////////////////////////////////////////      

bool ClientConnection::handleWrite (const double timeout, void* buffer, size_t length, size_t* bytesWritten) {
  *bytesWritten = 0;

  if (prepare(timeout, true)) {
    return write(buffer, length, bytesWritten);
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read data from endpoint
////////////////////////////////////////////////////////////////////////////////      
    
bool ClientConnection::handleRead (double timeout, StringBuffer& buffer) {
  if (prepare(timeout, false)) {
    return read(buffer);
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

