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

#include "BasicsC/json.h"
#include "Rest/HttpRequest.h"
#include "Rest/SslInterface.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "VocBase/server-id.h"

using namespace std;
using namespace triagens::basics;
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
  _timeout(timeout),
  _endpoint(Endpoint::clientFactory(masterEndpoint)),
  _connection(0),
  _client(0) {
 
  _master.endpoint = masterEndpoint;

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
/// @brief get master state
////////////////////////////////////////////////////////////////////////////////

int ReplicationFetcher::getMasterState () {
  if (_client == 0) {
    return TRI_ERROR_INTERNAL;
  }

  map<string, string> headers;

  // send request to get master inventory
  SimpleHttpResult* response = _client->request(HttpRequest::HTTP_REQUEST_GET, 
                                                "/_api/replication/state",
                                                0, 
                                                0,  
                                                headers); 

  if (response == 0) {
    // TODO: fix error code
    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }

  string returnMessage;
  int    res = TRI_ERROR_NO_ERROR;

  if (! response->isComplete()) {
    returnMessage = _client->getErrorMessage();
    res           = TRI_ERROR_REPLICATION_NO_RESPONSE;
  }
  else {
    returnMessage  = response->getHttpReturnMessage();

    if (response->wasHttpError()) {
      res = TRI_ERROR_REPLICATION_MASTER_ERROR;
    }
    else {
      TRI_json_t* json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, response->getBody().str().c_str());

      if (json != 0) {
        res = handleStateResponse(json);

        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      }
    }
  std::cout << response->getBody().str() << std::endl;
  }

  delete response;

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run method
////////////////////////////////////////////////////////////////////////////////

int ReplicationFetcher::run () {
  // TODO: make this a loop
  int res = getMasterState();

  std::cout << "RES: " << res << "\n";
  return res;
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
/// @brief handle the state response of the master
////////////////////////////////////////////////////////////////////////////////

int ReplicationFetcher::handleStateResponse (TRI_json_t const* json) {
  TRI_json_t const* server = TRI_LookupArrayJson(json, "server");

  if (server == 0 || server->_type != TRI_JSON_ARRAY) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_json_t const* version = TRI_LookupArrayJson(server, "version");
  
  if (version == 0 || 
      version->_type != TRI_JSON_STRING ||
      version->_value._string.data == 0) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
  
  TRI_json_t const* id = TRI_LookupArrayJson(server, "id");

  if (id == 0 || 
      id->_type != TRI_JSON_STRING ||
      id->_value._string.data == 0) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  const TRI_server_id_t masterId = StringUtils::uint64(id->_value._string.data);

  if (masterId == 0) {
    // invalid master id
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  if (masterId == TRI_GetServerId()) {
    // master and replica are the same instance. this is not supported.
    return TRI_ERROR_REPLICATION_LOOP;
  }

  int major = 0;
  int minor = 0;

  if (sscanf(version->_value._string.data, "%d.%d", &major, &minor) != 2) {
    return TRI_ERROR_REPLICATION_MASTER_INCOMPATIBLE;
  }

  if (major != 1 ||
      (major == 1 && minor != 4)) {
    return TRI_ERROR_REPLICATION_MASTER_INCOMPATIBLE;
  }

  _master.version.major = major;
  _master.version.minor = minor;
  _master.id            = masterId;

  _master.log("connected to");

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
