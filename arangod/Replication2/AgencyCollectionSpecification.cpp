////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "AgencyCollectionSpecification.h"

#include "Basics/VelocyPackHelper.h"

using namespace arangodb::replication2::agency;
using namespace arangodb::basics;

bool Collection::MutableProperties::operator==(
    MutableProperties other) const noexcept {
  if (schema.has_value() != other.schema.has_value()) {
    // One has a schema, the other doesn't
    return false;
  }
  if (schema.has_value()) {
    TRI_ASSERT(other.schema.has_value())
        << "We should have tested that either both or none have a schema";
    if (!VelocyPackHelper::equal(schema.value().slice(),
                                 other.schema.value().slice(), true)) {
      return false;
    }
  }
  return VelocyPackHelper::equal(computedValues.slice(),
                                 other.computedValues.slice(), true);
}