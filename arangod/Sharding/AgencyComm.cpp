////////////////////////////////////////////////////////////////////////////////
/// @brief communication with agency node(s)
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2013, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Sharding/AgencyComm.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "BasicsC/logging.h"
#include "Rest/Endpoint.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                  AgencyCommResult
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief global prefix used for endpoint
////////////////////////////////////////////////////////////////////////////////

std::string AgencyComm::_globalPrefix = "";

////////////////////////////////////////////////////////////////////////////////
/// @brief lock for the global endpoints
////////////////////////////////////////////////////////////////////////////////

triagens::basics::ReadWriteLock AgencyComm::_globalLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief list of global endpoints
////////////////////////////////////////////////////////////////////////////////

std::set<std::string> AgencyComm::_globalEndpoints;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a communication result
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult::AgencyCommResult () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a communication result
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult::~AgencyCommResult () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        AgencyComm
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs an agency communication object
////////////////////////////////////////////////////////////////////////////////

AgencyComm::AgencyComm () 
  : _endpoints(),
    _connectTimeout(15.0),
    _requestTimeout(3.0) {

}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an agency communication object
////////////////////////////////////////////////////////////////////////////////

AgencyComm::~AgencyComm () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                             public static methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an endpoint to the agents list
////////////////////////////////////////////////////////////////////////////////

