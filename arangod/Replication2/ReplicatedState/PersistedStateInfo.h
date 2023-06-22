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
#pragma once
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedState/StateCommon.h"

namespace arangodb {
template<typename T>
class ResultT;
}
namespace arangodb::futures {
struct Unit;
template<typename T>
class Future;
}  // namespace arangodb::futures
namespace arangodb::replication2 {
struct PersistedLogIterator;
}

namespace arangodb::replication2::replicated_state {

struct PersistedStateInfo {
  LogId stateId;  // could be removed
  SnapshotInfo snapshot;
  StateGeneration generation;
  replication2::agency::ImplementationSpec specification;
};

template<class Inspector>
auto inspect(Inspector& f, PersistedStateInfo& x) {
  return f.object(x).fields(f.field("stateId", x.stateId),
                            f.field("snapshot", x.snapshot),
                            f.field("generation", x.generation),
                            f.field("specification", x.specification));
}

struct IStorageEngineMethods {
  virtual ~IStorageEngineMethods() = default;
  [[nodiscard]] virtual auto updateMetadata(PersistedStateInfo) -> Result = 0;
  [[nodiscard]] virtual auto readMetadata() -> ResultT<PersistedStateInfo> = 0;
  [[nodiscard]] virtual auto read(LogIndex first)
      -> std::unique_ptr<PersistedLogIterator> = 0;

  struct WriteOptions {
    bool waitForSync = false;
  };

  using SequenceNumber = std::uint64_t;

  virtual auto insert(std::unique_ptr<PersistedLogIterator>,
                      WriteOptions const&)
      -> futures::Future<ResultT<futures::Future<Result>>> = 0;
  virtual auto removeFront(LogIndex stop, WriteOptions const&)
      -> futures::Future<ResultT<futures::Future<Result>>> = 0;
  virtual auto removeBack(LogIndex start, WriteOptions const&)
      -> futures::Future<ResultT<futures::Future<Result>>> = 0;
  virtual auto getObjectId() -> std::uint64_t = 0;
  virtual auto getLogId() -> LogId = 0;

  virtual auto getSyncedSequenceNumber() -> SequenceNumber = 0;
  virtual auto waitForSync(SequenceNumber)
      -> futures::Future<futures::Unit> = 0;

  // waits for all ongoing requests to be done
  virtual void waitForCompletion() noexcept = 0;
};

}  // namespace arangodb::replication2::replicated_state
