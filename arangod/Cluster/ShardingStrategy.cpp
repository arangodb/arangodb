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
////////////////////////////////////////////////////////////////////////////////

#include "ShardingStrategy.h"
#include "Basics/StaticStrings.h"

using namespace arangodb;

bool ShardingStrategy::usesDefaultShardKeys(std::vector<std::string> const& shardKeys) {
  if (shardKeys.size() != 1) {
    return false;
  }

  size_t const n = shardKeys[0].size();
  if (n == 0) {
    return false;
  }

  TRI_ASSERT(shardKeys.size() == 1); 

  return (shardKeys[0] == StaticStrings::KeyString ||
          (shardKeys[0][0] == ':' &&
           shardKeys[0].compare(1, n - 1,
                                StaticStrings::KeyString) == 0) ||
          (shardKeys[0].back() == ':' &&
           shardKeys[0].compare(0, n - 1,
                                StaticStrings::KeyString) == 0)
         );
}
