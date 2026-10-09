////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <mutex>

namespace arangodb {

// RAII handle for a truncate operation's lock; used by both RocksDBIndex
// (RocksDBEngine/RocksDBIndex.h) and the engine-agnostic IResearchDataStore
// (IResearch/IResearchDataStore.h). Lives here, not in RocksDBIndex.h, so
// that the coordinator-side arangosearch link doesn't pull in RocksDB.
// TODO could be interface if it will be necessary
struct TruncateGuard {
  struct UnlockDeleter {
    void operator()(std::mutex* mutex) { mutex->unlock(); }
  };
  using Ptr = std::unique_ptr<std::mutex, UnlockDeleter>;
  Ptr mutex;
};

}  // namespace arangodb
