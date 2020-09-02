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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Aql/AttributeNamePath.h"
#include "Basics/StaticStrings.h"
#include "Basics/debugging.h"
#include "Basics/fasthash.h"

#include <algorithm>

namespace arangodb {
namespace aql {

AttributeNamePath::AttributeNamePath(std::string attribute) {
  path.emplace_back(std::move(attribute));
}
  
AttributeNamePath::AttributeNamePath(std::vector<std::string> p) 
    : path(std::move(p)) {}
  
bool AttributeNamePath::empty() const noexcept {
  return path.empty();
}

size_t AttributeNamePath::size() const noexcept {
  return path.size();
}

AttributeNamePath::Type AttributeNamePath::type() const noexcept {
  TRI_ASSERT(!empty());

  Type type = Type::SingleAttribute;
  if (size() == 1) {
    if (path[0] == StaticStrings::IdString) {
      type = Type::IdAttribute;
    } else if (path[0] == StaticStrings::KeyString) {
      type = Type::KeyAttribute;
    } else if (path[0] == StaticStrings::FromString) {
      type = Type::FromAttribute;
    } else if (path[0] == StaticStrings::ToString) {
      type = Type::ToAttribute;
    }
  } else {
    TRI_ASSERT(size() > 1);
    type = Type::MultiAttribute;
  }
  return type;
}

size_t AttributeNamePath::hash() const noexcept {
  // intentionally not use std::hash() here, because its results are platform-dependent.
  // however, we need portable hash values because we are testing for them in unit tests.
  uint64_t hash = 0x0404b00b1e5;
  for (auto const& it : path) {
    hash = fasthash64(it.data(), it.size(), hash);
  }
  return static_cast<size_t>(hash);
}

std::string const& AttributeNamePath::operator[](size_t index) const noexcept {
  TRI_ASSERT(index < path.size());
  return path[index];
}

bool AttributeNamePath::operator==(AttributeNamePath const& other) const noexcept {
  if (path.size() != other.path.size()) {
    return false;
  }
  for (size_t i = 0; i < path.size(); ++i) {
    if (path[i] != other.path[i]) {
      return false;
    }
  }
  return true;
}

bool AttributeNamePath::operator<(AttributeNamePath const& other) const noexcept {
  size_t const commonLength = std::min(size(), other.size());
  for (size_t i = 0; i < commonLength; ++i) {
    if (path[i] < other[i]) {
      return true;
    } else if (path[i] > other[i]) {
      return false;
    }
  }
  return (size() < other.size());
}

std::vector<std::string> const& AttributeNamePath::get() const noexcept { 
  return path; 
}

void AttributeNamePath::clear() noexcept {
  path.clear();
}

AttributeNamePath& AttributeNamePath::reverse() {
  std::reverse(path.begin(), path.end());
  return *this;
}
  
/// @brief shorten the attributes in the path to the specified length
AttributeNamePath& AttributeNamePath::shortenTo(size_t length) {
  if (length >= size()) {
    return *this;
  }
  path.erase(path.begin() + length, path.end());
  return *this;
}

/*static*/ size_t AttributeNamePath::commonPrefixLength(AttributeNamePath const& lhs,
                                                        AttributeNamePath const& rhs) {
  size_t numEqual = 0;
  size_t commonLength = std::min(lhs.size(), rhs.size());
  for (size_t i = 0; i < commonLength; ++i) {
    if (lhs[i] == rhs[i]) {
      ++numEqual;
    }
  }
  return numEqual;
}

} // namespace aql
} // namespace arangodb
