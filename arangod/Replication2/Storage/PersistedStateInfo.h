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

namespace arangodb::replication2::replicated_state {

struct PersistedStateInfo {
  LogId stateId;  // could be removed
  replicated_state::SnapshotInfo snapshot;
  replicated_state::StateGeneration generation;
  replication2::agency::ImplementationSpec specification;
};

template<class Inspector>
auto inspect(Inspector& f, PersistedStateInfo& x) {
  return f.object(x).fields(f.field("stateId", x.stateId),
                            f.field("snapshot", x.snapshot),
                            f.field("generation", x.generation),
                            f.field("specification", x.specification));
}

}  // namespace arangodb::replication2::replicated_state

namespace arangodb::replication2::storage {
// TODO - update usages, move type to namespace storage, and remove this alias
using PersistedStateInfo =
    ::arangodb::replication2::replicated_state::PersistedStateInfo;
}  // namespace arangodb::replication2::storage
