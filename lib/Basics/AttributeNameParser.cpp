////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include <stddef.h>
#include <algorithm>
#include <memory>
#include <ostream>

#include "AttributeNameParser.h"

#include "Basics/Exceptions.h"
#include "Basics/debugging.h"
#include "Basics/fasthash.h"
#include "Basics/voc-errors.h"

#include <velocypack/StringRef.h>

using AttributeName = arangodb::basics::AttributeName;

arangodb::basics::AttributeName::AttributeName(arangodb::velocypack::StringRef const& name)
    : AttributeName(name, false) {}

arangodb::basics::AttributeName::AttributeName(arangodb::velocypack::StringRef const& name, bool expand)
    : name(name.toString()), shouldExpand(expand) {}

uint64_t arangodb::basics::AttributeName::hash(uint64_t seed) const {
  return fasthash64(name.data(), name.size(), seed) ^ (shouldExpand ? 0xec59a4d : 0x4040ec59a4d40);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two attribute name vectors
////////////////////////////////////////////////////////////////////////////////

bool arangodb::basics::AttributeName::isIdentical(std::vector<AttributeName> const& lhs,
                                                  std::vector<AttributeName> const& rhs,
                                                  bool ignoreExpansionInLast) {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  for (size_t i = 0; i < lhs.size(); ++i) {
    if (lhs[i].name != rhs[i].name) {
      return false;
    }
    if (lhs[i].shouldExpand != rhs[i].shouldExpand) {
      if (!ignoreExpansionInLast) {
        return false;
      }
      if (i != lhs.size() - 1) {
        return false;
      }
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two attribute name vectors and return true if their names
/// matches
////////////////////////////////////////////////////////////////////////////////

bool arangodb::basics::AttributeName::namesMatch(std::vector<AttributeName> const& lhs,
                                                 std::vector<AttributeName> const& rhs) {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  for (size_t i = 0; i < lhs.size(); ++i) {
    if (lhs[i].name != rhs[i].name) {
      return false;
    }
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two attribute name vectors
////////////////////////////////////////////////////////////////////////////////

bool arangodb::basics::AttributeName::isIdentical(
    std::vector<std::vector<AttributeName>> const& lhs,
    std::vector<std::vector<AttributeName>> const& rhs, bool ignoreExpansionInLast) {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  for (size_t i = 0; i < lhs.size(); ++i) {
    if (!isIdentical(lhs[i], rhs[i], ignoreExpansionInLast && (i == lhs.size() - 1))) {
      return false;
    }
  }

  return true;
}

void arangodb::basics::TRI_ParseAttributeString(std::string const& input,
                                                std::vector<AttributeName>& result,
                                                bool allowExpansion) {
  TRI_ParseAttributeString(arangodb::velocypack::StringRef(input), result, allowExpansion);
}

void arangodb::basics::TRI_ParseAttributeString(arangodb::velocypack::StringRef const& input,
                                                std::vector<AttributeName>& result,
                                                bool allowExpansion) {
  bool foundExpansion = false;
  size_t parsedUntil = 0;
  size_t const length = input.length();

  for (size_t pos = 0; pos < length; ++pos) {
    auto token = input[pos];
    if (token == '[') {
      if (!allowExpansion) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_BAD_PARAMETER,
            "cannot use [*] expansion for this type of index");
      }
      // We only allow attr[*] and attr[*].attr2 as valid patterns
      if (length - pos < 3 || input[pos + 1] != '*' || input[pos + 2] != ']' ||
          (length - pos > 3 && input[pos + 3] != '.')) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED,
                                       "can only use [*] for indexes");
      }
      if (foundExpansion) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                       "cannot use multiple [*] "
                                       "expansions for a single index "
                                       "field");
      }
      result.emplace_back(input.substr(parsedUntil, pos - parsedUntil), true);
      foundExpansion = true;
      pos += 4;
      parsedUntil = pos;
    } else if (token == '.') {
      result.emplace_back(input.substr(parsedUntil, pos - parsedUntil), false);
      ++pos;
      parsedUntil = pos;
    }
  }
  if (parsedUntil < length) {
    result.emplace_back(input.substr(parsedUntil), false);
  }
}

void arangodb::basics::TRI_AttributeNamesToString(std::vector<AttributeName> const& input,
                                                  std::string& result, bool excludeExpansion) {
  TRI_ASSERT(result.empty());

  bool isFirst = true;
  for (auto const& it : input) {
    if (!isFirst) {
      result += ".";
    }
    isFirst = false;
    result += it.name;
    if (!excludeExpansion && it.shouldExpand) {
      result += "[*]";
    }
  }
}

bool arangodb::basics::TRI_AttributeNamesHaveExpansion(std::vector<AttributeName> const& input) {
  return std::any_of(input.begin(), input.end(), [](AttributeName const& value) {
    return value.shouldExpand; 
  });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append the attribute name to an output stream
////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& stream, arangodb::basics::AttributeName const& name) {
  stream << name.name;
  if (name.shouldExpand) {
    stream << "[*]";
  }
  return stream;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append the attribute names to an output stream
////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& stream,
                         std::vector<arangodb::basics::AttributeName> const& attributes) {
  size_t const n = attributes.size();
  for (size_t i = 0; i < n; ++i) {
    if (i > 0) {
      stream << ".";
    }
    stream << attributes[i];
  }
  return stream;
}
