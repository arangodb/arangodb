////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include "Inspection/VPack.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Utils/SingleCollectionTransaction.h"
#include "StorageEngine/ReplicationIterator.h"

#include <velocypack/SharedSlice.h>

namespace arangodb {
class LogicalCollection;
class PhysicalCollection;

namespace transaction {
class Context;
}
}  // namespace arangodb

namespace arangodb::replication2::replicated_state::document {
struct Snapshot {
  velocypack::SharedSlice documents;

  template<class Inspector>
  inline friend auto inspect(Inspector& f, Snapshot& s) {
    return f.object(s).fields(f.field("documents", s.documents));
  }
};

struct SnapshotOptions {
  enum Batch { kFirst, kNext };

  std::string clientId;
  Batch batch;
  LogIndex waitForIndex;
};

class SnapshotIterator {
 public:
  explicit SnapshotIterator(
      std::shared_ptr<LogicalCollection> logicalCollection);

  auto next() -> Snapshot;

 private:
  static const std::size_t kBatchSizeLimit = 1024 * 1024;  // 1MB

  std::shared_ptr<LogicalCollection> _logicalCollection;
  std::shared_ptr<transaction::Context> _ctx;
  SingleCollectionTransaction _trx;
  std::unique_ptr<ReplicationIterator> _it;
};

}  // namespace arangodb::replication2::replicated_state::document
