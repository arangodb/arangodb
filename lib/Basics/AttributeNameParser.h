////////////////////////////////////////////////////////////////////////////////
/// @brief Parser for Attibute Names. Tokenizes the attribute by dots and handles [*] expansion.
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014-2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_ATTRIBUTENAMEPARSER_H
#define ARANGODB_BASICS_ATTRIBUTENAMEPARSER_H 1

#include "Common.h"

namespace triagens {
  namespace basics {

// -----------------------------------------------------------------------------
// --SECTION--                                              struct AttributeName
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Struct that nows the name of the attribute, used to identify it's pid
///        but also knows if the attribute was followed by [*] which means
///        it should be expanded. Only works on arrays.
////////////////////////////////////////////////////////////////////////////////

    struct AttributeName {
      std::string const name;
      bool const shouldExpand;

      AttributeName (std::string const& pName)
        : name(pName), shouldExpand(false) {};

      AttributeName (std::string const& pName, bool pExpand)
        : name(pName), shouldExpand(pExpand) {};

    };

    void TRI_ParseAttributeString (
        std::string const& input,
        std::vector<AttributeName const>& result
      );
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
