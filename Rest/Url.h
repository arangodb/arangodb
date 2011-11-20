////////////////////////////////////////////////////////////////////////////////
/// @brief url class
///
/// @file
/// A class describing the various parts of an URL.
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
/// @author Achim Brandt
/// @author Copyright 2007-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_REST_URL_H
#define TRIAGENS_REST_URL_H 1

#include <Basics/Common.h>

namespace triagens {
  namespace rest {

    ////////////////////////////////////////////////////////////////////////////////
    /// @ingroup Utilities
    /// @brief url class
    ////////////////////////////////////////////////////////////////////////////////

    class Url {
      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief url class
        ////////////////////////////////////////////////////////////////////////////////

        explicit
        Url (string const& urlName);

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns the service part
        ////////////////////////////////////////////////////////////////////////////////

        string const& service () const {
          return serviceValue;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns the host part
        ////////////////////////////////////////////////////////////////////////////////

        string const& host () const {
          return hostValue;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns the port
        ////////////////////////////////////////////////////////////////////////////////

        uint16_t port () const {
          return portValue;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns the path part
        ////////////////////////////////////////////////////////////////////////////////

        string const& path () const {
          return pathValue;
        }

      private:
        string hostValue;
        uint16_t portValue;
        string serviceValue;
        string pathValue;
    };
  }
}

#endif
