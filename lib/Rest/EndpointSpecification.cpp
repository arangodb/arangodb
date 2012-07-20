////////////////////////////////////////////////////////////////////////////////
/// @brief connection endpoint specification
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

#include "EndpointSpecification.h"

#include <regex.h>

#include "Basics/StringUtils.h"

using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                             EndpointSpecification
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Rest
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief set default port number 
////////////////////////////////////////////////////////////////////////////////

const uint32_t EndpointSpecification::_defaultPort = 8529;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Rest
/// @{
////////////////////////////////////////////////////////////////////////////////
      
EndpointSpecification::EndpointSpecification (EndpointType type, const string& specification) :
  _type(type),
  _specification(specification) {
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
/// @brief create an endpoint object from a string value
////////////////////////////////////////////////////////////////////////////////

EndpointSpecification* EndpointSpecification::factory (const string& specification) {
  if (specification.size() < 7) {
    return 0;
  }

  string copy = specification;
  if (specification[specification.size() - 1] == '/') {
    // address ends with a slash => remove
    copy = copy.substr(0, copy.size() - 1);
  }

  EndpointType type = ENDPOINT_UNKNOWN;
  string proto = StringUtils::tolower(copy.substr(0, 7));
  int cut;
  if (StringUtils::isPrefix(proto, "tcp://")) {
    // tcp/ip
    type = ENDPOINT_TCP;
    cut = 4;
  }
  else if (StringUtils::isPrefix(proto, "unix://")) {
    // unix socket
    type = ENDPOINT_UNIX;
    cut = 5;
  }
  else {
    // invalid type
    return 0;
  }

  string remain = copy.substr(cut + 2, proto.size() - cut - 2);
  if (type == ENDPOINT_UNIX) {
    // unix socket
    return new EndpointSpecificationUnix(specification, remain);
  }

  
  if (type == ENDPOINT_TCP) {
    // tcp/ip
    string address;
    uint32_t port;
    size_t found;
    
    if (remain[0] == '[') {
      // ipv6
      found = remain.find("]:", 1);
      if (found != string::npos && found + 2 < remain.size()) {
        // hostname and port (e.g. [address]:port)
        address = remain.substr(1, found - 1);
        port = StringUtils::uint32(remain.substr(found + 2));
      
        return new EndpointSpecificationTcp(specification, address, port);
      }

      found = remain.find("]", 1);
      if (found != string::npos && found + 1 == remain.size()) {
        // hostname only (e.g. [address])
        address = remain.substr(1, found - 1);
        port = _defaultPort;
        
        return new EndpointSpecificationTcp(specification, address, port);
      }

      // invalid address specification
      return 0;
    }

    // ipv4
    found = remain.find(':');

    if (found != string::npos && found + 1 < remain.size()) {
      // hostname and port
      string host = remain.substr(0, found);
      port = StringUtils::uint32(remain.substr(found + 2));
        
      return new EndpointSpecificationTcp(specification, address, port);
    }

    // hostname only
    string host = remain;
    port = _defaultPort;

    return new EndpointSpecificationTcp(specification, address, port);
  }

  // all other cases are invalid

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two endpoints
////////////////////////////////////////////////////////////////////////////////

bool EndpointSpecification::operator== (EndpointSpecification const &that) const {
  return getSpecification() == that.getSpecification();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                         EndpointSpecificationUnix
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Rest
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a Unix socket endpoint
////////////////////////////////////////////////////////////////////////////////

EndpointSpecificationUnix::EndpointSpecificationUnix (string const& specification, string const& socket) :
    EndpointSpecification(ENDPOINT_UNIX, specification),
    _socket(socket) { 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a Unix socket endpoint
////////////////////////////////////////////////////////////////////////////////

EndpointSpecificationUnix::~EndpointSpecificationUnix () {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                          EndpointSpecificationTcp
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Rest
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a TCP socket endpoint
////////////////////////////////////////////////////////////////////////////////

EndpointSpecificationTcp::EndpointSpecificationTcp (string const& specification, string const& host, const uint32_t port) :
    EndpointSpecification(ENDPOINT_TCP, specification),
    _host(host),
    _port(port) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a TCP socket endpoint
////////////////////////////////////////////////////////////////////////////////

EndpointSpecificationTcp::~EndpointSpecificationTcp () {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
