////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_ATTRIBUTE_NAME_PARSER_H
#define ARANGODB_BASICS_ATTRIBUTE_NAME_PARSER_H 1

#include <iosfwd>
#include "Common.h"

namespace arangodb {

class StringRef;

namespace basics {

////////////////////////////////////////////////////////////////////////////////
/// @brief Struct that knows the name of the attribute used to identify its pid
///        but also knows if the attribute was followed by [*] which means
///        it should be expanded. Only works on arrays.
////////////////////////////////////////////////////////////////////////////////

struct AttributeName {
  std::string name;
  bool shouldExpand;

  explicit AttributeName(arangodb::StringRef const& name);

  AttributeName(std::string const& name, bool expand)
      : name(name), shouldExpand(expand) {}

  AttributeName(std::string&& name, bool expand)
      : name(std::move(name)), shouldExpand(expand) {}

  AttributeName(arangodb::StringRef const& name, bool expand);

  AttributeName(AttributeName const& other) = default;
  AttributeName& operator=(AttributeName const& other) = default;

  AttributeName(AttributeName&& other) = default;
  AttributeName& operator=(AttributeName&& other) = default;

  bool operator==(AttributeName const& other) const {
    return name == other.name && shouldExpand == other.shouldExpand;
  }

  bool operator!=(AttributeName const& other) const {
    return name != other.name || shouldExpand != other.shouldExpand;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief compare two attribute name vectors
  //////////////////////////////////////////////////////////////////////////////

  static bool isIdentical(std::vector<AttributeName> const&,
                          std::vector<AttributeName> const&, bool);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief compare two attribute name vectors
  //////////////////////////////////////////////////////////////////////////////

  static bool isIdentical(std::vector<std::vector<AttributeName>> const& lhs,
                          std::vector<std::vector<AttributeName>> const& rhs,
                          bool ignoreExpansionInLast);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief compare two attribute name vectors and return true if their names
  /// matches
  //////////////////////////////////////////////////////////////////////////////

  static bool namesMatch(std::vector<AttributeName> const&,
                         std::vector<AttributeName> const&);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Parse an input string into attribute names and expansion flags
////////////////////////////////////////////////////////////////////////////////

void TRI_ParseAttributeString(arangodb::StringRef const& input,
                              std::vector<AttributeName>& result, bool allowExpansion);

////////////////////////////////////////////////////////////////////////////////
/// @brief Parse an input string into attribute names and expansion flags
////////////////////////////////////////////////////////////////////////////////

void TRI_ParseAttributeString(std::string const& input,
                              std::vector<AttributeName>& result, bool allowExpansion);

////////////////////////////////////////////////////////////////////////////////
/// @brief Transform a vector of AttributeNames back into a string
////////////////////////////////////////////////////////////////////////////////

void TRI_AttributeNamesToString(std::vector<AttributeName> const& input,
                                std::string& result, bool excludeExpansion = false);

////////////////////////////////////////////////////////////////////////////////
/// @brief Transform a vector of AttributeNames into joined nested strings
///        The onlyFirst parameter is used to define if we only have to join the
///        first attribute
////////////////////////////////////////////////////////////////////////////////

void TRI_AttributeNamesJoinNested(std::vector<AttributeName> const& input,
                                  std::vector<std::string>& result, bool onlyFirst);

////////////////////////////////////////////////////////////////////////////////
/// @brief Tests if this AttributeName uses an expansion operator
////////////////////////////////////////////////////////////////////////////////

bool TRI_AttributeNamesHaveExpansion(std::vector<AttributeName> const& input);
}  // namespace basics
}  // namespace arangodb

std::ostream& operator<<(std::ostream&, arangodb::basics::AttributeName const&);
std::ostream& operator<<(std::ostream&, std::vector<arangodb::basics::AttributeName> const&);

#endif
