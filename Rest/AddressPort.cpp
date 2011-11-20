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

#include <Basics/StringUtils.h>

using namespace triagens::basics;

namespace triagens {
  namespace rest {
    AddressPort::AddressPort ()
      : address("127.0.0.1"),
        port(0) {
    }



    AddressPort::AddressPort (string const& definition) {
      split(definition);
    }



    bool AddressPort::operator== (AddressPort const &that) {
      return address == that.address && port == that.port;
    }



    bool AddressPort::split (string const& definition) {
      size_t n = StringUtils::numEntries(definition, ":");

      if (n == 1) {
        address = "";
        port = StringUtils::uint32(definition);
        return true;
      }
      else if (n == 2) {
        address = StringUtils::entry(1, definition, ":");
        port = StringUtils::int32(StringUtils::entry(2, definition, ":"));
        return true;
      }
      else {
        return false;
      }
    }
  }
}
