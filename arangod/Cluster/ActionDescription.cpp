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
/// @author Kaveh Vahedipour
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#include "ActionDescription.h"

#include <functional>

using namespace arangodb::maintenance;

/// @brief ctor
ActionDescription::ActionDescription(
  std::map<std::string, std::string> const& p) : _properties(p) {}

/// @brief Default dtor
ActionDescription::~ActionDescription() {}

/// @brief Hash function
std::size_t ActionDescription::hash() const {
  std::string propstr;
  for (auto const& i : _properties) {
    propstr += i.first + i.second;
  }
  return std::hash<std::string>{}(propstr); 
}

/// @brief Equality operator
bool ActionDescription::operator==(
  ActionDescription const& other) const noexcept {
  return _properties==other._properties;
}

/// @brief hash implementation for ActionRegistry
namespace std {
std::size_t hash<ActionDescription>::operator()(
  ActionDescription const& a) const noexcept {
  return a.hash();
}
}
