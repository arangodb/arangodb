////////////////////////////////////////////////////////////////////////////////
/// @brief connection endpoint list
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

#include "EndpointList.h"

#include "Logger/Logger.h"

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
   _lists() {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy an endpoint list
////////////////////////////////////////////////////////////////////////////////

EndpointList::~EndpointList () {
  for (map<string, ListType>::iterator i = _lists.begin(); i != _lists.end(); ++i) {
    for (ListType::iterator i2 = (*i).second.begin(); i2 != (*i).second.end(); ++i2) {
      delete *i2;
    }

    (*i).second.clear();
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
/// @brief count the number of elements in a sub-list
////////////////////////////////////////////////////////////////////////////////

size_t EndpointList::count (const Endpoint::ProtocolType protocol,
                            const Endpoint::EncryptionType encryption) const {

   map<string, ListType>::const_iterator i = _lists.find(getKey(protocol, encryption));

   if (i == _lists.end()) {
     return 0;
   }

   return i->second.size();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump all endpoints used
////////////////////////////////////////////////////////////////////////////////

void EndpointList::dump () const {
  for (map<string, ListType>::const_iterator i = _lists.begin(); i != _lists.end(); ++i) {
    for (ListType::const_iterator i2 = (*i).second.begin(); i2 != (*i).second.end(); ++i2) {
      LOGGER_INFO("using endpoint '" << (*i2)->getSpecification() << "' for " << (*i).first << " requests");
    }
  }
}
////////////////////////////////////////////////////////////////////////////////
/// @brief return all endpoints for a specific protocol
////////////////////////////////////////////////////////////////////////////////

EndpointList::ListType EndpointList::getEndpoints (const Endpoint::ProtocolType protocol,
                                                   const Endpoint::EncryptionType encryption) const {

  EndpointList::ListType result;
  map<string, EndpointList::ListType>::const_iterator i = _lists.find(getKey(protocol, encryption));

  if (i != _lists.end()) {
    for (ListType::const_iterator i2 = i->second.begin(); i2 != i->second.end(); ++i2) {
      result.insert(*i2);
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an endpoint for a specific protocol
////////////////////////////////////////////////////////////////////////////////

bool EndpointList::addEndpoint (const Endpoint::ProtocolType protocol,
                                const Endpoint::EncryptionType encryption, Endpoint* endpoint) {

  _lists[getKey(protocol, encryption)].insert(endpoint);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
