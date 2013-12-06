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
// --SECTION--                                                    AgencyEndpoint
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an agency endpoint
////////////////////////////////////////////////////////////////////////////////

AgencyEndpoint::AgencyEndpoint (triagens::rest::Endpoint* endpoint,
                                triagens::httpclient::GeneralClientConnection* connection)
  : _endpoint(endpoint),
    _connection(connection),
    _busy(false) {

}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an agency endpoint
////////////////////////////////////////////////////////////////////////////////
      
AgencyEndpoint::~AgencyEndpoint () {
  if (_connection != 0) {
    delete _connection;
  }
  if (_endpoint != 0) {
    delete _endpoint;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  AgencyCommResult
// -----------------------------------------------------------------------------

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
// --SECTION--                                                  static variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief the static global URL prefix
////////////////////////////////////////////////////////////////////////////////

const std::string AgencyComm::AGENCY_URL_PREFIX = "v2/keys";

////////////////////////////////////////////////////////////////////////////////
/// @brief global (variable) prefix used for endpoint
////////////////////////////////////////////////////////////////////////////////

std::string AgencyComm::_globalPrefix = "";

////////////////////////////////////////////////////////////////////////////////
/// @brief lock for the global endpoints
////////////////////////////////////////////////////////////////////////////////

triagens::basics::ReadWriteLock AgencyComm::_globalLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief list of global endpoints
////////////////////////////////////////////////////////////////////////////////

std::list<AgencyEndpoint*> AgencyComm::_globalEndpoints;

////////////////////////////////////////////////////////////////////////////////
/// @brief global connection options
////////////////////////////////////////////////////////////////////////////////

AgencyConnectionOptions AgencyComm::_globalConnectionOptions = {
  15.0,  // connectTimeout 
  3.0,   // requestTimeout
  3      // numRetries
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        AgencyComm
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs an agency communication object
////////////////////////////////////////////////////////////////////////////////

AgencyComm::AgencyComm () { 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an agency communication object
////////////////////////////////////////////////////////////////////////////////

AgencyComm::~AgencyComm () {
  disconnect();
  
  {
    WRITE_LOCKER(AgencyComm::_globalLock);

    std::list<AgencyEndpoint*>::iterator it = _globalEndpoints.begin();

    while (it != _globalEndpoints.end()) {
      AgencyEndpoint* agencyEndpoint = (*it);

      assert(agencyEndpoint != 0);
      delete agencyEndpoint;

      ++it;
    }

    // empty the list
    _globalEndpoints.clear();
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                             public static methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an endpoint to the endpoints list
////////////////////////////////////////////////////////////////////////////////

bool AgencyComm::addEndpoint (std::string const& endpointSpecification) {
  LOG_TRACE("adding global endpoint '%s'", endpointSpecification.c_str());

  {
    WRITE_LOCKER(AgencyComm::_globalLock);

    // check if we already have got this endpoint
    std::list<AgencyEndpoint*>::const_iterator it = _globalEndpoints.begin();

    while (it != _globalEndpoints.end()) {
      AgencyEndpoint const* agencyEndpoint = (*it);

      assert(agencyEndpoint != 0);
     
      if (agencyEndpoint->_endpoint->getSpecification() == endpointSpecification) {
        // a duplicate. just ignore
        return false;
      }

      ++it;
    }
  
    // not found a previous endpoint, now create one 
    AgencyEndpoint* agencyEndpoint = createAgencyEndpoint(endpointSpecification);

    if (agencyEndpoint == 0) {
      return false;
    }
    
    AgencyComm::_globalEndpoints.push_back(agencyEndpoint);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an endpoint from the endpoints list
////////////////////////////////////////////////////////////////////////////////

bool AgencyComm::removeEndpoint (std::string const& endpointSpecification) {
  LOG_TRACE("removing global endpoint '%s'", endpointSpecification.c_str());
 
  {
    WRITE_LOCKER(AgencyComm::_globalLock);
    
    // check if we have got this endpoint
    std::list<AgencyEndpoint*>::iterator it = _globalEndpoints.begin();

    while (it != _globalEndpoints.end()) {
      AgencyEndpoint const* agencyEndpoint = (*it);
     
      if (agencyEndpoint->_endpoint->getSpecification() == endpointSpecification) {
        // found, now remove
        AgencyComm::_globalEndpoints.erase(it);

        // and get rid of the endpoint
        delete agencyEndpoint;

        return true;
      }

      ++it;
    }
  }

  // not found
  return false;
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

////////////////////////////////////////////////////////////////////////////////
/// @brief get a stringified version of the endpoints
////////////////////////////////////////////////////////////////////////////////

const std::string AgencyComm::getEndpointsString () {
  std::string result;

  {
    // iterate over the list of endpoints
    READ_LOCKER(AgencyComm::_globalLock);
    
    std::list<AgencyEndpoint*>::const_iterator it = AgencyComm::_globalEndpoints.begin();

    while (it != AgencyComm::_globalEndpoints.end()) {
      if (! result.empty()) {
        result += ", ";
      }

      AgencyEndpoint const* agencyEndpoint = (*it);

      assert(agencyEndpoint != 0);

      result.append(agencyEndpoint->_endpoint->getSpecification());
      ++it;
    }
  }

  return result;
}

// -----------------------------------------------------------------------------
// --SECTION--                                            private static methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new agency endpoint
////////////////////////////////////////////////////////////////////////////////

AgencyEndpoint* AgencyComm::createAgencyEndpoint (std::string const& endpointSpecification) {
  triagens::rest::Endpoint* endpoint = triagens::rest::Endpoint::clientFactory(endpointSpecification);

  if (endpoint == 0) {
    // could not create endpoint...
    return 0;
  }

  triagens::httpclient::GeneralClientConnection* connection = 
    triagens::httpclient::GeneralClientConnection::factory(endpoint, 
                                                           _globalConnectionOptions._requestTimeout, 
                                                           _globalConnectionOptions._connectTimeout, 
                                                           _globalConnectionOptions._connectRetries);

  if (connection == 0) {
    delete endpoint;

    return 0;
  }
  
  return new AgencyEndpoint(endpoint, connection);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to establish a communication channel
////////////////////////////////////////////////////////////////////////////////

bool AgencyComm::tryConnect () {
  {
    WRITE_LOCKER(AgencyComm::_globalLock);
    assert(_globalEndpoints.size() > 0);

    std::list<AgencyEndpoint*>::iterator it = _globalEndpoints.begin();

    while (it != _globalEndpoints.end()) {
      AgencyEndpoint* agencyEndpoint = (*it);

      assert(agencyEndpoint != 0);
      assert(agencyEndpoint->_endpoint != 0);
      assert(agencyEndpoint->_connection != 0);

      if (agencyEndpoint->_endpoint->isConnected()) {
        return true;
      }
      
      agencyEndpoint->_endpoint->connect(_globalConnectionOptions._connectTimeout,
                                         _globalConnectionOptions._requestTimeout);
      
      if (agencyEndpoint->_endpoint->isConnected()) {
        return true;
      }

      ++it;
    }
  }

  // unable to connect to any endpoint
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief disconnects all communication channels
////////////////////////////////////////////////////////////////////////////////

void AgencyComm::disconnect () {
  WRITE_LOCKER(AgencyComm::_globalLock);

  std::list<AgencyEndpoint*>::iterator it = _globalEndpoints.begin();
  
  while (it != _globalEndpoints.end()) {
    AgencyEndpoint* agencyEndpoint = (*it);

    assert(agencyEndpoint != 0);
    assert(agencyEndpoint->_connection != 0);
    assert(agencyEndpoint->_endpoint != 0);

    agencyEndpoint->_connection->disconnect();
    agencyEndpoint->_endpoint->disconnect();

    ++it;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a value in the backend
////////////////////////////////////////////////////////////////////////////////
        
bool AgencyComm::setValue (std::string const& key, 
                           std::string const& value) {
  size_t numEndpoints;

  {
    READ_LOCKER(AgencyComm::_globalLock);
    numEndpoints = AgencyComm::_globalEndpoints.size(); 
    assert(numEndpoints > 0);
  }

  size_t tries = 0;

  while (tries++ < numEndpoints) {
    AgencyEndpoint* agencyEndpoint = popEndpoint();
  
    bool result = send(agencyEndpoint->_connection, 
                       triagens::rest::HttpRequest::HTTP_REQUEST_PUT, 
                       buildUrl(key), 
                       "value=" + value);

    if (requeueEndpoint(agencyEndpoint, result)) {
      // we're done
      return true;
    }

    // otherwise, try next
  }

  // if we get here, we could not send data to any endpoint successfully
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets one or multiple values from the backend
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult AgencyComm::getValues (std::string const& key, 
                                        bool recursive) {
  AgencyCommResult result;

  // TODO

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes one or multiple values from the backend
////////////////////////////////////////////////////////////////////////////////

bool AgencyComm::removeValues (std::string const& key, 
                               bool recursive) {
  std::string url(buildUrl(key));
  if (recursive) {
    url += "?recursive=true";
  }
    
  size_t numEndpoints;

  {
    READ_LOCKER(AgencyComm::_globalLock);
    numEndpoints = AgencyComm::_globalEndpoints.size(); 
    assert(numEndpoints > 0);
  }

  size_t tries = 0;

  while (tries++ < numEndpoints) {
    AgencyEndpoint* agencyEndpoint = popEndpoint();
    
    bool result = send(agencyEndpoint->_connection, 
                       triagens::rest::HttpRequest::HTTP_REQUEST_DELETE, 
                       url);

    if (requeueEndpoint(agencyEndpoint, result)) {
      // we're done
      return true;
    }

    // otherwise, try next
  }

  // if we get here, we could not send data to any endpoint successfully
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares and swaps a single value in the backend
////////////////////////////////////////////////////////////////////////////////

int AgencyComm::casValue (std::string const& key,
                          std::string const& oldValue, 
                          std::string const& newValue) {

  // TODO

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief blocks on a change of a single value in the backend
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult AgencyComm::watchValues (std::string const& key, 
                                          double timeout) {
  AgencyCommResult result;

  // TODO

  return result;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief pop an endpoint from the queue
////////////////////////////////////////////////////////////////////////////////
    
AgencyEndpoint* AgencyComm::popEndpoint () {
  while (1) {
    {
      WRITE_LOCKER(AgencyComm::_globalLock);
  
      std::list<AgencyEndpoint*>::iterator it = _globalEndpoints.begin();
    
      while (it != _globalEndpoints.end()) {
        AgencyEndpoint* agencyEndpoint = (*it);

        assert(agencyEndpoint != 0);

        if (! agencyEndpoint->_busy) {
          agencyEndpoint->_busy = true;

          if (AgencyComm::_globalEndpoints.size() > 1) {
            // remove from list
            AgencyComm::_globalEndpoints.remove(agencyEndpoint);
            // and re-insert at end
            AgencyComm::_globalEndpoints.push_back(agencyEndpoint);
          }
  
          return agencyEndpoint;
        }

        ++it;
      }
    }

    usleep(500); // TODO: make this configurable
  }

  // just to shut up compilers
  assert(false);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reinsert an endpoint into the queue
////////////////////////////////////////////////////////////////////////////////
    
bool AgencyComm::requeueEndpoint (AgencyEndpoint* agencyEndpoint,
                                  bool wasWorking) {
  WRITE_LOCKER(AgencyComm::_globalLock);
  
  assert(agencyEndpoint != 0);
  assert(agencyEndpoint->_busy);

  // set to non-busy
  agencyEndpoint->_busy = false;

  if (AgencyComm::_globalEndpoints.size() > 1) {
    if (wasWorking) {
      // remove from list
      AgencyComm::_globalEndpoints.remove(agencyEndpoint);

      // and re-insert at front
      AgencyComm::_globalEndpoints.push_front(agencyEndpoint);
    }
  }

  return wasWorking;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief construct a URL
////////////////////////////////////////////////////////////////////////////////

std::string AgencyComm::buildUrl (std::string const& relativePart) const {
  return AgencyComm::AGENCY_URL_PREFIX + _globalPrefix + relativePart;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sends data to the URL w/o body
////////////////////////////////////////////////////////////////////////////////
    
bool AgencyComm::send (triagens::httpclient::GeneralClientConnection* connection,
                       triagens::rest::HttpRequest::HttpRequestType method, 
                       std::string const& url) {
  assert(method == triagens::rest::HttpRequest::HTTP_REQUEST_DELETE ||
         method == triagens::rest::HttpRequest::HTTP_REQUEST_GET);

  return send(connection, method, url, "");
} 

////////////////////////////////////////////////////////////////////////////////
/// @brief sends data to the URL w/ body
////////////////////////////////////////////////////////////////////////////////
    
bool AgencyComm::send (triagens::httpclient::GeneralClientConnection* connection,
                       triagens::rest::HttpRequest::HttpRequestType method, 
                       std::string const& url, 
                       std::string const& body) {
 
  assert(connection != 0);
  
  LOG_TRACE("sending %s request to agency at endpoint '%s', url '%s': %s", 
            triagens::rest::HttpRequest::translateMethod(method).c_str(),
            connection->getEndpoint()->getSpecification().c_str(),
            url.c_str(), 
            body.c_str());
  
  triagens::httpclient::SimpleHttpClient client(connection, 
                                                _globalConnectionOptions._requestTimeout, 
                                                false);

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
    LOG_TRACE("sending request to agency failed");
    return false;
  }

  if (! response->isComplete()) {
    LOG_TRACE("sending request to agency failed");
    delete response;
    return false;
  }
      
  int statusCode = response->getHttpReturnCode();
  
  LOG_TRACE("request to agency returned status code %d, message: '%s', body: '%s'", 
            statusCode,
            response->getHttpReturnMessage().c_str(),
            response->getBody().str().c_str());

  delete response;

  return (statusCode >= 200 && statusCode <= 299);
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

