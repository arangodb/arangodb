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

using namespace arangodb;
using namespace arangodb::maintenance;

/// @brief ctor
ActionDescription::ActionDescription(
  std::map<std::string, std::string> const& d, VPackBuilder const& p) :
  _description(d), _properties(p) {}

/// @brief Default dtor
ActionDescription::~ActionDescription() {}

/// @brief Does this description have a "p" parameter?
bool ActionDescription::has(std::string const& p) const noexcept {
  return _description.find(p) != _description.end();
}

/// @brief Does this description have a "p" parameter?
std::string ActionDescription::get(std::string const& p) const {
  return _description.at(p);
}

/// @brief Does this description have a "p" parameter?
void ActionDescription::set(std::string const& key, std::string const& value) {
  _description.emplace(key, value);
}

/// @brief Does this description have a "p" parameter?
void ActionDescription::set(std::pair<std::string, std::string> const& kvpair) {
  _description.emplace(kvpair);
}

/// @brief Hash function
std::size_t ActionDescription::hash() const {
  std::string propstr;
  for (auto const& i : _description) {
    propstr += i.first + i.second;
  }
  return std::hash<std::string>{}(propstr); 
}

/// @brief Equality operator
bool ActionDescription::operator==(
  ActionDescription const& other) const noexcept {
  return _description==other._description;
}

/// @brief Get action name
std::string ActionDescription::name() const {
  return _description.at("name");
}

/// @brief summary to velocypack
VPackBuilder ActionDescription::toVelocyPack() const {
  VPackBuilder b;
  { VPackObjectBuilder bb(&b);
    for (auto const& i : _description) {
      b.add(i.first, VPackValue(i.second));
    }
    if (!_properties.isEmpty()) {
      b.add("properties", _properties.slice());
    }}
  return b;
}


/// @brief hash implementation for ActionRegistry
namespace std {
std::size_t hash<ActionDescription>::operator()(
  ActionDescription const& a) const noexcept {
  return a.hash();
}}

