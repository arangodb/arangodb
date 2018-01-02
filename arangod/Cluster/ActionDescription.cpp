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

ActionDescription::ActionDescription(
  Type const& t, std::map<std::string, std::string> const& p) :
  _type(t), _properties(p) {}

ActionDescription::~ActionDescription() {}

ActionDescription::operator== (ActionDescription const& other) {
  return _type==other._type && _properties==other._properties;
}

namespace std {
std::size_t hash<ActionDescription>::operator()(
  ActionDescription const& a) const noexcept {
  std::size_t const h1 (a._type);
  std::string h2;
  for (auto const& i : a._properties) {
    h2 += i.first + i.second;
  }
  return h1 ^ (std::hash<std::string>{}(h2) << 1); 
}}
