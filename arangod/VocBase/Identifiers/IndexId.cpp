////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "VocBase/Identifiers/IndexId.h"

namespace arangodb {

/// @brief whether or not the id is set (not none())
bool IndexId::isSet() const noexcept {
  return id() != std::numeric_limits<BaseType>::max();
}

/// @brief whether or not the identifier is unset (equal to none())
bool IndexId::empty() const noexcept { return !isSet(); }

bool IndexId::isPrimary() const { return id() == 0; }

bool IndexId::isEdge() const { return id() == 1 || id() == 2; }

}  // namespace arangodb
