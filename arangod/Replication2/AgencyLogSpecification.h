////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
#include <optional>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "Replication2/ReplicatedLog/Common.h"

#include "Cluster/ClusterTypes.h"

namespace arangodb::replication2::agency {

struct from_velocypack_t {};
inline constexpr auto from_velocypack = from_velocypack_t{};

struct LogPlanTermSpecification {
  LogTerm term;

  struct Leader {
    ParticipantId serverId;
    RebootId rebootId;
  };
  std::optional<Leader> leader;

  struct Participant {};
  std::unordered_map<ParticipantId, Participant> participants;

  auto toVelocyPack(VPackBuilder&) const -> void;
  LogPlanTermSpecification(from_velocypack_t, VPackSlice);
};

struct LogPlanSpecification {
  LogId id;
  std::optional<LogPlanTermSpecification> term;

  auto toVelocyPack(VPackBuilder&) const -> void;
  LogPlanSpecification(from_velocypack_t, VPackSlice);
};

struct LogCurrentLocalState {
  LogTerm term;
  LogIndex spearhead;

  auto toVelocyPack(VPackBuilder&) const -> void;
  LogCurrentLocalState(from_velocypack_t, VPackSlice);
};

struct LogCurrent {
  std::unordered_map<ParticipantId, LogCurrentLocalState> localState;

  auto toVelocyPack(VPackBuilder&) const -> void;
  LogCurrent(from_velocypack_t, VPackSlice);
};

}
