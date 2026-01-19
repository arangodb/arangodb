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

#pragma once

#include <cstddef>

namespace arangodb {

struct RocksDBIndexCacheRefillFeatureOptions {
  // maximum capacity of queue used for automatic refilling of in-memory index
  // caches after insert/update/replace (not used for initial filling at
  // startup)
  size_t maxCapacity = 128 * 1024;

  // maximum concurrent index fill tasks that we are allowed to run to fill
  // indexes during startup
  size_t maxConcurrentIndexFillTasks;

  // whether or not in-memory cache values for indexes are automatically
  // refilled upon insert/update/replace
  bool autoRefill = false;

  // whether or not in-memory cache values for indexes are automatically
  // populated on server start
  bool fillOnStartup = false;

  // whether or not in-memory cache values for indexes are automatically
  // refilled on followers
  bool autoRefillOnFollowers = true;

  RocksDBIndexCacheRefillFeatureOptions();
};

}  // namespace arangodb
