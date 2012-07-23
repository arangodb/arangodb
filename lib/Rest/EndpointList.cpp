////////////////////////////////////////////////////////////////////////////////
/// @brief connection endpoint list
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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "EndpointList.h"

#include <Logger/Logger.h>

using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                      EndpointList
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Rest
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create an endpoint list
////////////////////////////////////////////////////////////////////////////////
      
EndpointList::EndpointList () : 
   _httpEndpoints(),
   _binaryEndpoints() {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy an endpoint list
////////////////////////////////////////////////////////////////////////////////
      
EndpointList::~EndpointList () {
  // free memory
  for (ListType::iterator i = _httpEndpoints.begin(); i != _httpEndpoints.end(); ++i) {
    delete *i;
  }
  
  for (ListType::iterator i = _binaryEndpoints.begin(); i != _binaryEndpoints.end(); ++i) {
    delete *i;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Rest
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief dump all endpoints used
////////////////////////////////////////////////////////////////////////////////
    
void EndpointList::dump () {
  for (EndpointList::ListType::const_iterator i = _httpEndpoints.begin(); i != _httpEndpoints.end(); ++i) {
    LOGGER_INFO << "using endpoint '" << (*i)->getSpecification() << "' for HTTP requests";
  }
  
  for (EndpointList::ListType::const_iterator i = _binaryEndpoints.begin(); i != _binaryEndpoints.end(); ++i) {
    LOGGER_INFO << "using endpoint '" << (*i)->getSpecification() << "' for binary requests";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return all endpoints for HTTP requests
////////////////////////////////////////////////////////////////////////////////

EndpointList::ListType EndpointList::getHttpEndpoints () const {
  EndpointList::ListType result;

  for (EndpointList::ListType::const_iterator i = _httpEndpoints.begin(); i != _httpEndpoints.end(); ++i) {
    result.insert(*i);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return all endpoints for binary requests
////////////////////////////////////////////////////////////////////////////////

EndpointList::ListType EndpointList::getBinaryEndpoints () const {
  EndpointList::ListType result;

  for (EndpointList::ListType::const_iterator i = _binaryEndpoints.begin(); i != _binaryEndpoints.end(); ++i) {
    result.insert(*i);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an endpoint for HTTP requests
////////////////////////////////////////////////////////////////////////////////

bool EndpointList::addHttpEndpoint (const string& specification) {
  Endpoint* endpoint = Endpoint::serverFactory(specification);

  if (endpoint == 0) {
    return false;
  }

  _httpEndpoints.insert(endpoint);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an endpoint for binary requests
////////////////////////////////////////////////////////////////////////////////

bool EndpointList::addBinaryEndpoint (const string& specification) {
  Endpoint* endpoint = Endpoint::serverFactory(specification);

  if (endpoint == 0) {
    return false;
  }

  _binaryEndpoints.insert(endpoint);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
