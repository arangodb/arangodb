////////////////////////////////////////////////////////////////////////////////
/// @brief address and port
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
/// @author Dr. Frank Celler
/// @author Copyright 2009-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "AddressPort.h"

#include "Basics/StringUtils.h"

using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Rest
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an empty address / port structure
////////////////////////////////////////////////////////////////////////////////

AddressPort::AddressPort ()
  : _address("127.0.0.1"),
    _port(0) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an IP4 or IP6 address / port structure
////////////////////////////////////////////////////////////////////////////////

AddressPort::AddressPort (string const& definition) {
  split(definition);
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

bool AddressPort::operator== (AddressPort const &that) {
  return _address == that._address && _port == that._port;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief splits an address
////////////////////////////////////////////////////////////////////////////////

bool AddressPort::split (string const& definition) {
  if (definition.empty()) {
    return false;
  }
      
  if (definition[0] == '[') {

    // ipv6 address
    size_t find = definition.find("]:", 1);
        
    if (find != string::npos && find + 2 < definition.size()) {
      _address = definition.substr(1, find-1);
      _port = StringUtils::uint32(definition.substr(find+2));

      return true;
    }
  }

  int n = StringUtils::numEntries(definition, ":");

  if (n == 1) {
    _address = "";
    _port = StringUtils::uint32(definition);

    return true;
  }
  else if (n == 2) {
    _address = StringUtils::entry(1, definition, ":");
    _port = StringUtils::int32(StringUtils::entry(2, definition, ":"));

    return true;
  }
  else {
    return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