bool AgencyComm::addEndpoint (std::string const& endpoint) {
  LOG_TRACE("adding global endpoint '%s'", endpoint.c_str());

  {
    WRITE_LOCKER(AgencyComm::_globalLock);
    AgencyComm::_globalEndpoints.insert(endpoint);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an agent from the agents list
////////////////////////////////////////////////////////////////////////////////

bool AgencyComm::removeEndpoint (std::string const& endpoint) {
  LOG_TRACE("removing global endpoint '%s'", endpoint.c_str());

  WRITE_LOCKER(AgencyComm::_globalLock);
  return AgencyComm::_globalEndpoints.erase(endpoint) == 1;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the global prefix for all operations
////////////////////////////////////////////////////////////////////////////////

void AgencyComm::setPrefix (std::string const& prefix) {
  _globalPrefix = prefix;

  // make sure prefix starts with a forward slash
  if (prefix[0] != '/') {
    _globalPrefix = '/' + _globalPrefix;
  }

  // make sure prefix ends with a forward slash
  if (_globalPrefix.size() > 0) {
    if (_globalPrefix[_globalPrefix.size() - 1] != '/') {
      _globalPrefix += '/';
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a timestamp
////////////////////////////////////////////////////////////////////////////////

std::string AgencyComm::generateStamp () {
  time_t tt = time(0);
  struct tm tb;
  char buffer[21];

  // TODO: optimise this
  TRI_gmtime(tt, &tb);

  size_t len = ::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tb);

  return std::string(buffer, len);
}

// -----------------------------------------------------------------------------
// --SECTION--                                            private static methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a local copy of endpoints, to be used without the global lock
////////////////////////////////////////////////////////////////////////////////

const std::set<std::string> AgencyComm::getEndpoints () {
  std::set<std::string> endpoints;

  {
    // copy the global list of endpoints, using the lock
    READ_LOCKER(AgencyComm::_globalLock);
    endpoints = AgencyComm::_globalEndpoints;
  }

  assert(endpoints.size() > 0);
  return endpoints;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief establishes the communication channels
////////////////////////////////////////////////////////////////////////////////

int AgencyComm::connect () {
  assert(_endpoints.size() == 0);

  bool hasConnected = false;

  const std::set<std::string> endpoints = getEndpoints();
  std::set<std::string>::const_iterator it = endpoints.begin();

  while (it != endpoints.end()) {
    triagens::rest::Endpoint* endpoint = triagens::rest::Endpoint::clientFactory((*it));

    if (endpoint == 0) {
      // could not create endpoint... disconnect all
      disconnect();
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    if (! hasConnected) {
      // connect to just one endpoint
      endpoint->connect(_connectTimeout, _requestTimeout);

      if (endpoint->isConnected()) {
        hasConnected = true;
      }
    }

    // insert all endpoints (even the unconnected ones)
    _endpoints.insert(endpoint);

    ++it;
  }

  if (! hasConnected) {
    return TRI_ERROR_CLUSTER_NO_AGENCY;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief disconnects all communication channels
////////////////////////////////////////////////////////////////////////////////

int AgencyComm::disconnect () {
  std::set<triagens::rest::Endpoint*>::iterator it = _endpoints.begin();

  while (it != _endpoints.end()) {
    (*it)->disconnect();

    ++it;
  }

  // empty the set
  _endpoints.clear();

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a value in the back end
////////////////////////////////////////////////////////////////////////////////
        
bool AgencyComm::setValue (std::string const& key, 
                           std::string const& value) {

  std::set<triagens::rest::Endpoint*>::iterator it = _endpoints.begin();
 
  while (it != _endpoints.end()) {
    triagens::rest::Endpoint* endpoint = (*it);

    // check if we are connected
    if (! endpoint->isConnected()) {
      endpoint->connect(10, 10);
    }

    if (endpoint->isConnected()) {
      if (send(endpoint, triagens::rest::HttpRequest::HTTP_REQUEST_PUT, buildUrl(key), "value=" + value)) {
        return true;
      }
    }

    ++it;
  }

  // if we got here, we could not send data to any endpoint successfully

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets one or multiple values from the back end
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult AgencyComm::getValues (std::string const& key, 
                                        bool recursive) {
  AgencyCommResult result;

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes one or multiple values from the back end
////////////////////////////////////////////////////////////////////////////////

int AgencyComm::removeValues (std::string const& key, 
                              bool recursive) {
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares and swaps a single value in the back end
////////////////////////////////////////////////////////////////////////////////

int AgencyComm::casValue (std::string const& key,
                          std::string const& oldValue, 
                          std::string const& newValue) {
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief blocks on a change of a single value in the back end
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult AgencyComm::watchValues (std::string const& key, 
                                          double timeout) {
  AgencyCommResult result;

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief construct a URL
////////////////////////////////////////////////////////////////////////////////

std::string AgencyComm::buildUrl (std::string const& relativePart) const {
  return "/v2/keys" + _globalPrefix + relativePart;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sends data to the URL
////////////////////////////////////////////////////////////////////////////////
    
bool AgencyComm::send (triagens::rest::Endpoint* endpoint,
                       triagens::rest::HttpRequest::HttpRequestType method, 
                       std::string const& url, 
                       std::string const& body) {

  LOG_TRACE("sending request to url '%s': %s", url.c_str(), body.c_str());

  triagens::httpclient::GeneralClientConnection* connection = 
    triagens::httpclient::GeneralClientConnection::factory(endpoint, _requestTimeout, _connectTimeout, 3);

  triagens::httpclient::SimpleHttpClient client(connection, _requestTimeout, false);

  // set up headers
  std::map<std::string, std::string> headers;
  // the agency needs this
  headers["content-type"] = "application/x-www-form-urlencoded";
  
  // send the actual request
  triagens::httpclient::SimpleHttpResult* response = client.request(method,
                                                                    url,
                                                                    body.c_str(),
                                                                    body.size(),
                                                                    headers);

  if (response == 0) {
    return false;
  }

  if (! response->isComplete()) {
    delete response;
    return false;
  }
      
  int statusCode = response->getHttpReturnCode();
  
  /* 
  LOG_TRACE("return message: %s", response->getHttpReturnMessage().c_str());
  LOG_TRACE("return body: %s", response->getBody().str().c_str());
  LOG_TRACE("statuscode was %d", statusCode);
  */

  delete response;

  return (statusCode >= 200 && statusCode <= 299);
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

