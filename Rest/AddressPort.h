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

#ifndef TRIAGENS_FYN_REST_ADDRESS_PORT_H
#define TRIAGENS_FYN_REST_ADDRESS_PORT_H 1

#include <BasicsC/Common.h>

namespace triagens {
  namespace rest {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief address and port
    ////////////////////////////////////////////////////////////////////////////////

    struct AddressPort {
      AddressPort ();
      AddressPort (string const&);

      string address;
      unsigned int port;

      bool operator== (AddressPort const& that);
      bool split (string const& definition);
    };
  }
}

#endif
