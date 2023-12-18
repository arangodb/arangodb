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

#include "Replication2/StateMachines/Document/DocumentStateSnapshot.h"
#include "Replication2/StateMachines/Document/ReplicatedOperationInspectors.h"

namespace arangodb::replication2::replicated_state::document {
inline constexpr auto kStringOperations = std::string_view{"operations"};
inline constexpr auto kStringSnapshotId = std::string_view{"snapshotId"};
inline constexpr auto kStringShards = std::string_view{"shards"};
inline constexpr auto kStringHasMore = std::string_view{"hasMore"};
inline constexpr auto kStringState = std::string_view{"state"};
inline constexpr auto kStringTotalDocsToBeSent =
    std::string_view{"totalDocsToBeSent"};
inline constexpr auto kStringDocsSent = std::string_view{"docsSent"};
inline constexpr auto kStringTotalBatches = std::string_view{"totalBatches"};
inline constexpr auto kStringTotalBytes = std::string_view{"totalBytes"};
inline constexpr auto kStringStartTime = std::string_view{"startTime"};
inline constexpr auto kStringLastUpdated = std::string_view{"lastUpdated"};
inline constexpr auto kStringLastBatchSent = std::string_view{"lastBatchSent"};
inline constexpr auto kStringSnapshots = std::string_view{"snapshots"};
inline constexpr auto kStringOngoing = std::string_view{"ongoing"};
inline constexpr auto kStringAborted = std::string_view{"aborted"};
inline constexpr auto kStringFinished = std::string_view{"finished"};

// The SnapshotId is serialized as a string because large 64-bit integers may
// not be represented correctly in JavaScript.
template<class Inspector>
auto inspect(Inspector& f, SnapshotId& x) {
  if constexpr (Inspector::isLoading) {
    auto v = std::string{};
    auto res = f.apply(v);
    if (res.ok()) {
      if (auto r = SnapshotId::fromString(v); r.fail()) {
        return inspection::Status{std::string{r.result().errorMessage()}};
      } else {
        x = r.get();
      }
    }
    return res;
  } else {
    return f.apply(to_string(x));
  }
}

template<class Inspector>
auto inspect(Inspector& f, SnapshotParams::Start& s) {
  return f.object(s).fields(f.field(StaticStrings::ServerId, s.serverId),
                            f.field(StaticStrings::RebootId, s.rebootId));
}

template<class Inspector>
auto inspect(Inspector& f, SnapshotBatch& s) {
  return f.object(s).fields(f.field(kStringSnapshotId, s.snapshotId),
                            f.field(kStringHasMore, s.hasMore),
                            f.field(kStringOperations, s.operations));
}

template<class Inspector>
auto inspect(Inspector& f, SnapshotStatistics::ShardStatistics& s) {
  return f.object(s).fields(f.field(kStringTotalDocsToBeSent, s.totalDocs),
                            f.field(kStringDocsSent, s.docsSent));
}

template<class Inspector>
auto inspect(Inspector& f, SnapshotStatistics& s) {
  return f.object(s).fields(
      f.field(kStringShards, s.shards),
      f.field(kStringTotalBatches, s.batchesSent),
      f.field(kStringTotalBytes, s.bytesSent),
      f.field(kStringStartTime, s.startTime)
          .transformWith(inspection::TimeStampTransformer{}),
      f.field(kStringLastUpdated, s.lastUpdated)
          .transformWith(inspection::TimeStampTransformer{}),
      f.field(kStringLastBatchSent, s.lastBatchSent)
          .transformWith(inspection::TimeStampTransformer{}));
}

template<class Inspector>
auto inspect(Inspector& f, SnapshotStatus& s) {
  return f.object(s).fields(f.field(kStringState, s.state),
                            f.embedFields(s.statistics));
}

struct SnapshotMapTransformer {
  using MemoryType = std::unordered_map<SnapshotId, SnapshotStatus>;
  using SerializedType = std::unordered_map<std::string, SnapshotStatus>;

  static arangodb::inspection::Status toSerialized(MemoryType const& v,
                                                   SerializedType& result) {
    for (auto const& [key, value] : v) {
      result.emplace(to_string(key), value);
    }

    return {};
  }
};

template<class Inspector>
auto inspect(Inspector& f, AllSnapshotsStatus& s) {
  return f.object(s).fields(f.field(kStringSnapshots, s.snapshots)
                                .transformWith(SnapshotMapTransformer{}));
}
}  // namespace arangodb::replication2::replicated_state::document
