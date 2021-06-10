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

#include <velocypack/Iterator.h>

#include "AgencyLogSpecification.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::agency;

template <>
struct std::tuple_size<velocypack::ObjectIteratorPair> {
  static constexpr auto value = 2;
};

template <std::size_t idx>
struct std::tuple_element<idx, velocypack::ObjectIteratorPair> {
  static_assert(0 <= idx && idx <= 1);
  using type = arangodb::velocypack::Slice;
};

namespace arangodb::velocypack {
template <std::size_t idx>
auto get(velocypack::ObjectIteratorPair pair) -> VPackSlice {
  static_assert(0 <= idx && idx <= 1);
  if constexpr (idx == 0) {
    return pair.key;
  } else if constexpr (idx == 1) {
    return pair.value;
  }
}
}
// namespace arangodb::velocypack

LogPlanConfig::LogPlanConfig(from_velocypack_t, VPackSlice slice) {
  waitForSync = slice.get("waitForSync").extract<bool>();
  writeConcern = slice.get("writeConcern").extract<std::size_t>();
}

auto LogPlanConfig::toVelocyPack(VPackBuilder& builder) const -> void {
  VPackObjectBuilder ob(&builder);
  builder.add("waitForSync", VPackValue(waitForSync));
  builder.add("writeConcern", VPackValue(writeConcern));
}

template<>
struct velocypack::Extractor<LogTerm> {
  static auto extract(VPackSlice slice) -> LogTerm {
    return LogTerm{slice.getNumericValue<std::size_t>()};
  }
};

template<>
struct velocypack::Extractor<LogIndex> {
  static auto extract(VPackSlice slice) -> LogIndex {
    return LogIndex{slice.getNumericValue<std::size_t>()};
  }
};

template<>
struct velocypack::Extractor<LogId> {
  static auto extract(VPackSlice slice) -> LogId {
    return LogId{slice.getNumericValue<std::size_t>()};
  }
};

template<>
struct velocypack::Extractor<RebootId> {
  static auto extract(VPackSlice slice) -> RebootId {
    return RebootId{slice.getNumericValue<std::size_t>()};
  }
};

auto LogPlanTermSpecification::toVelocyPack(VPackBuilder& builder) const -> void {
  VPackObjectBuilder ob(&builder);
  builder.add("term", VPackValue(term.value));
  {
    VPackObjectBuilder ob2(&builder, "participants");
    for (auto const& [p, l] : participants) {
      builder.add(p, VPackSlice::emptyObjectSlice());
    }
  }

  builder.add(VPackValue("config"));
  config.toVelocyPack(builder);

  if (leader.has_value()) {
    VPackObjectBuilder ob2(&builder, "leader");
    builder.add("serverId", VPackValue(leader->serverId));
    builder.add("rebootId", VPackValue(leader->rebootId.value()));
  }
}

LogPlanTermSpecification::LogPlanTermSpecification(from_velocypack_t, VPackSlice slice) {
  term = slice.get("term").extract<LogTerm>();
  config = LogPlanConfig(from_velocypack, slice.get("config"));
  for (auto const& [key, value] : VPackObjectIterator(slice.get("participants"))) {
    TRI_ASSERT(value.isEmptyObject());
    participants.emplace(ParticipantId{key.copyString()}, Participant{});
  }
  if (auto leaders = slice.get("leader"); !leaders.isNone()) {
    leader = Leader{leaders.get("serverId").copyString(), leaders.get("rebootId").extract<RebootId>()};
  }
}

auto LogPlanSpecification::toVelocyPack(VPackBuilder& builder) const -> void {
  VPackObjectBuilder ob(&builder);
  builder.add("id", VPackValue(id.id()));
  builder.add(VPackValue("targetConfig"));
  targetConfig.toVelocyPack(builder);
  if (currentTerm.has_value()) {
    builder.add(VPackValue("currentTerm"));
    currentTerm->toVelocyPack(builder);
  }
}

LogPlanSpecification::LogPlanSpecification(from_velocypack_t, VPackSlice slice) {
  id = slice.get("id").extract<LogId>();
  targetConfig = LogPlanConfig(from_velocypack, slice.get("targetConfig"));
  if (auto term = slice.get("currentTerm"); !term.isNone()) {
    currentTerm = LogPlanTermSpecification{from_velocypack, term};
  }
}

LogCurrentLocalState::LogCurrentLocalState(from_velocypack_t, VPackSlice slice) {
  auto spearheadSlice = slice.get("spearhead");
  spearhead.term = spearheadSlice.get("term").extract<LogTerm>();
  spearhead.index = spearheadSlice.get("index").extract<LogIndex>();
  term = slice.get("term").extract<LogTerm>();
}

auto LogCurrentLocalState::toVelocyPack(VPackBuilder& builder) const -> void {
  VPackObjectBuilder ob(&builder);
  builder.add("term", VPackValue(term.value));
  builder.add(VPackValue("spearhead"));
  spearhead.toVelocyPack(builder);
}

LogCurrent::LogCurrent(from_velocypack_t, VPackSlice slice) {
  for (auto const& [key, value] : VPackObjectIterator(slice.get("localStatus"))) {
    localState.emplace(ParticipantId{key.copyString()},
                       LogCurrentLocalState(from_velocypack, value));
  }
}

auto LogCurrent::toVelocyPack(VPackBuilder& builder) const -> void {
  VPackObjectBuilder ob(&builder);
  VPackObjectBuilder localStatusBuilder(&builder, "localStatus");
  for (auto const& [key, value] : localState) {
    builder.add(VPackValue(key));
    value.toVelocyPack(builder);
  }
}
