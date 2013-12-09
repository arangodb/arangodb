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
#include "Basics/StringUtils.h"
#include "Basics/WriteLocker.h"
#include "BasicsC/json.h"
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

AgencyCommResult::AgencyCommResult () 
  : _message(),
    _body(),
    _statusCode(0) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a communication result
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult::~AgencyCommResult () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief recursively flatten the JSON response into a map
////////////////////////////////////////////////////////////////////////////////

bool AgencyCommResult::processJsonNode (TRI_json_t const* node,
                                        std::map<std::string, std::string>& out,
                                        std::string const& stripKeyPrefix,
                                        bool returnIndex) const {
  if (! TRI_IsArrayJson(node)) {
    return true;
  }

  // get "key" attribute
  TRI_json_t const* key = TRI_LookupArrayJson(node, "key");

  if (! TRI_IsStringJson(key)) {
    return false;
  }

  // make sure we don't strip more bytes than the key is long
  const size_t offset = AgencyComm::_globalPrefix.size() + stripKeyPrefix.size();
  const size_t length = key->_value._string.length - 1;

  std::string prefix;
  if (offset >= length) {
    prefix = "";
  }
  else {
    prefix = std::string(key->_value._string.data + offset, 
                         key->_value._string.length - 1 - offset);
  }

  // get "dir" attribute
  TRI_json_t const* dir = TRI_LookupArrayJson(node, "dir");
  bool isDir = (TRI_IsBooleanJson(dir) && dir->_value._boolean);

  if (isDir) {
    // is a directory, so there may be a "nodes" attribute
    TRI_json_t const* nodes = TRI_LookupArrayJson(node, "nodes");

    if (! TRI_IsListJson(nodes)) {
      // if directory is empty...
      return true;
    }

    const size_t n = TRI_LengthVector(&nodes->_value._objects);

    for (size_t i = 0; i < n; ++i) {
      if (! processJsonNode((TRI_json_t const*) TRI_AtVector(&nodes->_value._objects, i), 
                            out, 
                            stripKeyPrefix, 
                            returnIndex)) {
        return false;
      }
    }
  }
  else {
    // not a directory
    
    // get "value" attribute
    TRI_json_t const* value = TRI_LookupArrayJson(node, "value");

    if (! TRI_IsStringJson(value)) {
      return false;
    }

    if (! prefix.empty()) {
      if (returnIndex) {
        // return "modifiedIndex"
        TRI_json_t const* modifiedIndex = TRI_LookupArrayJson(node, "modifiedIndex");
        
        if (! TRI_IsNumberJson(modifiedIndex)) {
          return false;
        }
      
        // convert the number to an integer  
        out[prefix] = triagens::basics::StringUtils::itoa((uint64_t) modifiedIndex->_value._number);
      }
      else {
        // otherwise return value
        out[prefix] = std::string(value->_value._string.data, value->_value._string.length - 1);
      }
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief turn a result into a map
////////////////////////////////////////////////////////////////////////////////

bool AgencyCommResult::flattenJson (std::map<std::string, std::string>& out,
                                    std::string const& stripKeyPrefix,
                                    bool returnIndex) const {
  TRI_json_t* json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, _body.c_str());

  if (! TRI_IsArrayJson(json)) {
    if (json != 0) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
    return false;
  }

  // get "node" attribute
  TRI_json_t const* node = TRI_LookupArrayJson(json, "node");

  const bool result = processJsonNode(node, out, stripKeyPrefix, returnIndex);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  return result;
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
}

// -----------------------------------------------------------------------------
// --SECTION--                                             public static methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief cleans up all connections
////////////////////////////////////////////////////////////////////////////////

void AgencyComm::cleanup () {
  AgencyComm::disconnect();
  
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

  LOG_TRACE("setting agency-prefix to '%s'", prefix.c_str());
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
/// @brief sets a value in the backend
////////////////////////////////////////////////////////////////////////////////
        
bool AgencyComm::setValue (std::string const& key, 
                           std::string const& value) {
  AgencyCommResult result;
  size_t numEndpoints;

  {
    READ_LOCKER(AgencyComm::_globalLock);
    numEndpoints = AgencyComm::_globalEndpoints.size(); 
    assert(numEndpoints > 0);
  }

  size_t tries = 0;

  while (tries++ < numEndpoints) {
    AgencyEndpoint* agencyEndpoint = popEndpoint();
  
    send(agencyEndpoint->_connection, 
         triagens::rest::HttpRequest::HTTP_REQUEST_PUT, 
         _globalConnectionOptions._requestTimeout, 
         result,
         buildUrl(key), 
         "value=" + value);

    if (requeueEndpoint(agencyEndpoint, result.successful())) {
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
  std::string url(buildUrl(key));
  if (recursive) {
    url += "?recursive=true";
  }

  AgencyCommResult result;
  size_t numEndpoints;

  {
    READ_LOCKER(AgencyComm::_globalLock);
    numEndpoints = AgencyComm::_globalEndpoints.size(); 
    assert(numEndpoints > 0);
  }

  size_t tries = 0;

  while (tries++ < numEndpoints) {
    AgencyEndpoint* agencyEndpoint = popEndpoint();
    
    send(agencyEndpoint->_connection, 
         triagens::rest::HttpRequest::HTTP_REQUEST_GET,
         _globalConnectionOptions._requestTimeout * 1000.0 * 1000.0, 
         result,
         url);

    if (requeueEndpoint(agencyEndpoint, result.successful())) {
      // we're done
      break;
    }

    // otherwise, try next
  }

  // if we get here, we could not send data to any endpoint successfully
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
    
  AgencyCommResult result;
  size_t numEndpoints;

  {
    READ_LOCKER(AgencyComm::_globalLock);
    numEndpoints = AgencyComm::_globalEndpoints.size(); 
    assert(numEndpoints > 0);
  }

  size_t tries = 0;

  while (tries++ < numEndpoints) {
    AgencyEndpoint* agencyEndpoint = popEndpoint();
    
    send(agencyEndpoint->_connection, 
         triagens::rest::HttpRequest::HTTP_REQUEST_DELETE, 
         _globalConnectionOptions._requestTimeout, 
         result,
         url);

    if (requeueEndpoint(agencyEndpoint, result.successful())) {
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

AgencyCommResult AgencyComm::watchValue (std::string const& key, 
                                         uint64_t waitIndex,
                                         double timeout) {
  std::string url(buildUrl(key));
  url += "?wait=true";

  if (waitIndex > 0) {
    url += "&waitIndex=" + triagens::basics::StringUtils::itoa(waitIndex);
  }

  AgencyCommResult result;
  size_t numEndpoints;

  {
    READ_LOCKER(AgencyComm::_globalLock);
    numEndpoints = AgencyComm::_globalEndpoints.size(); 
    assert(numEndpoints > 0);
  }

  size_t tries = 0;

  while (tries++ < numEndpoints) {
    AgencyEndpoint* agencyEndpoint = popEndpoint();

    send(agencyEndpoint->_connection, 
         triagens::rest::HttpRequest::HTTP_REQUEST_GET,
         timeout == 0.0 ? _globalConnectionOptions._requestTimeout : timeout, 
         result,
         url);

    if (requeueEndpoint(agencyEndpoint, result.successful())) {
      // we're done
      break;
    }

    // otherwise, try next
  }

  // if we get here, we could not send data to any endpoint successfully
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
 
      const size_t numEndpoints = _globalEndpoints.size(); 
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

          assert(_globalEndpoints.size() == numEndpoints);
  
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
  const size_t numEndpoints = _globalEndpoints.size(); 
  
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
          
  assert(_globalEndpoints.size() == numEndpoints);

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
                       double timeout,
                       AgencyCommResult& result,
                       std::string const& url) {
  // only these methods can be called without a body
  assert(method == triagens::rest::HttpRequest::HTTP_REQUEST_DELETE ||
         method == triagens::rest::HttpRequest::HTTP_REQUEST_GET ||
         method == triagens::rest::HttpRequest::HTTP_REQUEST_HEAD);

  return send(connection, method, timeout, result, url, "");
} 

////////////////////////////////////////////////////////////////////////////////
/// @brief sends data to the URL w/ body
////////////////////////////////////////////////////////////////////////////////
    
bool AgencyComm::send (triagens::httpclient::GeneralClientConnection* connection,
                       triagens::rest::HttpRequest::HttpRequestType method, 
                       double timeout,
                       AgencyCommResult& result,
                       std::string const& url, 
                       std::string const& body) {
 
  assert(connection != 0);

  result._statusCode = 0;
  
  LOG_TRACE("sending %s request to agency at endpoint '%s', url '%s': %s", 
            triagens::rest::HttpRequest::translateMethod(method).c_str(),
            connection->getEndpoint()->getSpecification().c_str(),
            url.c_str(), 
            body.c_str());
  
  triagens::httpclient::SimpleHttpClient client(connection,
                                                timeout, 
                                                false);

  // set up headers
  std::map<std::string, std::string> headers;
  if (method == triagens::rest::HttpRequest::HTTP_REQUEST_PUT ||
      method == triagens::rest::HttpRequest::HTTP_REQUEST_POST) {
    // the agency needs this content-type for the body
    headers["content-type"] = "application/x-www-form-urlencoded";
  }
  
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
      
  result._statusCode = response->getHttpReturnCode();
  result._message    = response->getHttpReturnMessage();
  result._body       = response->getBody().str();
  
  LOG_TRACE("request to agency returned status code %d, message: '%s', body: '%s'", 
            result._statusCode,
            result._message.c_str(),
            result._body.c_str());

  delete response;

  return result.successful();
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

