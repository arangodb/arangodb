////////////////////////////////////////////////////////////////////////////////
/// @brief Parser for attibute names.
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

#include "AttributeNameParser.h"
#include "Exceptions.h"

using AttributeName = triagens::basics::AttributeName;

namespace triagens {
  namespace basics {

////////////////////////////////////////////////////////////////////////////////
/// @brief append the index description to an output stream
////////////////////////////////////////////////////////////////////////////////
     
    std::ostream& operator<< (std::ostream& stream,
                              AttributeName const* name) {
      stream << name->name << " (" << (name->shouldExpand ? "true" : "false") << ")";
      return stream;
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief append the index description to an output stream
////////////////////////////////////////////////////////////////////////////////

    std::ostream& operator<< (std::ostream& stream,
                              AttributeName const& name) {
      stream << name.name << " (" << (name.shouldExpand ? "true" : "false") << ")";
      return stream;
    }

  } // namespace basics
} // namespace triagens

void triagens::basics::TRI_ParseAttributeString (std::string const& input,
                                                 std::vector<AttributeName>& result) {
  size_t parsedUntil = 0;
  size_t const length = input.length();

  for (size_t pos = 0; pos < length; ++pos) {
    auto token = input[pos];

    if (token == '[') {
      // We only allow attr[*] and attr[*].attr2 as valid patterns
      if (   length - pos < 3
          || input[pos + 1] != '*'
          || input[pos + 2] != ']'
          || (length - pos > 3 && input[pos + 3] != '.')) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_PARSER_FAILED);
      }
      result.emplace_back(input.substr(parsedUntil, pos - parsedUntil), true);
      pos += 4;
      parsedUntil = pos; 
    }
  }
  if (parsedUntil < length) {
    result.emplace_back(input.substr(parsedUntil), false);
  }
}

void triagens::basics::TRI_AttributeNamesToString (std::vector<AttributeName> const& input,
                                                   std::string& result,
                                                   bool excludeExpansion) {
  TRI_ASSERT(result.empty());

  bool isFirst = true;
  for (auto& it : input) {
    if (! isFirst) {
      result += ".";
    }
    isFirst = false;
    result += it.name;
    if (! excludeExpansion && it.shouldExpand) {
      result += "[*]";
    }
  }
}

bool triagens::basics::TRI_AttributeNamesHaveExpansion (std::vector<AttributeName> const& input) {
  for (auto& it : input) {
    if (it.shouldExpand) {
      return true;
    }
  }
  return false;
}
