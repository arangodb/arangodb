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

#include "CollectionMutableProperties.h"

#include "Basics/VelocyPackHelper.h"
#include "Inspection/VPack.h"
#include <velocypack/Builder.h>

using namespace arangodb;

bool CollectionMutableProperties::operator==(
    CollectionMutableProperties const& other) const {
  if (name != other.name) {
    return false;
  }
  if (waitForSync != other.waitForSync) {
    return false;
  }
  if (replicationFactor != other.replicationFactor) {
    return false;
  }
  if (writeConcern != other.writeConcern) {
    return false;
  }
  if (!basics::VelocyPackHelper::equal(computedValues.slice(),
                                       other.computedValues.slice(), true)) {
    return false;
  }
  if (!basics::VelocyPackHelper::equal(schema.slice(), other.schema.slice(),
                                       true)) {
    return false;
  }
  return true;
}

auto CollectionMutableProperties::Transformers::ReplicationSatellite::
    toSerialized(MemoryType v, SerializedType& result)
        -> arangodb::inspection::Status {
  result.add(VPackValue(v));
  return {};
}

auto CollectionMutableProperties::Transformers::ReplicationSatellite::
    fromSerialized(SerializedType const& b, MemoryType& result)
        -> arangodb::inspection::Status {
  auto v = b.slice();
  if (v.isString() && v.isEqualString("satellite")) {
    result = 0;
    return {};
  } else if (v.isNumber()) {
    result = v.getNumber<MemoryType>();
    if (result != 0) {
      return {};
    }
  }
  return {"Only an integer number or 'satellite' is allowed"};
}
