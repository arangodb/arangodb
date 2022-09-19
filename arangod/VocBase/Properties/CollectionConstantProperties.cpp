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

#include <velocypack/Builder.h>

using namespace arangodb;
VPackBuilder CollectionConstantProperties::defaultKeyOptions() {
  // NOTE: Only required until keyGenerator has it's own inspection
  VPackBuilder defaultEntry;
  {
    VPackObjectBuilder objectBuilder{&defaultEntry};
    defaultEntry.add("allowUserKeys", VPackValue(true));
    defaultEntry.add("type", VPackValue("traditional"));
  }
  return defaultEntry;
}

std::string CollectionConstantProperties::defaultShardingStrategy() {
  // NOTE: Only required until ShardingStrategy has it's own inspection
  // NOTE: We may want to add some context here
  return "hash";
}
