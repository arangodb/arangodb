////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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

#include "CollectionConstantProperties.h"

#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Inspection/Status.h"

#include <velocypack/Builder.h>

using namespace arangodb;

bool CollectionConstantProperties::operator==(
    CollectionConstantProperties const& other) const {
  if (type != other.type) {
    return false;
  }
  if (isSystem != other.isSystem) {
    return false;
  }
  if (smartJoinAttribute != other.smartJoinAttribute) {
    return false;
  }
  if (isSmart != other.isSmart) {
    return false;
  }
  if (isDisjoint != other.isDisjoint) {
    return false;
  }
  if (cacheEnabled != other.cacheEnabled) {
    return false;
  }
  if (smartGraphAttribute != other.smartGraphAttribute) {
    return false;
  }
  if (keyOptions != other.keyOptions) {
    return false;
  }
  return true;
}
