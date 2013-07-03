////////////////////////////////////////////////////////////////////////////////
/// @brief replication data fetcher
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
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ReplicationFetcher.h"

#include "Rest/HttpRequest.h"
#include "Rest/SslInterface.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

using namespace std;
//using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;
using namespace triagens::httpclient;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ReplicationFetcher::ReplicationFetcher (const string& masterEndpoint,
                                        double timeout) :
  _masterEndpoint(masterEndpoint),
  _timeout(timeout),
  _endpoint(Endpoint::clientFactory(masterEndpoint)),
  _connection(0),
  _client(0) {
 
  if (_endpoint != 0) { 
    _connection = GeneralClientConnection::factory(_endpoint, _timeout, _timeout, 3);

    if (_connection != 0) {
      _client = new SimpleHttpClient(_connection, _timeout, false);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ReplicationFetcher::~ReplicationFetcher () {
  // shutdown everything properly
  if (_client != 0) {
    delete _client;
  }

  if (_connection != 0) {
    delete _connection;
  }

  if (_endpoint != 0) {
    delete _endpoint;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief disconnect
////////////////////////////////////////////////////////////////////////////////
  
void ReplicationFetcher::disconnect () {
  if (_connection != 0) {
    _connection->disconnect();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief connect
////////////////////////////////////////////////////////////////////////////////
    
int ReplicationFetcher::connect () {
  if (_connection == 0) {
    return TRI_ERROR_INTERNAL;
  }

  if (! _connection->connect()) {
    // TODO: fix error code
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get master status
////////////////////////////////////////////////////////////////////////////////

int ReplicationFetcher::getMasterStatus () {
  if (_client == 0) {
    return TRI_ERROR_INTERNAL;
  }

  map<string, string> headers;

  // send GET request
  SimpleHttpResult* response = _client->request(HttpRequest::HTTP_REQUEST_GET, 
                                                "/_api/replication/state",
                                                0, 
                                                0,  
                                                headers); 

  if (response == 0) {
    // TODO: fix error code
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  string returnMessage;
  int    returnCode;

  if (! response->isComplete()) {
    returnMessage = _client->getErrorMessage();
    returnCode    = 500;
  }
  else {
    returnMessage = response->getHttpReturnMessage();
    returnCode    = response->getHttpReturnCode();
  }

  std::cout << "GOT STATUS CODE " << returnCode << ", MESSAGE: " << returnMessage << std::endl;
  std::cout << response->getBody().str() << std::endl;
  
  delete response;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run method
////////////////////////////////////////////////////////////////////////////////

int ReplicationFetcher::run () {
  // TODO: make this a loop
  return getMasterStatus();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
