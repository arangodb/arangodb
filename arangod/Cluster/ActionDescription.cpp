////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
ActionDescription::ActionDescription(std::map<std::string, std::string> const& d,
                                     int priority,
                                     std::shared_ptr<VPackBuilder> const& p)
    : _description(d), _properties(p), _priority(priority) {
  TRI_ASSERT(d.find(NAME) != d.end());
  TRI_ASSERT(p == nullptr || p->isEmpty() || p->slice().isObject());
}

/// @brief Default dtor
ActionDescription::~ActionDescription() {}

/// @brief Does this description have a "p" parameter?
bool ActionDescription::has(std::string const& p) const {
  return _description.find(p) != _description.end();
}

/// @brief Does this description have a "p" parameter?
std::string ActionDescription::operator()(std::string const& p) const {
  return _description.at(p);
}

/// @brief Does this description have a "p" parameter?
std::string ActionDescription::get(std::string const& p) const {
  return _description.at(p);
}

/// @brief Get parameter
Result ActionDescription::get(std::string const& p, std::string& r) const {
  Result result;
  auto const& it = _description.find(p);
  if (it == _description.end()) {
    result.reset(TRI_ERROR_FAILED);
  } else {
    r = it->second;
  }
  return result;
}

/// @brief Hash function
std::size_t ActionDescription::hash() const {
  std::string propstr;
  for (auto const& i : _description) {
    propstr += i.first + i.second;
  }
  return std::hash<std::string>{}(propstr);
}

std::size_t ActionDescription::hash(std::map<std::string, std::string> const& desc) {
  std::string propstr;
  for (auto const& i : desc) {
    propstr += i.first + i.second;
  }
  return std::hash<std::string>{}(propstr);
}

/// @brief Equality operator
bool ActionDescription::operator==(ActionDescription const& other) const {
  return _description == other._description;
}

/// @brief Get action name. Cannot throw. See constructor
std::string const& ActionDescription::name() const {
  static const std::string EMPTY_STRING;
  auto const& it = _description.find(NAME);
  return (it != _description.end()) ? it->second : EMPTY_STRING;
}

/// @brief summary to velocypack
VPackBuilder ActionDescription::toVelocyPack() const {
  VPackBuilder b;
  {
    VPackObjectBuilder bb(&b);
    toVelocyPack(b);
  }
  return b;
}

/// @brief summary to velocypack
void ActionDescription::toVelocyPack(VPackBuilder& b) const {
  TRI_ASSERT(b.isOpenObject());
  for (auto const& i : _description) {
    b.add(i.first, VPackValue(i.second));
  }
  if (_properties != nullptr && !_properties->isEmpty()) {
    b.add("properties", _properties->slice());
  }
}

/// @brief summary to JSON string
std::string ActionDescription::toJson() const {
  return toVelocyPack().toJson();
}

/// @brief non discrimantory properties.
std::shared_ptr<VPackBuilder> const ActionDescription::properties() const {
  return _properties;
}

/// @brief hash implementation for ActionRegistry
namespace std {
std::size_t hash<ActionDescription>::operator()(ActionDescription const& a) const noexcept {
  return a.hash();
}

ostream& operator<<(ostream& out, arangodb::maintenance::ActionDescription const& d) {
  out << d.toJson() << " Priority: " << d.priority();
  return out;
}
}  // namespace std
