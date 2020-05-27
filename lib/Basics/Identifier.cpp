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
/// @author Jan Steemann
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include <ostream>
#include <string>

#include "Basics/Identifier.h"

namespace arangodb::basics {

Identifier::BaseType Identifier::id() const noexcept { return _id; }

Identifier::BaseType const* Identifier::data() const noexcept { return &_id; }

Identifier::operator bool() const noexcept { return _id != 0; }

bool Identifier::operator==(Identifier const& other) const {
  return _id == other._id;
}

bool Identifier::operator!=(Identifier const& other) const {
  return !(operator==(other));
}

bool Identifier::operator<(Identifier const& other) const {
  return _id < other._id;
}

bool Identifier::operator<=(Identifier const& other) const {
  return _id <= other._id;
}

bool Identifier::operator>(Identifier const& other) const {
  return _id > other._id;
}

bool Identifier::operator>=(Identifier const& other) const {
  return _id >= other._id;
}

}  // namespace arangodb::basics

std::ostream& operator<<(std::ostream& s, arangodb::basics::Identifier const& i) {
  return s << std::to_string(i.id());
}
