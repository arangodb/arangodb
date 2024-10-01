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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ShardingStrategy.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ServerState.h"

#include <velocypack/Builder.h>
#include <velocypack/Value.h>

using namespace arangodb;

bool ShardingStrategy::isCompatible(ShardingStrategy const* other) const {
  return name() == other->name();
}

void ShardingStrategy::toVelocyPack(velocypack::Builder& result) const {
  // only need to print sharding strategy if we are in a cluster
  if (ServerState::instance()->isRunningInCluster()) {
    result.add(StaticStrings::ShardingStrategy, velocypack::Value(name()));
  }
}
