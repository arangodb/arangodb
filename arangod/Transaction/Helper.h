////////////////////////////////////////////////////////////////////////////////
/// @brief static transaction helper functions
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_TRANSACTION_HELPER_H
#define ARANGODB_TRANSACTION_HELPER_H 1

#include "Basics/Common.h"
#include "Basics/BsonHelper.h"

namespace triagens {
  namespace transaction {

    class Helper {
      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief append a document key
////////////////////////////////////////////////////////////////////////////////

        static bool appendKey (basics::Bson&,
                               std::string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief extract a document key
/// this will throw if the document key is invalid
/// if the document does not contain the _key attribute, an empty string will
/// be returned
////////////////////////////////////////////////////////////////////////////////

        static std::string documentKey (basics::Bson const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a BSON document from the string
////////////////////////////////////////////////////////////////////////////////

        static basics::Bson documentFromJson (std::string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a BSON document from the string
////////////////////////////////////////////////////////////////////////////////

        static basics::Bson documentFromJson (char const*,
                                              size_t);

    };
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
