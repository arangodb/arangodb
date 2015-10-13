////////////////////////////////////////////////////////////////////////////////
/// @brief Parser for attibute names
/// Tokenizes the attribute by dots and handles [*] expansion.
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
/// @brief Struct that knows the name of the attribute used to identify its pid
///        but also knows if the attribute was followed by [*] which means
///        it should be expanded. Only works on arrays.
////////////////////////////////////////////////////////////////////////////////

    struct AttributeName {
      std::string name;
      bool shouldExpand;

      explicit AttributeName (std::string const& name)
        : AttributeName(name, false) {
      }

      AttributeName (std::string const& name, bool expand)
        : name(name), 
          shouldExpand(expand) { 
      }

      AttributeName (AttributeName const& other) 
        : name(other.name), 
          shouldExpand(other.shouldExpand) {
      }

      bool operator== (AttributeName const& other) const {
        return name == other.name && shouldExpand == other.shouldExpand;
      }

      bool operator!= (AttributeName const& other) const {
        return name != other.name || shouldExpand != other.shouldExpand;
      }

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Parse an input string into attribute names and expansion flags
////////////////////////////////////////////////////////////////////////////////

    void TRI_ParseAttributeString (std::string const& input,
                                   std::vector<AttributeName>& result);

////////////////////////////////////////////////////////////////////////////////
/// @brief Transform a vector of AttributeNames back into a string
////////////////////////////////////////////////////////////////////////////////

    void TRI_AttributeNamesToString (std::vector<AttributeName> const& input,
                                     std::string& result,
                                     bool excludeExpansion = false);

////////////////////////////////////////////////////////////////////////////////
/// @brief Transform a vector of AttributeNames into joined nested strings
///        The onlyFirst parameter is used to define if we only have to join the first attribute
////////////////////////////////////////////////////////////////////////////////

    void TRI_AttributeNamesJoinNested (std::vector<AttributeName> const& input,
                                       std::vector<std::string>& result,
                                       bool onlyFirst);

////////////////////////////////////////////////////////////////////////////////
/// @brief Tests if this AttributeName uses an expansion operator
////////////////////////////////////////////////////////////////////////////////

    bool TRI_AttributeNamesHaveExpansion (std::vector<AttributeName> const& input);

  }
}
    
std::ostream& operator<< (std::ostream&, triagens::basics::AttributeName const*);
std::ostream& operator<< (std::ostream&, triagens::basics::AttributeName const&);
std::ostream& operator<< (std::ostream&, std::vector<triagens::basics::AttributeName> const*);
std::ostream& operator<< (std::ostream&, std::vector<triagens::basics::AttributeName> const&);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
