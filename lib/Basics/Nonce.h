////////////////////////////////////////////////////////////////////////////////
/// @brief nonces
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Frank Celler
/// @author Achim Brandt
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_NONCE_H
#define ARANGODB_BASICS_NONCE_H 1

#include "Basics/Common.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                            nonces
// -----------------------------------------------------------------------------

namespace triagens {
  namespace basics {
    namespace Nonce {

////////////////////////////////////////////////////////////////////////////////
/// @brief statistics collection
////////////////////////////////////////////////////////////////////////////////

      struct Statistics {
        uint32_t age;
        uint32_t checks;
        uint32_t isUnused;
        uint32_t isUsed;
        uint32_t marked;
        uint32_t falselyUsed;
        double fillingDegree;
      };

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a nonce hash of given size
////////////////////////////////////////////////////////////////////////////////

      void create (size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the nonce storage
////////////////////////////////////////////////////////////////////////////////

      void destroy ();

////////////////////////////////////////////////////////////////////////////////
/// @brief create a nonce
////////////////////////////////////////////////////////////////////////////////

      std::string createNonce ();

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a nonce is valid and invalidates it
////////////////////////////////////////////////////////////////////////////////

      bool checkAndMark (std::string const& nonce);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a nonce is valid and invalidates it
////////////////////////////////////////////////////////////////////////////////

      bool checkAndMark (uint32_t timestamp, uint64_t random);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the statistics
////////////////////////////////////////////////////////////////////////////////

      std::vector<Statistics> statistics ();
    }
  }
}
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
