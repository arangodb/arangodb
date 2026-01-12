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
/// @author Andrey Abramov
/// @author Vasily Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace arangodb::iresearch {

struct IResearchOptions {
  // whether or not to fail queries on links/indexes that are marked as
  // out of sync
  bool failQueriesOnOutOfSync = false;

  // names/ids of links/indexes to *NOT* recover. all entries should
  // be in format "collection-name/index-name" or "collection/index-id".
  // the pseudo-entry "all" skips recovering data for all links/indexes
  // found during recovery.
  std::vector<std::string> skipRecoveryItems;
  uint32_t deprecatedOptions = 0;
  uint32_t consolidationThreads = 0;
  uint32_t commitThreads = 0;
  uint32_t threads = 0;
  uint32_t threadsLimit = 0;
  uint32_t searchExecutionThreadsLimit = 0;
  uint32_t defaultParallelism = 1;

#ifdef USE_ENTERPRISE
  bool columnsCacheOnlyLeader = false;
#endif
};

}  // namespace arangodb::iresearch
