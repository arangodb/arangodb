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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <memory>
#include "Cluster/ClusterTypes.h"

struct TRI_vocbase_t;

namespace arangodb {

template<typename T>
class ResultT;

namespace futures {
template<typename T>
class Future;
}

namespace velocypack {
class SharedSlice;
}

namespace replication2 {
struct LogIndex;
class LogId;

namespace replicated_state::document {
struct SnapshotParams;
}

/**
 * Abstraction used by the RestHandler to access the DocumentState.
 */
struct DocumentStateMethods {
  virtual ~DocumentStateMethods() = default;

  [[nodiscard]] static auto createInstance(TRI_vocbase_t& vocbase)
      -> std::shared_ptr<DocumentStateMethods>;

  [[nodiscard]] virtual auto processSnapshotRequest(
      LogId logId, replicated_state::document::SnapshotParams params) const
      -> ResultT<velocypack::SharedSlice> = 0;

  [[nodiscard]] virtual auto getAssociatedShardList(LogId logId) const
      -> std::vector<ShardID> = 0;
};
}  // namespace replication2
}  // namespace arangodb
