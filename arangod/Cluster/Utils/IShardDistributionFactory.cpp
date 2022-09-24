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

#include "IShardDistributionFactory.h"
#include "Basics/debugging.h"

using namespace arangodb;

/**
 * @brief Get the List of server for the given ShardIndex
 * @param index The index of the shard (in alphabetical order)
 */
auto IShardDistributionFactory::getServersForShardIndex(size_t index) const
    -> ResponsibleServerList {
  TRI_ASSERT(!_shardToServerMapping.empty());
  TRI_ASSERT(_shardToServerMapping.size() > index);
  return _shardToServerMapping.at(index);
}
