////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "RebootId.h"
#include "Basics/debugging.h"
#include "Basics/StaticStrings.h"
#include "Basics/voc-errors.h"

#include <iostream>

namespace arangodb {

std::ostream& operator<<(std::ostream& o, arangodb::RebootId const& r) {
  return r.print(o);
}

std::ostream& RebootId::print(std::ostream& o) const {
  o << _value;
  return o;
}
}  // namespace arangodb
